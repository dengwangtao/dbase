#include "dbase/error/error.h"

namespace dbase
{
const char* toString(ErrorCode code) noexcept
{
    switch (code)
    {
        case ErrorCode::Ok:
            return "Ok";
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::NotFound:
            return "NotFound";
        case ErrorCode::AlreadyExists:
            return "AlreadyExists";
        case ErrorCode::IOError:
            return "IOError";
        case ErrorCode::Timeout:
            return "Timeout";
        case ErrorCode::NotSupported:
            return "NotSupported";
        case ErrorCode::SystemError:
            return "SystemError";
        case ErrorCode::Unknown:
            return "Unknown";
        default:
            return "Unknown";
    }
}
}  // namespace dbase