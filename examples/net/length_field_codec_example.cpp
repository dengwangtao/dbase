#include "dbase/log/log.h"
#include "dbase/net/buffer.h"
#include "dbase/net/length_field_codec.h"

#include <string_view>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::net::LengthFieldCodec codec(
            4,
            dbase::net::LengthFieldCodec::LengthMode::PayloadOnly,
            1024 * 1024);

    dbase::net::Buffer inBuffer;

    {
        auto frame1 = codec.encode(std::string_view("hello"));
        auto frame2 = codec.encode(std::string_view("world"));
        inBuffer.append(frame1);
        inBuffer.append(frame2);
    }

    for (;;)
    {
        const auto result = codec.tryDecode(inBuffer);
        if (result.status == dbase::net::LengthFieldCodec::DecodeStatus::NeedMoreData)
        {
            break;
        }

        if (result.status != dbase::net::LengthFieldCodec::DecodeStatus::Ok)
        {
            DBASE_LOG_ERROR("decode failed, status={}", static_cast<int>(result.status));
            break;
        }

        DBASE_LOG_INFO("decoded payload={}", result.payload);
    }

    {
        dbase::net::Buffer partial;
        auto frame = codec.encode(std::string_view("partial-frame"));
        partial.append(frame.peek(), 3);

        auto result = codec.tryDecode(partial);
        DBASE_LOG_INFO("partial decode status={}", static_cast<int>(result.status));

        partial.append(frame.peek() + 3, frame.readableBytes() - 3);
        result = codec.tryDecode(partial);

        if (result.status == dbase::net::LengthFieldCodec::DecodeStatus::Ok)
        {
            DBASE_LOG_INFO("decoded partial payload={}", result.payload);
        }
    }

    return 0;
}