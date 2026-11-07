#include "mist/common/Error.h"
#include <sstream>

namespace mist::common {

std::string Error::format() const {
    if (isSuccess()) {
        return "Success";
    }

    std::ostringstream oss;
    oss << "[" << subsystemToString(subsystem_) << "] "
        << errorCodeToString(code_) << " (" << static_cast<uint32_t>(code_) << ")";

    if (!message_.empty()) {
        oss << ": " << message_;
    }

    if (platformCode_ != 0) {
        oss << " [PlatformCode=" << platformCode_ << "]";
    }

    return oss.str();
}

} // namespace mist::common
