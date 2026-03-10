#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string_view>

namespace dbase::log
{
struct LogEvent;

class Sink
{
    public:
        virtual ~Sink() = default;

        virtual void write(const LogEvent& event, std::string_view formatted) = 0;
        virtual void flush() = 0;
};

class ConsoleSink final : public Sink
{
    public:
        void write(const LogEvent& event, std::string_view formatted) override;
        void flush() override;

    private:
        std::mutex m_mutex;
};

class FileSink final : public Sink
{
    public:
        explicit FileSink(std::filesystem::path filePath, bool truncate = false);

        [[nodiscard]] const std::filesystem::path& filePath() const noexcept;

        void write(const LogEvent& event, std::string_view formatted) override;
        void flush() override;

    private:
        void open(bool truncate);

    private:
        std::filesystem::path m_filePath;
        std::ofstream m_stream;
        std::mutex m_mutex;
};

}  // namespace dbase::log