#pragma once

#include "dbase/net/socket_ops.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dbase::net
{
class Buffer
{
    public:
        static constexpr std::size_t kCheapPrepend = 8;
        static constexpr std::size_t kInitialSize = 1024;

        explicit Buffer(std::size_t initialSize = kInitialSize);

        [[nodiscard]] std::size_t readableBytes() const noexcept;
        [[nodiscard]] std::size_t writableBytes() const noexcept;
        [[nodiscard]] std::size_t prependableBytes() const noexcept;
        [[nodiscard]] std::size_t capacity() const noexcept;

        [[nodiscard]] const char* peek() const noexcept;
        [[nodiscard]] char* beginWrite() noexcept;
        [[nodiscard]] const char* beginWrite() const noexcept;

        [[nodiscard]] const char* findCRLF() const noexcept;
        [[nodiscard]] const char* findEOL() const noexcept;

        void retrieve(std::size_t len) noexcept;
        void retrieveUntil(const char* end) noexcept;
        void retrieveAll() noexcept;

        [[nodiscard]] std::string retrieveAsString(std::size_t len);
        [[nodiscard]] std::string retrieveAllAsString();

        [[nodiscard]] std::string_view readableView() const noexcept;
        [[nodiscard]] std::span<const std::byte> readableSpan() const noexcept;
        [[nodiscard]] std::span<std::byte> writableSpan() noexcept;

        void ensureWritableBytes(std::size_t len);
        void hasWritten(std::size_t len) noexcept;
        void unwrite(std::size_t len) noexcept;

        void append(const void* data, std::size_t len);
        void append(const char* data);
        void append(std::string_view data);
        void append(const std::string& data);
        void append(std::span<const std::byte> data);
        void append(const Buffer& other);

        void prepend(const void* data, std::size_t len);
        void prepend(std::span<const std::byte> data);

        [[nodiscard]] std::int8_t peekInt8() const;
        [[nodiscard]] std::int16_t peekInt16() const;
        [[nodiscard]] std::int32_t peekInt32() const;
        [[nodiscard]] std::int64_t peekInt64() const;

        [[nodiscard]] std::uint8_t peekUInt8() const;
        [[nodiscard]] std::uint16_t peekUInt16() const;
        [[nodiscard]] std::uint32_t peekUInt32() const;
        [[nodiscard]] std::uint64_t peekUInt64() const;

        [[nodiscard]] std::int8_t readInt8();
        [[nodiscard]] std::int16_t readInt16();
        [[nodiscard]] std::int32_t readInt32();
        [[nodiscard]] std::int64_t readInt64();

        [[nodiscard]] std::uint8_t readUInt8();
        [[nodiscard]] std::uint16_t readUInt16();
        [[nodiscard]] std::uint32_t readUInt32();
        [[nodiscard]] std::uint64_t readUInt64();

        void appendInt8(std::int8_t value);
        void appendInt16(std::int16_t value);
        void appendInt32(std::int32_t value);
        void appendInt64(std::int64_t value);

        void appendUInt8(std::uint8_t value);
        void appendUInt16(std::uint16_t value);
        void appendUInt32(std::uint32_t value);
        void appendUInt64(std::uint64_t value);

        void prependInt8(std::int8_t value);
        void prependInt16(std::int16_t value);
        void prependInt32(std::int32_t value);
        void prependInt64(std::int64_t value);

        void prependUInt8(std::uint8_t value);
        void prependUInt16(std::uint16_t value);
        void prependUInt32(std::uint32_t value);
        void prependUInt64(std::uint64_t value);

        [[nodiscard]] std::size_t readFd(SocketType fd);
        [[nodiscard]] std::size_t writeFd(SocketType fd);

        void shrink(std::size_t reserve = 0);
        void clear() noexcept;
        void swap(Buffer& other) noexcept;

    private:
        [[nodiscard]] char* begin() noexcept;
        [[nodiscard]] const char* begin() const noexcept;

        void makeSpace(std::size_t len);

    private:
        std::vector<char> m_buffer;
        std::size_t m_readerIndex{0};
        std::size_t m_writerIndex{0};
};

}  // namespace dbase::net