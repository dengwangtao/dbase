#include "dbase/log/log.h"
#include "dbase/net/buffer.h"

#include <string>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::net::Buffer buffer;

    buffer.appendUInt32(123456789u);
    buffer.append("hello");
    buffer.append("\r\nworld\n");

    DBASE_LOG_INFO("readableBytes={}", buffer.readableBytes());

    const auto value = buffer.readUInt32();
    DBASE_LOG_INFO("value={}", value);

    if (const char* crlf = buffer.findCRLF(); crlf != nullptr)
    {
        const std::string line(buffer.peek(), static_cast<std::size_t>(crlf - buffer.peek()));
        DBASE_LOG_INFO("line={}", line);
        buffer.retrieveUntil(crlf + 2);
    }

    if (const char* eol = buffer.findEOL(); eol != nullptr)
    {
        const std::string line(buffer.peek(), static_cast<std::size_t>(eol - buffer.peek()));
        DBASE_LOG_INFO("line={}", line);
        buffer.retrieveUntil(eol + 1);
    }

    buffer.prependUInt16(42);
    DBASE_LOG_INFO("prepend value={}", buffer.readUInt16());

    buffer.append(std::string("payload"));
    DBASE_LOG_INFO("all={}", buffer.retrieveAllAsString());

    return 0;
}