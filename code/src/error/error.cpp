#include "dbase/error/error.h"

#include <cstring>
#include <string>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <cerrno>
#endif

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
        case ErrorCode::ParseError:
            return "ParseError";
        case ErrorCode::InvalidState:
            return "InvalidState";
        case ErrorCode::Cancelled:
            return "Cancelled";
        case ErrorCode::Unknown:
            return "Unknown";
        default:
            return "Unknown";
    }
}

std::string Error::toString() const
{
    if (m_message.empty())
    {
        return std::string(dbase::toString(m_code));
    }

    return std::string(dbase::toString(m_code)) + ": " + m_message;
}

int lastSystemErrorCode() noexcept
{
#if defined(_WIN32)
    return static_cast<int>(::GetLastError());
#else
    return errno;
#endif
}

std::string systemErrorMessage(int code)
{
#if defined(_WIN32)
    char* buffer = nullptr;
    const DWORD size = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(code),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buffer),
            0,
            nullptr);

    std::string message;
    if (size != 0 && buffer != nullptr)
    {
        message.assign(buffer, size);
        ::LocalFree(buffer);

        while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' '))
        {
            message.pop_back();
        }
    }
    else
    {
        message = "unknown system error";
    }

    return message;
#else
    return std::strerror(code);
#endif
}

Error makeSystemError(std::string message, int code)
{
    if (!message.empty())
    {
        message += ": ";
    }

    message += systemErrorMessage(code);
    message += " (code=" + std::to_string(code) + ")";
    return Error(ErrorCode::SystemError, std::move(message));
}

}  // namespace dbase