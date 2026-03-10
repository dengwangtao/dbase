#pragma once

#include <string>
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
    Unknown
};

class Error
{
    public:
        Error() = default;

        Error(ErrorCode code, std::string message)
            : m_code(code), m_message(std::move(message))
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

        explicit operator bool() const noexcept
        {
            return ok();
        }

    private:
        ErrorCode m_code{ErrorCode::Ok};
        std::string m_message;
};

template <typename T>
class Result
{
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
            return std::get<T>(m_storage);
        }

        [[nodiscard]] const T& value() const&
        {
            return std::get<T>(m_storage);
        }

        [[nodiscard]] T&& value() &&
        {
            return std::get<T>(std::move(m_storage));
        }

        [[nodiscard]] Error& error() &
        {
            return std::get<Error>(m_storage);
        }

        [[nodiscard]] const Error& error() const&
        {
            return std::get<Error>(m_storage);
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

        [[nodiscard]] const Error& error() const noexcept
        {
            return m_error;
        }

    private:
        Error m_error;
};

[[nodiscard]] const char* toString(ErrorCode code) noexcept;

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

}  // namespace dbase