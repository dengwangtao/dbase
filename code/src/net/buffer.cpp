#include "dbase/net/buffer.h"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <cerrno>
#include <sys/uio.h>
#include <unistd.h>
#endif

namespace dbase::net
{
namespace
{
template <typename T>
[[nodiscard]] T byteswapValue(T value) noexcept;

template <>
[[nodiscard]] std::uint16_t byteswapValue(std::uint16_t value) noexcept
{
    return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

// clang-format off
template <>
[[nodiscard]] std::uint32_t byteswapValue(std::uint32_t value) noexcept
{
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) |
           ((value & 0xFF000000u) >> 24);
}

template <>
[[nodiscard]] std::uint64_t byteswapValue(std::uint64_t value) noexcept
{
    return ((value & 0x00000000000000FFull) << 56) |
           ((value & 0x000000000000FF00ull) << 40) |
           ((value & 0x0000000000FF0000ull) << 24) |
           ((value & 0x00000000FF000000ull) << 8) |
           ((value & 0x000000FF00000000ull) >> 8) |
           ((value & 0x0000FF0000000000ull) >> 24) |
           ((value & 0x00FF000000000000ull) >> 40) |
           ((value & 0xFF00000000000000ull) >> 56);
}
// clang-format on

template <typename T>
[[nodiscard]] T hostToBigEndian(T value) noexcept
{
    if constexpr (sizeof(T) == 1)
    {
        return value;
    }
    else
    {
        using U = std::make_unsigned_t<T>;
        U raw = static_cast<U>(value);
        if constexpr (std::endian::native == std::endian::little)
        {
            raw = byteswapValue(raw);
        }
        return static_cast<T>(raw);
    }
}

template <typename T>
[[nodiscard]] T bigEndianToHost(T value) noexcept
{
    if constexpr (sizeof(T) == 1)
    {
        return value;
    }
    else
    {
        using U = std::make_unsigned_t<T>;
        U raw = static_cast<U>(value);
        if constexpr (std::endian::native == std::endian::little)
        {
            raw = byteswapValue(raw);
        }
        return static_cast<T>(raw);
    }
}

template <typename T>
[[nodiscard]] T readInteger(const char* data)
{
    T value{};
    std::memcpy(&value, data, sizeof(T));
    return bigEndianToHost(value);
}

template <typename T>
void writeInteger(char* data, T value)
{
    const T be = hostToBigEndian(value);
    std::memcpy(data, &be, sizeof(T));
}

[[nodiscard]] bool isWouldBlockError(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAEWOULDBLOCK;
#else
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}
}  // namespace

Buffer::Buffer(std::size_t initialSize)
    : m_buffer(kCheapPrepend + initialSize),
      m_readerIndex(kCheapPrepend),
      m_writerIndex(kCheapPrepend)
{
}

std::size_t Buffer::readableBytes() const noexcept
{
    return m_writerIndex - m_readerIndex;
}

std::size_t Buffer::writableBytes() const noexcept
{
    return m_buffer.size() - m_writerIndex;
}

std::size_t Buffer::prependableBytes() const noexcept
{
    return m_readerIndex;
}

std::size_t Buffer::capacity() const noexcept
{
    return m_buffer.size();
}

const char* Buffer::peek() const noexcept
{
    return begin() + m_readerIndex;
}

char* Buffer::beginWrite() noexcept
{
    return begin() + m_writerIndex;
}

const char* Buffer::beginWrite() const noexcept
{
    return begin() + m_writerIndex;
}

const char* Buffer::findCRLF() const noexcept
{
    constexpr char kCRLF[] = "\r\n";
    const char* start = peek();
    const char* end = beginWrite();
    const char* pos = std::search(start, end, kCRLF, kCRLF + 2);
    return pos == end ? nullptr : pos;
}

const char* Buffer::findEOL() const noexcept
{
    const void* pos = std::memchr(peek(), '\n', readableBytes());
    return static_cast<const char*>(pos);
}

void Buffer::retrieve(std::size_t len) noexcept
{
    if (len < readableBytes())
    {
        m_readerIndex += len;
    }
    else
    {
        retrieveAll();
    }
}

void Buffer::retrieveUntil(const char* end) noexcept
{
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(static_cast<std::size_t>(end - peek()));
}

void Buffer::retrieveAll() noexcept
{
    m_readerIndex = kCheapPrepend;
    m_writerIndex = kCheapPrepend;
}

std::string Buffer::retrieveAsString(std::size_t len)
{
    if (len > readableBytes())
    {
        throw std::out_of_range("Buffer::retrieveAsString len out of range");
    }
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieveAllAsString()
{
    return retrieveAsString(readableBytes());
}

std::string_view Buffer::readableView() const noexcept
{
    return std::string_view(peek(), readableBytes());
}

std::span<const std::byte> Buffer::readableSpan() const noexcept
{
    return std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(peek()),
            readableBytes());
}

std::span<std::byte> Buffer::writableSpan() noexcept
{
    return std::span<std::byte>(
            reinterpret_cast<std::byte*>(beginWrite()),
            writableBytes());
}

void Buffer::ensureWritableBytes(std::size_t len)
{
    if (writableBytes() < len)
    {
        makeSpace(len);
    }
}

void Buffer::hasWritten(std::size_t len) noexcept
{
    assert(len <= writableBytes());
    m_writerIndex += len;
}

void Buffer::unwrite(std::size_t len) noexcept
{
    assert(len <= readableBytes());
    m_writerIndex -= len;
}

void Buffer::append(const void* data, std::size_t len)
{
    if (len == 0)
    {
        return;
    }
    ensureWritableBytes(len);
    std::memcpy(beginWrite(), data, len);
    hasWritten(len);
}

void Buffer::append(const char* data)
{
    if (data == nullptr)
    {
        return;
    }
    append(data, std::strlen(data));
}

void Buffer::append(std::string_view data)
{
    append(data.data(), data.size());
}

void Buffer::append(const std::string& data)
{
    append(data.data(), data.size());
}

void Buffer::append(std::span<const std::byte> data)
{
    append(data.data(), data.size());
}

void Buffer::append(const Buffer& other)
{
    append(other.peek(), other.readableBytes());
}

void Buffer::prepend(const void* data, std::size_t len)
{
    if (len == 0)
    {
        return;
    }
    if (len > prependableBytes())
    {
        throw std::out_of_range("Buffer::prepend not enough prependable bytes");
    }
    m_readerIndex -= len;
    std::memcpy(begin() + m_readerIndex, data, len);
}

void Buffer::prepend(std::span<const std::byte> data)
{
    prepend(data.data(), data.size());
}

std::int8_t Buffer::peekInt8() const
{
    if (readableBytes() < sizeof(std::int8_t))
    {
        throw std::out_of_range("Buffer::peekInt8 not enough readable bytes");
    }
    return *reinterpret_cast<const std::int8_t*>(peek());
}

std::int16_t Buffer::peekInt16() const
{
    if (readableBytes() < sizeof(std::int16_t))
    {
        throw std::out_of_range("Buffer::peekInt16 not enough readable bytes");
    }
    return readInteger<std::int16_t>(peek());
}

std::int32_t Buffer::peekInt32() const
{
    if (readableBytes() < sizeof(std::int32_t))
    {
        throw std::out_of_range("Buffer::peekInt32 not enough readable bytes");
    }
    return readInteger<std::int32_t>(peek());
}

std::int64_t Buffer::peekInt64() const
{
    if (readableBytes() < sizeof(std::int64_t))
    {
        throw std::out_of_range("Buffer::peekInt64 not enough readable bytes");
    }
    return readInteger<std::int64_t>(peek());
}

std::uint8_t Buffer::peekUInt8() const
{
    if (readableBytes() < sizeof(std::uint8_t))
    {
        throw std::out_of_range("Buffer::peekUInt8 not enough readable bytes");
    }
    return *reinterpret_cast<const std::uint8_t*>(peek());
}

std::uint16_t Buffer::peekUInt16() const
{
    if (readableBytes() < sizeof(std::uint16_t))
    {
        throw std::out_of_range("Buffer::peekUInt16 not enough readable bytes");
    }
    return readInteger<std::uint16_t>(peek());
}

std::uint32_t Buffer::peekUInt32() const
{
    if (readableBytes() < sizeof(std::uint32_t))
    {
        throw std::out_of_range("Buffer::peekUInt32 not enough readable bytes");
    }
    return readInteger<std::uint32_t>(peek());
}

std::uint64_t Buffer::peekUInt64() const
{
    if (readableBytes() < sizeof(std::uint64_t))
    {
        throw std::out_of_range("Buffer::peekUInt64 not enough readable bytes");
    }
    return readInteger<std::uint64_t>(peek());
}

std::int8_t Buffer::readInt8()
{
    const auto value = peekInt8();
    retrieve(sizeof(value));
    return value;
}

std::int16_t Buffer::readInt16()
{
    const auto value = peekInt16();
    retrieve(sizeof(value));
    return value;
}

std::int32_t Buffer::readInt32()
{
    const auto value = peekInt32();
    retrieve(sizeof(value));
    return value;
}

std::int64_t Buffer::readInt64()
{
    const auto value = peekInt64();
    retrieve(sizeof(value));
    return value;
}

std::uint8_t Buffer::readUInt8()
{
    const auto value = peekUInt8();
    retrieve(sizeof(value));
    return value;
}

std::uint16_t Buffer::readUInt16()
{
    const auto value = peekUInt16();
    retrieve(sizeof(value));
    return value;
}

std::uint32_t Buffer::readUInt32()
{
    const auto value = peekUInt32();
    retrieve(sizeof(value));
    return value;
}

std::uint64_t Buffer::readUInt64()
{
    const auto value = peekUInt64();
    retrieve(sizeof(value));
    return value;
}

void Buffer::appendInt8(std::int8_t value)
{
    append(&value, sizeof(value));
}

void Buffer::appendInt16(std::int16_t value)
{
    ensureWritableBytes(sizeof(value));
    writeInteger(beginWrite(), value);
    hasWritten(sizeof(value));
}

void Buffer::appendInt32(std::int32_t value)
{
    ensureWritableBytes(sizeof(value));
    writeInteger(beginWrite(), value);
    hasWritten(sizeof(value));
}

void Buffer::appendInt64(std::int64_t value)
{
    ensureWritableBytes(sizeof(value));
    writeInteger(beginWrite(), value);
    hasWritten(sizeof(value));
}

void Buffer::appendUInt8(std::uint8_t value)
{
    append(&value, sizeof(value));
}

void Buffer::appendUInt16(std::uint16_t value)
{
    ensureWritableBytes(sizeof(value));
    writeInteger(beginWrite(), value);
    hasWritten(sizeof(value));
}

void Buffer::appendUInt32(std::uint32_t value)
{
    ensureWritableBytes(sizeof(value));
    writeInteger(beginWrite(), value);
    hasWritten(sizeof(value));
}

void Buffer::appendUInt64(std::uint64_t value)
{
    ensureWritableBytes(sizeof(value));
    writeInteger(beginWrite(), value);
    hasWritten(sizeof(value));
}

void Buffer::prependInt8(std::int8_t value)
{
    prepend(&value, sizeof(value));
}

void Buffer::prependInt16(std::int16_t value)
{
    if (prependableBytes() < sizeof(value))
    {
        throw std::out_of_range("Buffer::prependInt16 not enough prependable bytes");
    }
    m_readerIndex -= sizeof(value);
    writeInteger(begin() + m_readerIndex, value);
}

void Buffer::prependInt32(std::int32_t value)
{
    if (prependableBytes() < sizeof(value))
    {
        throw std::out_of_range("Buffer::prependInt32 not enough prependable bytes");
    }
    m_readerIndex -= sizeof(value);
    writeInteger(begin() + m_readerIndex, value);
}

void Buffer::prependInt64(std::int64_t value)
{
    if (prependableBytes() < sizeof(value))
    {
        throw std::out_of_range("Buffer::prependInt64 not enough prependable bytes");
    }
    m_readerIndex -= sizeof(value);
    writeInteger(begin() + m_readerIndex, value);
}

void Buffer::prependUInt8(std::uint8_t value)
{
    prepend(&value, sizeof(value));
}

void Buffer::prependUInt16(std::uint16_t value)
{
    if (prependableBytes() < sizeof(value))
    {
        throw std::out_of_range("Buffer::prependUInt16 not enough prependable bytes");
    }
    m_readerIndex -= sizeof(value);
    writeInteger(begin() + m_readerIndex, value);
}

void Buffer::prependUInt32(std::uint32_t value)
{
    if (prependableBytes() < sizeof(value))
    {
        throw std::out_of_range("Buffer::prependUInt32 not enough prependable bytes");
    }
    m_readerIndex -= sizeof(value);
    writeInteger(begin() + m_readerIndex, value);
}

void Buffer::prependUInt64(std::uint64_t value)
{
    if (prependableBytes() < sizeof(value))
    {
        throw std::out_of_range("Buffer::prependUInt64 not enough prependable bytes");
    }
    m_readerIndex -= sizeof(value);
    writeInteger(begin() + m_readerIndex, value);
}

dbase::Result<Buffer::IoResult> Buffer::readFdResult(SocketType fd)
{
#if defined(_WIN32)
    ensureWritableBytes(64 * 1024);
    const int n = SocketOps::read(fd, beginWrite(), writableBytes());

    if (n > 0)
    {
        hasWritten(static_cast<std::size_t>(n));
        return IoResult{static_cast<std::size_t>(n), IoResult::Status::Ok};
    }

    if (n == 0)
    {
        return IoResult{0, IoResult::Status::EndOfFile};
    }

    const int err = WSAGetLastError();
    if (isWouldBlockError(err))
    {
        return IoResult{0, IoResult::Status::WouldBlock};
    }

    return dbase::makeSystemErrorResultT<IoResult>("Buffer::readFdResult failed", err);
#else
    char extrabuf[64 * 1024];
    iovec vec[2];
    const std::size_t writable = writableBytes();
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);
    const int iovcnt = writable < sizeof(extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n > 0)
    {
        if (static_cast<std::size_t>(n) <= writable)
        {
            hasWritten(static_cast<std::size_t>(n));
        }
        else
        {
            hasWritten(writable);
            append(extrabuf, static_cast<std::size_t>(n) - writable);
        }
        return IoResult{static_cast<std::size_t>(n), IoResult::Status::Ok};
    }

