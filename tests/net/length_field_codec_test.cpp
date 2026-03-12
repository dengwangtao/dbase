#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "dbase/net/buffer.h"
#include "dbase/net/length_field_codec.h"

namespace
{
using dbase::net::Buffer;
using dbase::net::LengthFieldCodec;

[[nodiscard]] std::string_view bufferView(const Buffer& buffer)
{
    return std::string_view(buffer.peek(), buffer.readableBytes());
}
}  // namespace

TEST_CASE("LengthFieldCodec constructor validates arguments", "[net][length_field_codec]")
{
    REQUIRE_THROWS_AS(
            LengthFieldCodec(3, LengthFieldCodec::LengthMode::PayloadOnly, 1024),
            std::invalid_argument);

    REQUIRE_THROWS_AS(
            LengthFieldCodec(0, LengthFieldCodec::LengthMode::PayloadOnly, 1024),
            std::invalid_argument);

    REQUIRE_THROWS_AS(
            LengthFieldCodec(1, LengthFieldCodec::LengthMode::PayloadOnly, 0),
            std::invalid_argument);
}

TEST_CASE("LengthFieldCodec getters reflect constructor arguments", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(4, LengthFieldCodec::LengthMode::WholeFrame, 4096);

    REQUIRE(codec.lengthFieldBytes() == 4);
    REQUIRE(codec.lengthMode() == LengthFieldCodec::LengthMode::WholeFrame);
    REQUIRE(codec.maxFrameLength() == 4096);
}

TEST_CASE("LengthFieldCodec encode PayloadOnly with 1 byte length field", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(1, LengthFieldCodec::LengthMode::PayloadOnly, 128);
    Buffer buffer;

    codec.encode("abc", buffer);

    REQUIRE(buffer.readableBytes() == 4);
    REQUIRE(buffer.peekUInt8() == 3);
    buffer.retrieve(1);
    REQUIRE(buffer.retrieveAllAsString() == "abc");
}

TEST_CASE("LengthFieldCodec encode PayloadOnly with 2 byte length field", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    Buffer buffer;

    codec.encode("hello", buffer);

    REQUIRE(buffer.readableBytes() == 7);
    REQUIRE(buffer.peekUInt16() == 5);
    buffer.retrieve(2);
    REQUIRE(buffer.retrieveAllAsString() == "hello");
}

TEST_CASE("LengthFieldCodec encode PayloadOnly with 4 byte length field", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(4, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    Buffer buffer;

    codec.encode("world", buffer);

    REQUIRE(buffer.readableBytes() == 9);
    REQUIRE(buffer.peekUInt32() == 5u);
    buffer.retrieve(4);
    REQUIRE(buffer.retrieveAllAsString() == "world");
}

TEST_CASE("LengthFieldCodec encode PayloadOnly with 8 byte length field", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(8, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    Buffer buffer;

    codec.encode("xyz", buffer);

    REQUIRE(buffer.readableBytes() == 11);
    REQUIRE(buffer.peekUInt64() == 3u);
    buffer.retrieve(8);
    REQUIRE(buffer.retrieveAllAsString() == "xyz");
}

TEST_CASE("LengthFieldCodec encode WholeFrame writes full frame length", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::WholeFrame, 1024);
    Buffer buffer;

    codec.encode("hello", buffer);

    REQUIRE(buffer.readableBytes() == 7);
    REQUIRE(buffer.peekUInt16() == 7);
    buffer.retrieve(2);
    REQUIRE(buffer.retrieveAllAsString() == "hello");
}

TEST_CASE("LengthFieldCodec encode overload returning Buffer works", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(4, LengthFieldCodec::LengthMode::PayloadOnly, 1024);

    Buffer buffer = codec.encode("frame");

    REQUIRE(buffer.readableBytes() == 9);
    REQUIRE(buffer.peekUInt32() == 5u);
    buffer.retrieve(4);
    REQUIRE(buffer.retrieveAllAsString() == "frame");
}

TEST_CASE("LengthFieldCodec encode appends into existing output buffer", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(1, LengthFieldCodec::LengthMode::PayloadOnly, 128);
    Buffer buffer;

    buffer.append("xx");
    codec.encode("abc", buffer);

    REQUIRE(buffer.readableBytes() == 6);
    REQUIRE(buffer.retrieveAsString(2) == "xx");
    REQUIRE(buffer.peekUInt8() == 3);
    buffer.retrieve(1);
    REQUIRE(buffer.retrieveAllAsString() == "abc");
}

TEST_CASE("LengthFieldCodec tryDecode returns NeedMoreData when header is incomplete", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(4, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    Buffer buffer;

    buffer.appendUInt16(3);

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::NeedMoreData);
    REQUIRE(result.payload.empty());
    REQUIRE(buffer.readableBytes() == 2);
}

TEST_CASE("LengthFieldCodec tryDecode returns NeedMoreData when payload is incomplete in PayloadOnly mode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    Buffer buffer;

    buffer.appendUInt16(5);
    buffer.append("abc");

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::NeedMoreData);
    REQUIRE(result.payload.empty());
    REQUIRE(buffer.readableBytes() == 5);
}

