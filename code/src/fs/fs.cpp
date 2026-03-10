#include "dbase/fs/fs.h"

#include <fstream>
#include <system_error>

namespace dbase::fs
{
namespace
{
dbase::Error makeIoError(const std::string& message)
{
    return dbase::Error(dbase::ErrorCode::IOError, message);
}
}  // namespace

bool exists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool isFile(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool isDirectory(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

dbase::Result<void> createDirectories(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec)
    {
        return dbase::Result<void>(makeIoError("create_directories failed: " + path.string() + ", " + ec.message()));
    }
    return dbase::Result<void>();
}

dbase::Result<void> removeFile(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec)
    {
        return dbase::Result<void>(makeIoError("remove failed: " + path.string() + ", " + ec.message()));
    }
    return dbase::Result<void>();
}

dbase::Result<void> removeAll(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec)
    {
        return dbase::Result<void>(makeIoError("remove_all failed: " + path.string() + ", " + ec.message()));
    }
    return dbase::Result<void>();
}

dbase::Result<std::string> readText(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open())
    {
        return dbase::Result<std::string>(makeIoError("open file failed: " + path.string()));
    }

    ifs.seekg(0, std::ios::end);
    const auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::string content;
    if (size > 0)
    {
        content.resize(static_cast<std::size_t>(size));
        ifs.read(content.data(), size);
    }

    if (!ifs.good() && !ifs.eof())
    {
        return dbase::Result<std::string>(makeIoError("read file failed: " + path.string()));
    }

    return dbase::Result<std::string>(std::move(content));
}

dbase::Result<void> writeText(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
    {
        return dbase::Result<void>(makeIoError("open file failed: " + path.string()));
    }

    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!ofs.good())
    {
        return dbase::Result<void>(makeIoError("write file failed: " + path.string()));
    }

    return dbase::Result<void>();
}

dbase::Result<void> appendText(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::app);
    if (!ofs.is_open())
    {
        return dbase::Result<void>(makeIoError("open file failed: " + path.string()));
    }

    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!ofs.good())
    {
        return dbase::Result<void>(makeIoError("append file failed: " + path.string()));
    }

    return dbase::Result<void>();
}

dbase::Result<std::vector<std::byte>> readBytes(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open())
    {
        return dbase::Result<std::vector<std::byte>>(makeIoError("open file failed: " + path.string()));
    }

    ifs.seekg(0, std::ios::end);
    const auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<std::byte> data;
    if (size > 0)
    {
        data.resize(static_cast<std::size_t>(size));
        ifs.read(reinterpret_cast<char*>(data.data()), size);
    }

    if (!ifs.good() && !ifs.eof())
    {
        return dbase::Result<std::vector<std::byte>>(makeIoError("read file failed: " + path.string()));
    }

    return dbase::Result<std::vector<std::byte>>(std::move(data));
}

dbase::Result<void> writeBytes(const std::filesystem::path& path, const std::vector<std::byte>& data)
{
    std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
    {
        return dbase::Result<void>(makeIoError("open file failed: " + path.string()));
    }

    if (!data.empty())
    {
        ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }

    if (!ofs.good())
    {
        return dbase::Result<void>(makeIoError("write file failed: " + path.string()));
    }

    return dbase::Result<void>();
}

dbase::Result<std::uintmax_t> fileSize(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec)
    {
        return dbase::Result<std::uintmax_t>(makeIoError("file_size failed: " + path.string() + ", " + ec.message()));
    }

    return dbase::Result<std::uintmax_t>(size);
}

}  // namespace dbase::fs