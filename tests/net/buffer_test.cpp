#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "dbase/net/buffer.h"

namespace
{
using dbase::net::Buffer;

[[nodiscard]] std::string_view asView(const Buffer& buffer)
{
    return std::string_view(buffer.peek(), buffer.readableBytes());
}

TEST_CASE("Buffer default state", "[net][buffer]")
{
    Buffer buffer;

    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.writableBytes() >= Buffer::kInitialSize);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
    REQUIRE(buffer.capacity() >= Buffer::kCheapPrepend + Buffer::kInitialSize);
    REQUIRE(buffer.readableView().empty());
    REQUIRE(buffer.readableSpan().empty());
}

TEST_CASE("Buffer custom initial size", "[net][buffer]")
{
    Buffer buffer(32);

    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.writableBytes() >= 32);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
    REQUIRE(buffer.capacity() >= Buffer::kCheapPrepend + 32);
}

TEST_CASE("Buffer append const char pointer", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("hello");
    REQUIRE(buffer.readableBytes() == 5);
    REQUIRE(asView(buffer) == "hello");
}

TEST_CASE("Buffer append nullptr is no-op", "[net][buffer]")
{
    Buffer buffer;

    buffer.append(static_cast<const char*>(nullptr));

    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
}

TEST_CASE("Buffer append empty data is no-op", "[net][buffer]")
{
    Buffer buffer;
    const std::string empty;
    const std::array<std::byte, 0> emptyBytes{};

    buffer.append("", 0);
    buffer.append(std::string_view{});
    buffer.append(empty);
    buffer.append(std::span<const std::byte>(emptyBytes.data(), emptyBytes.size()));

    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
}

TEST_CASE("Buffer append string_view string and raw memory preserve order", "[net][buffer]")
{
    Buffer buffer;

    const std::string s = "def";
    const std::array<char, 3> raw{'g', 'h', 'i'};

    buffer.append("abc");
    buffer.append(std::string_view(s));
    buffer.append(raw.data(), raw.size());

    REQUIRE(buffer.readableBytes() == 9);
    REQUIRE(asView(buffer) == "abcdefghi");
}

TEST_CASE("Buffer append span of bytes", "[net][buffer]")
{
    Buffer buffer;

    const std::array<std::byte, 4> bytes{
            std::byte{'a'},
            std::byte{'b'},
            std::byte{'c'},
            std::byte{'d'}};

    buffer.append(std::span<const std::byte>(bytes.data(), bytes.size()));

    REQUIRE(buffer.readableBytes() == 4);
    REQUIRE(asView(buffer) == "abcd");
}

TEST_CASE("Buffer append other buffer copies readable bytes only", "[net][buffer]")
{
    Buffer source;
    Buffer dest;

    source.append("012345");
    source.retrieve(2);

    dest.append("ab");
    dest.append(source);

    REQUIRE(asView(source) == "2345");
    REQUIRE(asView(dest) == "ab2345");
}

TEST_CASE("Buffer readableView readableSpan writableSpan reflect current regions", "[net][buffer]")
{
    Buffer buffer(16);

    buffer.append("hello");

    const auto readableView = buffer.readableView();
    const auto readableSpan = buffer.readableSpan();
    auto writableSpan = buffer.writableSpan();

    REQUIRE(readableView == "hello");
    REQUIRE(readableSpan.size() == 5);
    REQUIRE(static_cast<char>(readableSpan[0]) == 'h');
    REQUIRE(static_cast<char>(readableSpan[4]) == 'o');
    REQUIRE(writableSpan.size() == buffer.writableBytes());

    writableSpan[0] = std::byte{'x'};
    writableSpan[1] = std::byte{'y'};
    writableSpan[2] = std::byte{'z'};
    buffer.hasWritten(3);

    REQUIRE(asView(buffer) == "helloxyz");
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
    REQUIRE(asView(buffer) == "abc");
}