    if (n == 0)
    {
        return IoResult{0, IoResult::Status::EndOfFile};
    }

    const int err = errno;
    if (isWouldBlockError(err))
    {
        return IoResult{0, IoResult::Status::WouldBlock};
    }

    return dbase::makeSystemErrorResultT<IoResult>("Buffer::readFdResult failed", err);
#endif
}

dbase::Result<Buffer::IoResult> Buffer::writeFdResult(SocketType fd)
{
    const std::size_t readable = readableBytes();
    if (readable == 0)
    {
        return IoResult{0, IoResult::Status::Empty};
    }

    const int n = SocketOps::write(fd, peek(), readable);
    if (n > 0)
    {
        retrieve(static_cast<std::size_t>(n));
        return IoResult{static_cast<std::size_t>(n), IoResult::Status::Ok};
    }

    if (n == 0)
    {
        return IoResult{0, IoResult::Status::EndOfFile};
    }

#if defined(_WIN32)
    const int err = WSAGetLastError();
#else
    const int err = errno;
#endif

    if (isWouldBlockError(err))
    {
        return IoResult{0, IoResult::Status::WouldBlock};
    }

    return dbase::makeSystemErrorResultT<IoResult>("Buffer::writeFdResult failed", err);
}

std::size_t Buffer::readFd(SocketType fd)
{
    const auto ret = readFdResult(fd);
    if (!ret)
    {
        return 0;
    }
    return ret->status == IoResult::Status::Ok ? ret->bytes : 0;
}