TEST_CASE("LengthFieldCodec tryDecode returns NeedMoreData when payload is incomplete in WholeFrame mode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::WholeFrame, 1024);
    Buffer buffer;

    buffer.appendUInt16(7);
    buffer.append("abc");

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::NeedMoreData);
    REQUIRE(result.payload.empty());
    REQUIRE(buffer.readableBytes() == 5);
}

TEST_CASE("LengthFieldCodec tryDecode succeeds in PayloadOnly mode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    Buffer buffer = codec.encode("hello");

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::Ok);
    REQUIRE(result.payload == "hello");
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("LengthFieldCodec tryDecode succeeds in WholeFrame mode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(4, LengthFieldCodec::LengthMode::WholeFrame, 1024);
    Buffer buffer = codec.encode("world");

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::Ok);
    REQUIRE(result.payload == "world");
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("LengthFieldCodec tryDecode supports empty payload", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(1, LengthFieldCodec::LengthMode::PayloadOnly, 16);
    Buffer buffer = codec.encode("");

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::Ok);
    REQUIRE(result.payload.empty());
    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("LengthFieldCodec tryDecode returns InvalidLength for too small frame length in WholeFrame mode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(4, LengthFieldCodec::LengthMode::WholeFrame, 1024);
    Buffer buffer;

    buffer.appendUInt32(2u);

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::InvalidLength);
    REQUIRE(result.payload.empty());
    REQUIRE(buffer.readableBytes() == 4);
}

TEST_CASE("LengthFieldCodec tryDecode returns ExceedMaxFrameLength in PayloadOnly mode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::PayloadOnly, 8);
    Buffer buffer;

    buffer.appendUInt16(7);

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::ExceedMaxFrameLength);
    REQUIRE(result.payload.empty());
    REQUIRE(buffer.readableBytes() == 2);
}

TEST_CASE("LengthFieldCodec tryDecode returns ExceedMaxFrameLength in WholeFrame mode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::WholeFrame, 8);
    Buffer buffer;

    buffer.appendUInt16(9);

    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::ExceedMaxFrameLength);
    REQUIRE(result.payload.empty());
    REQUIRE(buffer.readableBytes() == 2);
}

TEST_CASE("LengthFieldCodec can decode multiple concatenated frames", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    Buffer buffer;

    codec.encode("one", buffer);
    codec.encode("two", buffer);
    codec.encode("three", buffer);

    const auto r1 = codec.tryDecode(buffer);
    REQUIRE(r1.status == LengthFieldCodec::DecodeStatus::Ok);
    REQUIRE(r1.payload == "one");

    const auto r2 = codec.tryDecode(buffer);
    REQUIRE(r2.status == LengthFieldCodec::DecodeStatus::Ok);
    REQUIRE(r2.payload == "two");

    const auto r3 = codec.tryDecode(buffer);
    REQUIRE(r3.status == LengthFieldCodec::DecodeStatus::Ok);
    REQUIRE(r3.payload == "three");

    REQUIRE(buffer.readableBytes() == 0);
}

