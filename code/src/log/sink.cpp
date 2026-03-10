#include "dbase/log/sink.h"

#include "dbase/log/log.h"
#include "dbase/time/time.h"

#include <iostream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace dbase::log
{
namespace
{
[[nodiscard]] std::size_t fileSizeOrZero(const std::filesystem::path& filePath)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(filePath, ec);
    if (ec)
    {
        return 0;
    }

    return static_cast<std::size_t>(size);
}

void ensureParentDirectory(const std::filesystem::path& filePath)
{
    const auto parent = filePath.parent_path();
    if (parent.empty())
    {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec)
    {
        throw std::runtime_error("create_directories failed: " + parent.string() + ", " + ec.message());
    }
}

void removeIfExists(const std::filesystem::path& filePath)
{
    std::error_code ec;
    std::filesystem::remove(filePath, ec);
    if (ec)
    {
        throw std::runtime_error("remove failed: " + filePath.string() + ", " + ec.message());
    }
}

void renameFile(const std::filesystem::path& from, const std::filesystem::path& to)
{
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec)
    {
        throw std::runtime_error(
                "rename failed: " + from.string() + " -> " + to.string() + ", " + ec.message());
    }
}

[[nodiscard]] std::string dateTextFromTimestampUs(std::int64_t timestampUs)
{
    return dbase::time::formatTimestampMs(timestampUs / 1000, "%Y-%m-%d");
}
}  // namespace

void ConsoleSink::write(const LogEvent& event, std::string_view formatted)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (static_cast<int>(event.level) >= static_cast<int>(Level::Error))
    {
        std::cerr << formatted << '\n';
        return;
    }

    std::cout << formatted << '\n';
}

void ConsoleSink::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout.flush();
    std::cerr.flush();
}

FileSink::FileSink(std::filesystem::path filePath, bool truncate)
    : m_filePath(std::move(filePath))
{
    open(truncate);
}

const std::filesystem::path& FileSink::filePath() const noexcept
{
    return m_filePath;
}

void FileSink::write(const LogEvent&, std::string_view formatted)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_stream.is_open())
    {
        open(false);
    }

    m_stream << formatted << '\n';
    if (!m_stream.good())
    {
        throw std::runtime_error("FileSink write failed: " + m_filePath.string());
    }
}

void FileSink::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_stream.is_open())
    {
        m_stream.flush();
        if (!m_stream.good())
        {
            throw std::runtime_error("FileSink flush failed: " + m_filePath.string());
        }
    }
}

void FileSink::open(bool truncate)
{
    ensureParentDirectory(m_filePath);

    const auto mode = std::ios::out | std::ios::binary | (truncate ? std::ios::trunc : std::ios::app);
    m_stream.open(m_filePath, mode);
    if (!m_stream.is_open())
    {
        throw std::runtime_error("FileSink open failed: " + m_filePath.string());
    }
}

RotatingFileSink::RotatingFileSink(
        std::filesystem::path filePath,
        std::size_t maxFileSize,
        std::size_t maxFiles,
        bool truncate)
    : m_filePath(std::move(filePath)),
      m_maxFileSize(maxFileSize),
      m_maxFiles(maxFiles)
{
    if (m_maxFileSize == 0)
    {
        throw std::invalid_argument("RotatingFileSink maxFileSize must be greater than 0");
    }

    if (m_maxFiles == 0)
    {
        throw std::invalid_argument("RotatingFileSink maxFiles must be greater than 0");
    }

    openNoLock(truncate);
}

const std::filesystem::path& RotatingFileSink::filePath() const noexcept
{
    return m_filePath;
}

std::size_t RotatingFileSink::maxFileSize() const noexcept
{
    return m_maxFileSize;
}

std::size_t RotatingFileSink::maxFiles() const noexcept
{
    return m_maxFiles;
}

void RotatingFileSink::write(const LogEvent&, std::string_view formatted)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_stream.is_open())
    {
        openNoLock(false);
    }

    const std::size_t incomingBytes = formatted.size() + 1;
    const std::size_t currentBytes = currentFileSizeNoLock();

    if (currentBytes > 0 && currentBytes + incomingBytes > m_maxFileSize)
    {
        rotateNoLock();
    }

    m_stream << formatted << '\n';
    if (!m_stream.good())
    {
        throw std::runtime_error("RotatingFileSink write failed: " + m_filePath.string());
    }
}

