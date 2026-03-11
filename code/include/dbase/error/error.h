#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace dbase
{
enum class ErrorCode
{
    Ok = 0,
    InvalidArgument,
    NotFound,
    AlreadyExists,
    IOError,
    Timeout,
    NotSupported,
    SystemError,
    ParseError,
    InvalidState,
    Cancelled,
    Unknown
};

class Error
{
    public:
        Error() = default;

        Error(ErrorCode code, std::string message)
            : m_code(code),
              m_message(std::move(message))
        {
        }

        [[nodiscard]] ErrorCode code() const noexcept
        {
            return m_code;
        }

        [[nodiscard]] const std::string& message() const noexcept
        {
            return m_message;
        }

        [[nodiscard]] bool ok() const noexcept
        {
            return m_code == ErrorCode::Ok;
        }

        [[nodiscard]] std::string toString() const;

        explicit operator bool() const noexcept
        {
            return ok();
        }

    private:
        ErrorCode m_code{ErrorCode::Ok};
        std::string m_message;
};

class BadResultAccess : public std::logic_error
{
    public:
        explicit BadResultAccess(std::string message)
            : std::logic_error(std::move(message))
        {
        }
};

[[nodiscard]] const char* toString(ErrorCode code) noexcept;

[[nodiscard]] int lastSystemErrorCode() noexcept;
[[nodiscard]] std::string systemErrorMessage(int code);
[[nodiscard]] Error makeSystemError(std::string message, int code = lastSystemErrorCode());

template <typename T>
class Result
{
        static_assert(!std::is_void_v<T>, "Result<void> must use the specialization");
        static_assert(!std::is_reference_v<T>, "Result<T> does not support reference types");

    public:
        using value_type = T;

        Result(const T& value)
            : m_storage(value)
        {
        }

        Result(T&& value)
            : m_storage(std::move(value))
        {
        }

        Result(const Error& error)
            : m_storage(error)
        {
        }

        Result(Error&& error)
            : m_storage(std::move(error))
        {
        }

        [[nodiscard]] bool hasValue() const noexcept
        {
            return std::holds_alternative<T>(m_storage);
        }

        [[nodiscard]] bool hasError() const noexcept
        {
            return std::holds_alternative<Error>(m_storage);
        }

        explicit operator bool() const noexcept
        {
            return hasValue();
        }

        [[nodiscard]] T& value() &
        {
            if (!hasValue())
            {
                throw BadResultAccess("Result::value() called on error result: " + error().toString());
            }
            return std::get<T>(m_storage);
        }

        [[nodiscard]] const T& value() const&
        {
            if (!hasValue())
            {
                throw BadResultAccess("Result::value() called on error result: " + error().toString());
            }
            return std::get<T>(m_storage);
        }

        [[nodiscard]] T&& value() &&
        {
            if (!hasValue())
            {
                throw BadResultAccess("Result::value() called on error result: " + error().toString());
            }
            return std::get<T>(std::move(m_storage));
        }

        [[nodiscard]] Error& error() &
        {
            if (!hasError())
            {
                throw BadResultAccess("Result::error() called on value result");
            }
            return std::get<Error>(m_storage);
        }

        [[nodiscard]] const Error& error() const&
        {
            if (!hasError())
            {
                throw BadResultAccess("Result::error() called on value result");
            }
            return std::get<Error>(m_storage);
        }

        [[nodiscard]] T valueOr(T defaultValue) const&
        {
            if (hasValue())
            {
                return std::get<T>(m_storage);
            }
            return defaultValue;
        }

        [[nodiscard]] T valueOr(T defaultValue) &&
        {
            if (hasValue())
            {
                return std::get<T>(std::move(m_storage));
            }
            return defaultValue;
        }

        [[nodiscard]] T& operator*() &
        {
            return value();
        }

        [[nodiscard]] const T& operator*() const&
        {
            return value();
        }

        [[nodiscard]] T* operator->()
        {
            return &value();
        }

        [[nodiscard]] const T* operator->() const
        {
            return &value();
        }

    private:
        std::variant<T, Error> m_storage;
};

template <>
class Result<void>
{
    public:
        Result() = default;

        Result(const Error& error)
            : m_error(error)
        {
        }

        Result(Error&& error)
            : m_error(std::move(error))
        {
        }

        [[nodiscard]] bool hasValue() const noexcept
        {
            return m_error.ok();
        }

        [[nodiscard]] bool hasError() const noexcept
        {
            return !m_error.ok();
        }

        explicit operator bool() const noexcept
        {
            return hasValue();
        }

        void value() const
        {
            if (hasError())
            {
                throw BadResultAccess("Result<void>::value() called on error result: " + m_error.toString());
            }
        }

        [[nodiscard]] Error& error() &
        {
            if (!hasError())
            {
                throw BadResultAccess("Result<void>::error() called on value result");
            }
            return m_error;
        }

        [[nodiscard]] const Error& error() const&
        {
            if (!hasError())
            {
                throw BadResultAccess("Result<void>::error() called on value result");
            }
            return m_error;
        }

    private:
        Error m_error;
};

template <typename T>
[[nodiscard]] inline Result<std::remove_cvref_t<T>> makeResult(T&& value)
{
    return Result<std::remove_cvref_t<T>>(std::forward<T>(value));
}

[[nodiscard]] inline Result<void> makeError(ErrorCode code, std::string message)
{
    return Result<void>(Error(code, std::move(message)));
}

template <typename T>
[[nodiscard]] inline Result<T> makeErrorResult(ErrorCode code, std::string message)
{
    return Result<T>(Error(code, std::move(message)));
}

[[nodiscard]] inline Result<void> makeSystemErrorResult(std::string message, int code = lastSystemErrorCode())
{
    return Result<void>(makeSystemError(std::move(message), code));
}

template <typename T>
[[nodiscard]] inline Result<T> makeSystemErrorResultT(std::string message, int code = lastSystemErrorCode())
{
    return Result<T>(makeSystemError(std::move(message), code));
}

}  // namespace dbase