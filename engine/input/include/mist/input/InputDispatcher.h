#pragma once

#include "mist/common/Result.h"
#include "mist/input/IInputManager.h"
#include "mist/input/InputState.h"
#include "mist/input/KeyboardInput.h"
#include "mist/input/MouseInput.h"
#include "mist/input/GamepadInput.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>

namespace mist::input {

struct QueuedInputItem {
    InputPacketHeader header;
    InputEvent event;
    uint64_t enqueueTimeNs{0};
};

class InputDispatcher : public IInputManager {
public:
    explicit InputDispatcher(
        std::shared_ptr<IKeyboardInjector> keyboard = nullptr,
        std::shared_ptr<IMouseInjector> mouse = nullptr,
        std::shared_ptr<IVirtualGamepad> gamepad = nullptr
    ) noexcept;

    ~InputDispatcher() override;

    [[nodiscard]] common::Result<void> initialize() override;
    [[nodiscard]] common::Result<void> injectEvent(const InputEvent& event) override;
    [[nodiscard]] common::Result<void> reset() override;
    [[nodiscard]] common::Result<void> shutdown() override;

    // Enqueue from network receiver
    [[nodiscard]] common::Result<void> enqueuePacket(
        const InputPacketHeader& header,
        const InputEvent& event
    );

    [[nodiscard]] const InputState& getState() const noexcept { return state_; }
    [[nodiscard]] size_t getQueueDepth() const;
    [[nodiscard]] uint64_t getEventsProcessed() const noexcept { return eventsProcessed_.load(); }
    [[nodiscard]] uint64_t getEventsDropped() const noexcept { return eventsDropped_.load(); }

private:
    void dispatchWorkerLoop();
    void processItem(const QueuedInputItem& item);

    std::shared_ptr<IKeyboardInjector> keyboard_;
    std::shared_ptr<IMouseInjector> mouse_;
    std::shared_ptr<IVirtualGamepad> gamepad_;
    InputState state_;

    static constexpr size_t MAX_QUEUE_DEPTH = 64;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::queue<QueuedInputItem> eventQueue_;

    // Coalesced continuous state slots
    std::optional<QueuedInputItem> pendingMouseMove_;
    std::optional<QueuedInputItem> pendingGamepadState_;

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};
    std::thread workerThread_;

    std::atomic<uint64_t> eventsProcessed_{0};
    std::atomic<uint64_t> eventsDropped_{0};
};

} // namespace mist::input