void RotatingFileSink::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_stream.is_open())
    {
        m_stream.flush();
        if (!m_stream.good())
        {
            throw std::runtime_error("RotatingFileSink flush failed: " + m_filePath.string());
        }
    }
}

std::filesystem::path RotatingFileSink::rotatedPath(std::size_t index) const
{
    const auto parent = m_filePath.parent_path();
    const auto stem = m_filePath.stem().string();
    const auto ext = m_filePath.extension().string();

    return parent / std::filesystem::path(stem + "." + std::to_string(index) + ext);
}

std::size_t RotatingFileSink::currentFileSizeNoLock() const
{
    return fileSizeOrZero(m_filePath);
}

void RotatingFileSink::openNoLock(bool truncate)
{
    ensureParentDirectory(m_filePath);

    const auto mode = std::ios::out | std::ios::binary | (truncate ? std::ios::trunc : std::ios::app);
    m_stream.open(m_filePath, mode);
    if (!m_stream.is_open())
    {
        throw std::runtime_error("RotatingFileSink open failed: " + m_filePath.string());
    }
}

void RotatingFileSink::rotateNoLock()
{
    if (m_stream.is_open())
    {
        m_stream.flush();
        m_stream.close();
    }

    removeIfExists(rotatedPath(m_maxFiles));

    for (std::size_t i = m_maxFiles; i > 1; --i)
    {
        const auto from = rotatedPath(i - 1);
        const auto to = rotatedPath(i);

        if (std::filesystem::exists(from))
        {
            removeIfExists(to);
            renameFile(from, to);
        }
    }

    if (std::filesystem::exists(m_filePath))
    {
        const auto firstRotated = rotatedPath(1);
        removeIfExists(firstRotated);
        renameFile(m_filePath, firstRotated);
    }

    openNoLock(true);
}

DailyFileSink::DailyFileSink(std::filesystem::path filePath, bool truncate)
    : m_filePath(std::move(filePath))
{
    openForDateNoLock(dateTextFromTimestampUs(dbase::time::nowUs()), truncate);
}

const std::filesystem::path& DailyFileSink::filePath() const noexcept
{
    return m_filePath;
}

const std::string& DailyFileSink::currentDate() const noexcept
{
    return m_currentDate;
}

void DailyFileSink::write(const LogEvent& event, std::string_view formatted)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto eventDate = dateTextFromTimestampUs(event.timestampUs);

    if (!m_stream.is_open() || eventDate != m_currentDate)
    {
        openForDateNoLock(eventDate, false);
    }

    m_stream << formatted << '\n';
    if (!m_stream.good())
    {
        throw std::runtime_error("DailyFileSink write failed: " + datedPath(m_currentDate).string());
    }
}

void DailyFileSink::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_stream.is_open())
    {
        m_stream.flush();
        if (!m_stream.good())
        {
            throw std::runtime_error("DailyFileSink flush failed: " + datedPath(m_currentDate).string());
        }
    }
}

std::filesystem::path DailyFileSink::datedPath(std::string_view date) const
{
    const auto parent = m_filePath.parent_path();
    const auto stem = m_filePath.stem().string();
    const auto ext = m_filePath.extension().string();

    if (ext.empty())
    {
        return parent / std::filesystem::path(stem + "." + std::string(date));
    }

    return parent / std::filesystem::path(stem + "." + std::string(date) + ext);
}

void DailyFileSink::openForDateNoLock(std::string date, bool truncate)
{
    if (m_stream.is_open())
    {
        m_stream.flush();
        m_stream.close();
    }

    const auto targetPath = datedPath(date);
    ensureParentDirectory(targetPath);

    const auto mode = std::ios::out | std::ios::binary | (truncate ? std::ios::trunc : std::ios::app);
    m_stream.open(targetPath, mode);
    if (!m_stream.is_open())
    {
        throw std::runtime_error("DailyFileSink open failed: " + targetPath.string());
    }

    m_currentDate = std::move(date);
}

}  // namespace dbase::log