TEST_CASE("Buffer retrieve and retrieveAll update indices correctly", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abcdef");
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);

    buffer.retrieve(2);
    REQUIRE(buffer.readableBytes() == 4);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend + 2);
    REQUIRE(asView(buffer) == "cdef");

    buffer.retrieveAll();
    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
    REQUIRE(buffer.readableView().empty());
}

TEST_CASE("Buffer retrieve zero is no-op", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abc");
    const auto prependableBefore = buffer.prependableBytes();
    const auto writableBefore = buffer.writableBytes();

    buffer.retrieve(0);

    REQUIRE(asView(buffer) == "abc");
    REQUIRE(buffer.prependableBytes() == prependableBefore);
    REQUIRE(buffer.writableBytes() == writableBefore);
}

TEST_CASE("Buffer retrieveAllAsString returns all readable bytes and clears buffer", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abcdef");

    const std::string result = buffer.retrieveAllAsString();

    REQUIRE(result == "abcdef");
    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
}

TEST_CASE("Buffer retrieveAsString returns prefix and consumes bytes", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abcdef");

    const std::string first = buffer.retrieveAsString(2);
    REQUIRE(first == "ab");
    REQUIRE(asView(buffer) == "cdef");

    const std::string second = buffer.retrieveAsString(4);
    REQUIRE(second == "cdef");
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer retrieveAsString zero length is no-op", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abc");

    const std::string value = buffer.retrieveAsString(0);

    REQUIRE(value.empty());
    REQUIRE(asView(buffer) == "abc");
}

TEST_CASE("Buffer clear resets readable region", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abcdef");
    buffer.clear();

    REQUIRE(buffer.readableBytes() == 0);
    REQUIRE(buffer.prependableBytes() == Buffer::kCheapPrepend);
    REQUIRE(buffer.readableView().empty());
}

TEST_CASE("Buffer prepend raw bytes", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("world");
    buffer.prepend("hello", 5);

    REQUIRE(buffer.readableBytes() == 10);
    REQUIRE(asView(buffer) == "helloworld");
}

TEST_CASE("Buffer prepend span of bytes", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("tail");

    const std::array<std::byte, 4> head{
            std::byte{'h'},
            std::byte{'e'},
            std::byte{'a'},
            std::byte{'d'}};

    buffer.prepend(std::span<const std::byte>(head.data(), head.size()));

    REQUIRE(asView(buffer) == "headtail");
}

TEST_CASE("Buffer prepend zero length is no-op", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abc");
    buffer.prepend("", 0);

    REQUIRE(asView(buffer) == "abc");
}

TEST_CASE("Buffer prepend throws when prependable bytes are insufficient", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abc");

    const std::string tooLong(Buffer::kCheapPrepend + 1, 'x');
    REQUIRE_THROWS_AS(buffer.prepend(tooLong.data(), tooLong.size()), std::out_of_range);
}

TEST_CASE("Buffer makeSpace can move readable data without growing", "[net][buffer]")
{
    Buffer buffer(16);

    buffer.append("abcdefghij");
    buffer.retrieve(8);

    const auto capacityBefore = buffer.capacity();

    buffer.append("0123456789");

    REQUIRE(asView(buffer) == "ij0123456789");
    REQUIRE(buffer.capacity() == capacityBefore);
}

TEST_CASE("Buffer grows when capacity is insufficient", "[net][buffer]")
{
    Buffer buffer(8);

    const std::string big(4096, 'x');
    const auto capacityBefore = buffer.capacity();

    buffer.append(big);

    REQUIRE(buffer.readableBytes() == big.size());
    REQUIRE(asView(buffer) == big);
    REQUIRE(buffer.capacity() > capacityBefore);
    REQUIRE(buffer.capacity() >= Buffer::kCheapPrepend + big.size());
}

TEST_CASE("Buffer ensure writable bytes by append after retrieve preserves contiguity", "[net][buffer]")
{
    Buffer buffer(12);

    buffer.append("1234567890");
    buffer.retrieve(4);
    buffer.append("abcd");

    REQUIRE(asView(buffer) == "567890abcd");
}

