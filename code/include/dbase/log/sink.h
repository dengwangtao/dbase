#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace dbase::log
{
enum class Level;
struct LogEvent;
class Formatter;

class Sink
{
    public:
        virtual ~Sink() = default;

        virtual void write(const LogEvent& event, std::string_view formatted) = 0;
};

class ConsoleSink final : public Sink
{
    public:
        void write(const LogEvent& event, std::string_view formatted) override;

    private:
        std::mutex m_mutex;
};

}  // namespace dbase::log