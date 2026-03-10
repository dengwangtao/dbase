#pragma once

#include <cstddef>
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

class RotatingFileSink final : public Sink
{
    public:
        RotatingFileSink(
                std::filesystem::path filePath,
                std::size_t maxFileSize,
                std::size_t maxFiles,
                bool truncate = false);

        [[nodiscard]] const std::filesystem::path& filePath() const noexcept;
        [[nodiscard]] std::size_t maxFileSize() const noexcept;
        [[nodiscard]] std::size_t maxFiles() const noexcept;

        void write(const LogEvent& event, std::string_view formatted) override;
        void flush() override;

    private:
        [[nodiscard]] std::filesystem::path rotatedPath(std::size_t index) const;
        [[nodiscard]] std::size_t currentFileSizeNoLock() const;
        void openNoLock(bool truncate);
        void rotateNoLock();

    private:
        std::filesystem::path m_filePath;
        std::ofstream m_stream;
        std::size_t m_maxFileSize{0};
        std::size_t m_maxFiles{0};
        std::mutex m_mutex;
};

}  // namespace dbase::log