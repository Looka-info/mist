#include "mist/input/InputDispatcher.h"
#include "mist/telemetry/Metrics.h"
#include <chrono>

namespace mist::input {

namespace {

uint64_t getNowNs() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace

InputDispatcher::InputDispatcher(
    std::shared_ptr<IKeyboardInjector> keyboard,
    std::shared_ptr<IMouseInjector> mouse,
    std::shared_ptr<IVirtualGamepad> gamepad
) noexcept
    : keyboard_(std::move(keyboard)),
      mouse_(std::move(mouse)),
      gamepad_(std::move(gamepad)) {
    if (!keyboard_) {
        keyboard_ = std::make_shared<WindowsKeyboardInjector>();
    }
    if (!mouse_) {
        mouse_ = std::make_shared<WindowsMouseInjector>();
    }
    if (!gamepad_) {
        gamepad_ = std::make_shared<HostVirtualGamepad>();
    }
}

InputDispatcher::~InputDispatcher() {
    (void)shutdown();
}

common::Result<void> InputDispatcher::initialize() {
    if (isInitialized_.load()) {
        return common::Result<void>::success();
    }

    if (gamepad_) {
        auto res = gamepad_->initialize();
        if (!res) {
            // Non-fatal fallback for virtual gamepad
        }
    }

    isRunning_.store(true);
    workerThread_ = std::thread(&InputDispatcher::dispatchWorkerLoop, this);
    isInitialized_.store(true);
    return common::Result<void>::success();
}

common::Result<void> InputDispatcher::shutdown() {
    if (!isInitialized_.load()) {
        return common::Result<void>::success();
    }

    isRunning_.store(false);
    queueCv_.notify_all();

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    (void)reset();
    isInitialized_.store(false);
    return common::Result<void>::success();
}

common::Result<void> InputDispatcher::reset() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!eventQueue_.empty()) {
            eventQueue_.pop();
        }
        pendingMouseMove_.reset();
        pendingGamepadState_.reset();
    }

    if (keyboard_) {
        keyboard_->releaseAllKeys();
    }
    if (mouse_) {
        mouse_->releaseAllButtons();
    }
    if (gamepad_) {
        gamepad_->reset();
    }
    state_.reset();
    return common::Result<void>::success();
}

common::Result<void> InputDispatcher::injectEvent(const InputEvent& event) {
    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.timestampNs = getNowNs();
    return enqueuePacket(hdr, event);
}

common::Result<void> InputDispatcher::enqueuePacket(
    const InputPacketHeader& header,
    const InputEvent& event
) {
    QueuedInputItem item{header, event, getNowNs()};

    std::unique_lock<std::mutex> lock(queueMutex_);

    // Check if this is a continuous movement or analog state that should be coalesced
    if (std::holds_alternative<MouseMoveEvent>(event)) {
        const auto& moveEv = std::get<MouseMoveEvent>(event);
        if (!moveEv.isAbsolute && pendingMouseMove_.has_value()) {
            // Accumulate relative deltas to preserve motion distance
            auto& existing = std::get<MouseMoveEvent>(pendingMouseMove_->event);
            existing.deltaX += moveEv.deltaX;
            existing.deltaY += moveEv.deltaY;
            existing.isAbsolute = false;
        } else {
            // Overwrite with freshest position/delta
            pendingMouseMove_ = item;
        }
        lock.unlock();
        queueCv_.notify_one();
        return common::Result<void>::success();
    }

    if (std::holds_alternative<GamepadState>(event)) {
        // Coalesce latest analog stick and trigger state
        pendingGamepadState_ = item;
        lock.unlock();
        queueCv_.notify_one();
        return common::Result<void>::success();
    }

    // Discrete transition event (key press/release, mouse click, permission message)
    if (eventQueue_.size() >= MAX_QUEUE_DEPTH) {
        // Drop oldest to preserve freshness and bound memory
        eventQueue_.pop();
        eventsDropped_.fetch_add(1, std::memory_order_relaxed);
        telemetry::MetricRegistry::instance().getOrCreateCounter("input.events_dropped")->increment();
    }

    eventQueue_.push(item);
    lock.unlock();
    queueCv_.notify_one();
    return common::Result<void>::success();
}

