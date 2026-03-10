#include "dbase/net/length_field_codec.h"

#include <limits>
#include <stdexcept>

namespace dbase::net
{
LengthFieldCodec::LengthFieldCodec(
        std::size_t lengthFieldBytes,
        LengthMode lengthMode,
        std::size_t maxFrameLength)
    : m_lengthFieldBytes(lengthFieldBytes),
      m_lengthMode(lengthMode),
      m_maxFrameLength(maxFrameLength)
{
    if (m_lengthFieldBytes != 1 && m_lengthFieldBytes != 2 && m_lengthFieldBytes != 4 && m_lengthFieldBytes != 8)
    {
        throw std::invalid_argument("LengthFieldCodec lengthFieldBytes must be one of 1, 2, 4, 8");
    }

    if (m_maxFrameLength == 0)
    {
        throw std::invalid_argument("LengthFieldCodec maxFrameLength must be greater than 0");
    }
}

std::size_t LengthFieldCodec::lengthFieldBytes() const noexcept
{
    return m_lengthFieldBytes;
}

LengthFieldCodec::LengthMode LengthFieldCodec::lengthMode() const noexcept
{
    return m_lengthMode;
}

std::size_t LengthFieldCodec::maxFrameLength() const noexcept
{
    return m_maxFrameLength;
}

void LengthFieldCodec::encode(std::string_view payload, Buffer& outBuffer) const
{
    const std::uint64_t payloadLength = static_cast<std::uint64_t>(payload.size());
    const std::uint64_t frameLength = payloadLength + static_cast<std::uint64_t>(m_lengthFieldBytes);
    const std::uint64_t encodedLength =
            (m_lengthMode == LengthMode::PayloadOnly) ? payloadLength : frameLength;

    switch (m_lengthFieldBytes)
    {
        case 1:
            if (encodedLength > std::numeric_limits<std::uint8_t>::max())
            {
                throw std::length_error("LengthFieldCodec encoded length exceeds uint8_t range");
            }
            break;
        case 2:
            if (encodedLength > std::numeric_limits<std::uint16_t>::max())
            {
                throw std::length_error("LengthFieldCodec encoded length exceeds uint16_t range");
            }
            break;
        case 4:
            if (encodedLength > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::length_error("LengthFieldCodec encoded length exceeds uint32_t range");
            }
            break;
        case 8:
            break;
        default:
            throw std::logic_error("LengthFieldCodec invalid length field bytes");
    }

    if (frameLength > m_maxFrameLength)
    {
        throw std::length_error("LengthFieldCodec frame length exceeds maxFrameLength");
    }

    outBuffer.ensureWritableBytes(static_cast<std::size_t>(frameLength));
    appendLengthField(outBuffer, encodedLength);
    outBuffer.append(payload);
}

Buffer LengthFieldCodec::encode(std::string_view payload) const
{
    Buffer outBuffer(payload.size() + m_lengthFieldBytes);
    encode(payload, outBuffer);
    return outBuffer;
}

LengthFieldCodec::DecodeResult LengthFieldCodec::tryDecode(Buffer& buffer) const
{
    DecodeResult result;

    if (buffer.readableBytes() < m_lengthFieldBytes)
    {
        result.status = DecodeStatus::NeedMoreData;
        return result;
    }

    const std::uint64_t encodedLength = peekLengthField(buffer);

    std::uint64_t frameLength = 0;
    std::uint64_t payloadLength = 0;

    if (m_lengthMode == LengthMode::PayloadOnly)
    {
        payloadLength = encodedLength;
        frameLength = payloadLength + static_cast<std::uint64_t>(m_lengthFieldBytes);
    }
    else
    {
        frameLength = encodedLength;
        if (frameLength < m_lengthFieldBytes)
        {
            result.status = DecodeStatus::InvalidLength;
            return result;
        }
        payloadLength = frameLength - static_cast<std::uint64_t>(m_lengthFieldBytes);
    }

    if (frameLength > m_maxFrameLength)
    {
        result.status = DecodeStatus::ExceedMaxFrameLength;
        return result;
    }

    if (buffer.readableBytes() < frameLength)
    {
        result.status = DecodeStatus::NeedMoreData;
        return result;
    }

    buffer.retrieve(m_lengthFieldBytes);
    result.payload = buffer.retrieveAsString(static_cast<std::size_t>(payloadLength));
    result.status = DecodeStatus::Ok;
    return result;
}

std::uint64_t LengthFieldCodec::peekLengthField(const Buffer& buffer) const
{
    switch (m_lengthFieldBytes)
    {
        case 1:
            return buffer.peekUInt8();
        case 2:
            return buffer.peekUInt16();
        case 4:
            return buffer.peekUInt32();
        case 8:
            return buffer.peekUInt64();
        default:
            throw std::logic_error("LengthFieldCodec invalid length field bytes");
    }
}

void LengthFieldCodec::appendLengthField(Buffer& buffer, std::uint64_t value) const
{
    switch (m_lengthFieldBytes)
    {
        case 1:
            buffer.appendUInt8(static_cast<std::uint8_t>(value));
            break;
        case 2:
            buffer.appendUInt16(static_cast<std::uint16_t>(value));
            break;
        case 4:
            buffer.appendUInt32(static_cast<std::uint32_t>(value));
            break;
        case 8:
            buffer.appendUInt64(static_cast<std::uint64_t>(value));
            break;
        default:
            throw std::logic_error("LengthFieldCodec invalid length field bytes");
    }
}

}  // namespace dbase::net