#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <string_view>

#include "dbase/net/buffer.h"

namespace
{
using dbase::net::Buffer;

TEST_CASE("Buffer default state", "[net][buffer]")
{
    Buffer buffer;

    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.writableBytes() >= Buffer::kInitialSize);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
    REQUIRE(buffer.capacity() >= Buffer::kCheapPrepend + Buffer::kInitialSize);
}

TEST_CASE("Buffer append and retrieve string data", "[net][buffer]")
{
    Buffer buffer;

    constexpr std::string_view text = "hello";
    buffer.append(text);

    REQUIRE(buffer.readableBytes() == text.size());
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == text);

    buffer.retrieve(2);
    REQUIRE(buffer.readableBytes() == 3);
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == "llo");

    buffer.retrieveAll();
    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
}

TEST_CASE("Buffer append multiple chunks preserves order", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abc");
    buffer.append("def");
    buffer.append("ghi");

    REQUIRE(buffer.readableBytes() == 9);
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == "abcdefghi");
}

TEST_CASE("Buffer retrieve all as string", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abcdef");

    const std::string result = buffer.retrieveAllAsString();
    REQUIRE(result == "abcdef");
    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
}

TEST_CASE("Buffer prepend bytes", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("world");
    buffer.prepend("hello", 5);

    REQUIRE(buffer.readableBytes() == 10);
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == "helloworld");
}

TEST_CASE("Buffer make space by moving readable data", "[net][buffer]")
{
    Buffer buffer(16);

    buffer.append("abcdefghij");
    buffer.retrieve(8);

    const auto writable_before = buffer.writableBytes();
    REQUIRE(buffer.readableBytes() == 2);

    buffer.append("0123456789");

    REQUIRE(buffer.readableBytes() == 12);
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == "ij0123456789");
    REQUIRE(buffer.writableBytes() <= writable_before + 8);
}

TEST_CASE("Buffer grows when space is insufficient", "[net][buffer]")
{
    Buffer buffer(8);

    const std::string big(4096, 'x');
    buffer.append(big);

    REQUIRE(buffer.readableBytes() == big.size());
    REQUIRE(buffer.capacity() >= Buffer::kCheapPrepend + big.size());
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == big);
}

TEST_CASE("Buffer beginWrite and hasWritten cooperate correctly", "[net][buffer]")
{
    Buffer buffer(16);

    char* ptr = buffer.beginWrite();
    REQUIRE(ptr != nullptr);

    ptr[0] = 'a';
    ptr[1] = 'b';
    ptr[2] = 'c';

    buffer.hasWritten(3);

    REQUIRE(buffer.readableBytes() == 3);
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == "abc");
}

TEST_CASE("Buffer append integer and read integer in network order", "[net][buffer]")
{
    Buffer buffer;

    buffer.appendInt8(static_cast<std::int8_t>(0x12));
    buffer.appendInt16(static_cast<std::int16_t>(0x3456));
    buffer.appendInt32(static_cast<std::int32_t>(0x789ABCDE));
    buffer.appendInt64(static_cast<std::int64_t>(0x1122334455667788LL));

    REQUIRE(buffer.readableBytes() == sizeof(std::int8_t) + sizeof(std::int16_t) + sizeof(std::int32_t) + sizeof(std::int64_t));

    REQUIRE(buffer.readInt8() == static_cast<std::int8_t>(0x12));
    REQUIRE(buffer.readInt16() == static_cast<std::int16_t>(0x3456));
    REQUIRE(buffer.readInt32() == static_cast<std::int32_t>(0x789ABCDE));
    REQUIRE(buffer.readInt64() == static_cast<std::int64_t>(0x1122334455667788LL));

    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer peek integer does not consume readable bytes", "[net][buffer]")
{
    Buffer buffer;

    buffer.appendInt32(123456789);

    REQUIRE(buffer.readableBytes() == sizeof(std::int32_t));
    REQUIRE(buffer.peekInt32() == 123456789);
    REQUIRE(buffer.readableBytes() == sizeof(std::int32_t));
    REQUIRE(buffer.readInt32() == 123456789);
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer retrieve integer sequences correctly", "[net][buffer]")
{
    Buffer buffer;

    buffer.appendInt16(100);
    buffer.appendInt16(200);
    buffer.appendInt16(300);

    REQUIRE(buffer.peekInt16() == 100);
    buffer.retrieve(sizeof(std::int16_t));

    REQUIRE(buffer.peekInt16() == 200);
    buffer.retrieve(sizeof(std::int16_t));

    REQUIRE(buffer.readInt16() == 300);
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer clear resets readable region", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abcdef");
    REQUIRE(buffer.readableBytes() == 6);

    buffer.clear();

    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
}

TEST_CASE("Buffer shrink reduces capacity while preserving readable data", "[net][buffer]")
{
    Buffer buffer;

    const std::string data(2048, 'z');
    buffer.append(data);
    const auto capacity_before = buffer.capacity();

    buffer.shrink(0);

    REQUIRE(buffer.readableBytes() == data.size());
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == data);
    REQUIRE(buffer.capacity() <= capacity_before);
    REQUIRE(buffer.capacity() >= Buffer::kCheapPrepend + data.size());
}

TEST_CASE("Buffer retrieve partial then append keeps contiguous semantics", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("1234567890");
    buffer.retrieve(4);
    buffer.append("abcd");

    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == "567890abcd");
}

TEST_CASE("Buffer prependable bytes increase after retrieve", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abcdef");
    const auto prepend_before = buffer.prependableBytes();

    buffer.retrieve(3);

    REQUIRE(buffer.prependableBytes() == prepend_before + 3);
    REQUIRE(std::string_view(buffer.peek(), buffer.readableBytes()) == "def");
}
}  // namespace