std::size_t Buffer::writeFd(SocketType fd)
{
    const auto ret = writeFdResult(fd);
    if (!ret)
    {
        return 0;
    }
    return ret->status == IoResult::Status::Ok ? ret->bytes : 0;
}

void Buffer::shrink(std::size_t reserve)
{
    Buffer other(readableBytes() + reserve);
    other.append(peek(), readableBytes());
    swap(other);
}

void Buffer::clear() noexcept
{
    retrieveAll();
}

void Buffer::swap(Buffer& other) noexcept
{
    m_buffer.swap(other.m_buffer);
    std::swap(m_readerIndex, other.m_readerIndex);
    std::swap(m_writerIndex, other.m_writerIndex);
}

char* Buffer::begin() noexcept
{
    return m_buffer.data();
}

const char* Buffer::begin() const noexcept
{
    return m_buffer.data();
}

void Buffer::makeSpace(std::size_t len)
{
    const std::size_t readable = readableBytes();

    if (readable == 0)
    {
        m_readerIndex = kCheapPrepend;
        m_writerIndex = kCheapPrepend;
        if (writableBytes() >= len)
        {
            return;
        }
    }

    if (writableBytes() + prependableBytes() - kCheapPrepend >= len)
    {
        std::memmove(begin() + kCheapPrepend, peek(), readable);
        m_readerIndex = kCheapPrepend;
        m_writerIndex = m_readerIndex + readable;
        return;
    }

    const std::size_t newSize = std::max(
            m_buffer.size() * 2,
            kCheapPrepend + readable + len);
    std::vector<char> newBuffer(newSize);
    std::memcpy(newBuffer.data() + kCheapPrepend, peek(), readable);
    m_buffer.swap(newBuffer);
    m_readerIndex = kCheapPrepend;
    m_writerIndex = m_readerIndex + readable;
}
}  // namespace dbase::net