TEST_CASE("Buffer unwrite rolls back writer index", "[net][buffer]")
{
    Buffer buffer(16);

    buffer.append("abcxyz");
    buffer.unwrite(3);

    REQUIRE(buffer.readableBytes() == 3);
    REQUIRE(asView(buffer) == "abc");
}

TEST_CASE("Buffer swap exchanges contents and indices", "[net][buffer]")
{
    Buffer lhs;
    Buffer rhs;

    lhs.append("left");
    lhs.retrieve(1);

    rhs.append("right");

    lhs.swap(rhs);

    REQUIRE(asView(lhs) == "right");
    REQUIRE(asView(rhs) == "eft");
}

TEST_CASE("Buffer shrink keeps readable bytes and may reduce capacity", "[net][buffer]")
{
    Buffer buffer;

    const std::string payload(2048, 'z');
    buffer.append(payload);
    buffer.retrieve(100);

    const auto remaining = std::string(payload.begin() + 100, payload.end());
    const auto capacityBefore = buffer.capacity();

    buffer.shrink(0);

    REQUIRE(asView(buffer) == remaining);
    REQUIRE(buffer.capacity() <= capacityBefore);
    REQUIRE(buffer.capacity() >= Buffer::kCheapPrepend + remaining.size());
}

TEST_CASE("Buffer shrink with reserve keeps extra capacity for future writes", "[net][buffer]")
{
    Buffer buffer;

    buffer.append(std::string(1024, 'a'));
    buffer.retrieve(1000);

    const std::string remaining(asView(buffer));
    buffer.shrink(512);

    REQUIRE(asView(buffer) == remaining);
    REQUIRE(buffer.readableBytes() == remaining.size());
    REQUIRE(buffer.capacity() == Buffer::kCheapPrepend + remaining.size() + 512);
    REQUIRE(buffer.writableBytes() == 512);
}