size_t InputDispatcher::getQueueDepth() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return eventQueue_.size() + (pendingMouseMove_.has_value() ? 1 : 0) + (pendingGamepadState_.has_value() ? 1 : 0);
}

void InputDispatcher::dispatchWorkerLoop() {
    while (isRunning_.load()) {
        QueuedInputItem itemToProcess{};
        bool hasItem = false;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this]() {
                return !isRunning_.load() || !eventQueue_.empty() || pendingMouseMove_.has_value() || pendingGamepadState_.has_value();
            });

            if (!isRunning_.load()) {
                break;
            }

            // Priority: Discrete events first (key/button transitions), then coalesced movement
            if (!eventQueue_.empty()) {
                itemToProcess = eventQueue_.front();
                eventQueue_.pop();
                hasItem = true;
            } else if (pendingMouseMove_.has_value()) {
                itemToProcess = *pendingMouseMove_;
                pendingMouseMove_.reset();
                hasItem = true;
            } else if (pendingGamepadState_.has_value()) {
                itemToProcess = *pendingGamepadState_;
                pendingGamepadState_.reset();
                hasItem = true;
            }
        }

        if (hasItem) {
            processItem(itemToProcess);
            eventsProcessed_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void InputDispatcher::processItem(const QueuedInputItem& item) {
    uint64_t nowNs = getNowNs();
    double latencyUs = (item.header.timestampNs > 0 && nowNs > item.header.timestampNs)
                           ? static_cast<double>(nowNs - item.header.timestampNs) / 1000.0
                           : 0.0;

    std::visit([&](const auto& ev) {
        using T = std::decay_t<decltype(ev)>;
        if constexpr (std::is_same_v<T, KeyEvent>) {
            if (keyboard_) {
                (void)keyboard_->injectKey(ev);
            }
            if (ev.action == KeyAction::Press) {
                state_.pressKey(ev.scanCode);
            } else {
                state_.releaseKey(ev.scanCode);
            }
            telemetry::MetricRegistry::instance().getOrCreateCounter("input.keyboard_events")->increment();
            if (latencyUs > 0.0) {
                telemetry::MetricRegistry::instance().getOrCreateHistogram("input.keyboard_latency_us")->record(latencyUs);
            }
        } else if constexpr (std::is_same_v<T, MouseMoveEvent>) {
            if (mouse_) {
                (void)mouse_->injectMove(ev);
            }
            if (ev.isAbsolute) {
                state_.setMousePosition(ev.normalizedX, ev.normalizedY);
            }
            telemetry::MetricRegistry::instance().getOrCreateCounter("input.mouse_events")->increment();
            if (latencyUs > 0.0) {
                telemetry::MetricRegistry::instance().getOrCreateHistogram("input.mouse_latency_us")->record(latencyUs);
            }
        } else if constexpr (std::is_same_v<T, MouseButtonEvent>) {
            if (mouse_) {
                (void)mouse_->injectButton(ev);
            }
            state_.setMouseButton(ev.button, ev.isDown);
            telemetry::MetricRegistry::instance().getOrCreateCounter("input.mouse_events")->increment();
            if (latencyUs > 0.0) {
                telemetry::MetricRegistry::instance().getOrCreateHistogram("input.mouse_latency_us")->record(latencyUs);
            }
        } else if constexpr (std::is_same_v<T, MouseWheelEvent>) {
            if (mouse_) {
                (void)mouse_->injectWheel(ev);
            }
            telemetry::MetricRegistry::instance().getOrCreateCounter("input.mouse_events")->increment();
        } else if constexpr (std::is_same_v<T, GamepadState>) {
            if (gamepad_) {
                (void)gamepad_->updateState(ev);
            }
            state_.updateGamepad(ev);
            telemetry::MetricRegistry::instance().getOrCreateCounter("input.gamepad_events")->increment();
        } else if constexpr (std::is_same_v<T, PermissionMessage>) {
            if (ev.action == PermissionAction::Revoked) {
                (void)reset();
            }
        } else if constexpr (std::is_same_v<T, ClipboardEvent>) {
            telemetry::MetricRegistry::instance().getOrCreateCounter("input.clipboard_events")->increment();
        }
    }, item.event);
}

} // namespace mist::input