TEST_CASE("LengthFieldCodec leaves subsequent frame bytes intact after single decode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(1, LengthFieldCodec::LengthMode::PayloadOnly, 64);
    Buffer buffer;

    codec.encode("ab", buffer);
    codec.encode("xyz", buffer);

    const auto first = codec.tryDecode(buffer);

    REQUIRE(first.status == LengthFieldCodec::DecodeStatus::Ok);
    REQUIRE(first.payload == "ab");
    REQUIRE(buffer.readableBytes() == 4);
    REQUIRE(buffer.peekUInt8() == 3);
}

TEST_CASE("LengthFieldCodec encode throws when encoded length exceeds uint8 range", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(1, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    const std::string payload(256, 'a');
    Buffer buffer;

    REQUIRE_THROWS_AS(codec.encode(payload, buffer), std::length_error);
}

TEST_CASE("LengthFieldCodec encode throws when encoded length exceeds uint16 range", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::PayloadOnly, 100000);
    const std::string payload(static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1, 'a');
    Buffer buffer;

    REQUIRE_THROWS_AS(codec.encode(payload, buffer), std::length_error);
}

TEST_CASE("LengthFieldCodec encode throws when encoded frame length exceeds uint8 range in WholeFrame mode", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(1, LengthFieldCodec::LengthMode::WholeFrame, 1024);
    const std::string payload(255, 'a');
    Buffer buffer;

    REQUIRE_THROWS_AS(codec.encode(payload, buffer), std::length_error);
}

TEST_CASE("LengthFieldCodec encode throws when frame length exceeds maxFrameLength", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::PayloadOnly, 6);
    Buffer buffer;

    REQUIRE_THROWS_AS(codec.encode("hello", buffer), std::length_error);
}

TEST_CASE("LengthFieldCodec PayloadOnly mode round trips all supported field sizes", "[net][length_field_codec]")
{
    for (const std::size_t fieldBytes : {1u, 2u, 4u, 8u})
    {
        const LengthFieldCodec codec(fieldBytes, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
        Buffer buffer = codec.encode("payload");

        const auto result = codec.tryDecode(buffer);
        REQUIRE(result.status == LengthFieldCodec::DecodeStatus::Ok);
        REQUIRE(result.payload == "payload");
        REQUIRE(buffer.readableBytes() == 0);
    }
}

TEST_CASE("LengthFieldCodec WholeFrame mode round trips all supported field sizes", "[net][length_field_codec]")
{
    for (const std::size_t fieldBytes : {1u, 2u, 4u, 8u})
    {
        const LengthFieldCodec codec(fieldBytes, LengthFieldCodec::LengthMode::WholeFrame, 1024);
        Buffer buffer = codec.encode("payload");

        const auto result = codec.tryDecode(buffer);
        REQUIRE(result.status == LengthFieldCodec::DecodeStatus::Ok);
        REQUIRE(result.payload == "payload");
        REQUIRE(buffer.readableBytes() == 0);
    }
}

TEST_CASE("LengthFieldCodec tryDecode does not consume buffer on NeedMoreData", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::PayloadOnly, 1024);
    Buffer buffer;

    buffer.appendUInt16(4);
    buffer.append("ab");

    const auto before = std::string(bufferView(buffer));
    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::NeedMoreData);
    REQUIRE(std::string(bufferView(buffer)) == before);
}

TEST_CASE("LengthFieldCodec tryDecode does not consume buffer on InvalidLength", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(4, LengthFieldCodec::LengthMode::WholeFrame, 1024);
    Buffer buffer;

    buffer.appendUInt32(1u);

    const auto before = std::string(bufferView(buffer));
    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::InvalidLength);
    REQUIRE(std::string(bufferView(buffer)) == before);
}

TEST_CASE("LengthFieldCodec tryDecode does not consume buffer on ExceedMaxFrameLength", "[net][length_field_codec]")
{
    const LengthFieldCodec codec(2, LengthFieldCodec::LengthMode::WholeFrame, 8);
    Buffer buffer;

    buffer.appendUInt16(100);

    const auto before = std::string(bufferView(buffer));
    const auto result = codec.tryDecode(buffer);

    REQUIRE(result.status == LengthFieldCodec::DecodeStatus::ExceedMaxFrameLength);
    REQUIRE(std::string(bufferView(buffer)) == before);
}