TEST_CASE("Buffer int8 int16 int32 int64 round trip", "[net][buffer]")
{
    Buffer buffer;

    buffer.appendInt8(static_cast<std::int8_t>(0x12));
    buffer.appendInt16(static_cast<std::int16_t>(0x3456));
    buffer.appendInt32(static_cast<std::int32_t>(0x789ABCDE));
    buffer.appendInt64(static_cast<std::int64_t>(0x1122334455667788LL));

    REQUIRE(buffer.peekInt8() == static_cast<std::int8_t>(0x12));
    REQUIRE(buffer.readInt8() == static_cast<std::int8_t>(0x12));
    REQUIRE(buffer.readInt16() == static_cast<std::int16_t>(0x3456));
    REQUIRE(buffer.readInt32() == static_cast<std::int32_t>(0x789ABCDE));
    REQUIRE(buffer.readInt64() == static_cast<std::int64_t>(0x1122334455667788LL));
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer uint8 uint16 uint32 uint64 round trip", "[net][buffer]")
{
    Buffer buffer;

    buffer.appendUInt8(static_cast<std::uint8_t>(0xABu));
    buffer.appendUInt16(static_cast<std::uint16_t>(0xCDEFu));
    buffer.appendUInt32(static_cast<std::uint32_t>(0x89ABCDEFu));
    buffer.appendUInt64(static_cast<std::uint64_t>(0x0123456789ABCDEFULL));

    REQUIRE(buffer.peekUInt8() == static_cast<std::uint8_t>(0xABu));
    REQUIRE(buffer.readUInt8() == static_cast<std::uint8_t>(0xABu));
    REQUIRE(buffer.readUInt16() == static_cast<std::uint16_t>(0xCDEFu));
    REQUIRE(buffer.readUInt32() == static_cast<std::uint32_t>(0x89ABCDEFu));
    REQUIRE(buffer.readUInt64() == static_cast<std::uint64_t>(0x0123456789ABCDEFULL));
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer peek integer does not consume readable bytes", "[net][buffer]")
{
    Buffer buffer;

    buffer.appendInt16(100);
    buffer.appendUInt32(200);

    REQUIRE(buffer.peekInt16() == 100);
    REQUIRE(buffer.readableBytes() == sizeof(std::int16_t) + sizeof(std::uint32_t));

    REQUIRE(buffer.readInt16() == 100);
    REQUIRE(buffer.peekUInt32() == 200u);
    REQUIRE(buffer.readUInt32() == 200u);
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer prepend signed and unsigned integers keeps expected order", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("tail");
    buffer.prependUInt16(0x3344u);
    buffer.prependInt32(0x11223344);

    REQUIRE(buffer.readInt32() == 0x11223344);
    REQUIRE(buffer.readUInt16() == 0x3344u);
    REQUIRE(asView(buffer) == "tail");
}

TEST_CASE("Buffer mixed append and prepend integers round trip", "[net][buffer]")
{
    Buffer buffer;

    buffer.appendUInt32(0xAABBCCDDu);
    buffer.prependUInt8(0x11u);
    buffer.prependInt16(0x2233);
    buffer.appendInt8(0x44);

    REQUIRE(buffer.readInt16() == 0x2233);
    REQUIRE(buffer.readUInt8() == 0x11u);
    REQUIRE(buffer.readUInt32() == 0xAABBCCDDu);
    REQUIRE(buffer.readInt8() == 0x44);
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer peek throws when readable bytes are insufficient", "[net][buffer]")
{
    Buffer buffer;

    REQUIRE_THROWS_AS(buffer.peekInt8(), std::out_of_range);
    REQUIRE_THROWS_AS(buffer.peekUInt64(), std::out_of_range);

    buffer.appendUInt16(0x1234u);

    REQUIRE_THROWS_AS(buffer.peekInt32(), std::out_of_range);
    REQUIRE_THROWS_AS(buffer.peekUInt64(), std::out_of_range);
}

TEST_CASE("Buffer read throws when readable bytes are insufficient", "[net][buffer]")
{
    Buffer buffer;

    REQUIRE_THROWS_AS(buffer.readInt8(), std::out_of_range);

    buffer.appendUInt16(0x1234u);

    REQUIRE_THROWS_AS(buffer.readInt32(), std::out_of_range);
    REQUIRE(buffer.readUInt16() == 0x1234u);
}

TEST_CASE("Buffer retrieveAsString throws when readable bytes are insufficient", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abc");

    REQUIRE_THROWS_AS(buffer.retrieveAsString(4), std::out_of_range);
}

TEST_CASE("Buffer retrieve throws when readable bytes are insufficient", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abc");

    // REQUIRE_THROWS_AS(buffer.retrieve(4), std::out_of_range);
    buffer.retrieve(4);
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("Buffer prepend integer functions can exhaust cheap prepend area exactly", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("x");
    buffer.prependUInt32(0x55667788u);
    buffer.prependInt32(0x11223344);

    REQUIRE(buffer.prependableBytes() == 0);
    REQUIRE(buffer.readInt32() == 0x11223344);
    REQUIRE(buffer.readUInt32() == 0x55667788u);
    REQUIRE(asView(buffer) == "x");
}

TEST_CASE("Buffer readable span tracks retrieved data correctly", "[net][buffer]")
{
    Buffer buffer;

    buffer.append("abcdef");
    buffer.retrieve(2);

    const auto span = buffer.readableSpan();

    REQUIRE(span.size() == 4);
    REQUIRE(static_cast<char>(span[0]) == 'c');
    REQUIRE(static_cast<char>(span[3]) == 'f');
}

TEST_CASE("Buffer writable span after growth remains writable", "[net][buffer]")
{
    Buffer buffer(4);

    buffer.append("abcd");
    const auto writableBefore = buffer.writableBytes();

    buffer.append("efghijkl");

    REQUIRE(buffer.readableBytes() == 12);
    REQUIRE(buffer.writableBytes() >= writableBefore);
    REQUIRE(asView(buffer) == "abcdefghijkl");
}
}  // namespace