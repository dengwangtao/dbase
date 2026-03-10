#include "dbase/log/sink.h"

#include "dbase/log/log.h"

#include <iostream>
#include <stdexcept>

namespace dbase::log
{
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
    const auto parent = m_filePath.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent);
    }

    const auto mode = std::ios::out | std::ios::binary | (truncate ? std::ios::trunc : std::ios::app);
    m_stream.open(m_filePath, mode);
    if (!m_stream.is_open())
    {
        throw std::runtime_error("FileSink open failed: " + m_filePath.string());
    }
}

}  // namespace dbase::log