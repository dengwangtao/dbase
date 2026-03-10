#pragma once

#include "dbase/net/buffer.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dbase::net
{
class LengthFieldCodec
{
    public:
        enum class LengthMode
        {
            PayloadOnly, // 长度字段表示包体长度
            WholeFrame   // 长度字段表示整包长度
        };

        enum class DecodeStatus
        {
            Ok,
            NeedMoreData,
            InvalidLength,
            ExceedMaxFrameLength
        };

        struct DecodeResult
        {
                DecodeStatus status{DecodeStatus::NeedMoreData};
                std::string payload;
        };

        explicit LengthFieldCodec(
                std::size_t lengthFieldBytes = 4,
                LengthMode lengthMode = LengthMode::PayloadOnly,
                std::size_t maxFrameLength = 16 * 1024 * 1024);

        [[nodiscard]] std::size_t lengthFieldBytes() const noexcept;
        [[nodiscard]] LengthMode lengthMode() const noexcept;
        [[nodiscard]] std::size_t maxFrameLength() const noexcept;

        void encode(std::string_view payload, Buffer& outBuffer) const;
        [[nodiscard]] Buffer encode(std::string_view payload) const;

        [[nodiscard]] DecodeResult tryDecode(Buffer& buffer) const;

    private:
        [[nodiscard]] std::uint64_t peekLengthField(const Buffer& buffer) const;
        void appendLengthField(Buffer& buffer, std::uint64_t value) const;

    private:
        std::size_t m_lengthFieldBytes{4};
        LengthMode m_lengthMode{LengthMode::PayloadOnly};
        std::size_t m_maxFrameLength{16 * 1024 * 1024};
};
}  // namespace dbase::net