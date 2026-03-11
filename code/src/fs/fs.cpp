#include "dbase/fs/fs.h"

#include "dbase/error/error.h"

#include <fstream>
#include <sstream>
#include <system_error>

namespace dbase::fs
{
namespace
{
[[nodiscard]] dbase::Error makeFsError(std::string action, const std::filesystem::path& path)
{
    if (!action.empty())
    {
        action += ": ";
    }
    action += path.string();
    return dbase::Error(dbase::ErrorCode::IOError, std::move(action));
}

[[nodiscard]] dbase::Error makeFsError(
        std::string action,
        const std::filesystem::path& path,
        const std::error_code& ec)
{
    std::string message;
    if (!action.empty())
    {
        message += std::move(action);
        message += ": ";
    }
    message += path.string();
    message += ": ";
    message += ec.message();
    return dbase::Error(dbase::ErrorCode::IOError, std::move(message));
}

[[nodiscard]] dbase::Error makeFsError(
        std::string action,
        const std::filesystem::path& from,
        const std::filesystem::path& to,
        const std::error_code& ec)
{
    std::string message;
    if (!action.empty())
    {
        message += std::move(action);
        message += ": ";
    }
    message += from.string();
    message += " -> ";
    message += to.string();
    message += ": ";
    message += ec.message();
    return dbase::Error(dbase::ErrorCode::IOError, std::move(message));
}

[[nodiscard]] dbase::Result<void> ensureParentDirectory(const std::filesystem::path& path)
{
    const auto parent = path.parent_path();
    if (parent.empty())
    {
        return {};
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec)
    {
        return dbase::Result<void>(makeFsError("create_directories", parent, ec));
    }

    return {};
}

[[nodiscard]] std::filesystem::path makeTempPath(const std::filesystem::path& path)
{
    auto temp = path;
    temp += ".tmp";
    return temp;
}

template <typename Writer>
[[nodiscard]] dbase::Result<void> writeAtomicImpl(
        const std::filesystem::path& path,
        Writer&& writer)
{
    auto ensureRet = ensureParentDirectory(path);
    if (!ensureRet)
    {
        return ensureRet;
    }

    const auto tempPath = makeTempPath(path);

    {
        std::ofstream ofs(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            return dbase::Result<void>(makeFsError("open file failed", tempPath));
        }

        writer(ofs);

        ofs.flush();
        if (!ofs.good())
        {
            return dbase::Result<void>(makeFsError("write file failed", tempPath));
        }
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, path, ec);
    if (ec)
    {
        std::filesystem::remove(tempPath);
        return dbase::Result<void>(makeFsError("rename failed", tempPath, path, ec));
    }

    return {};
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

dbase::Result<std::filesystem::path> absolute(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto result = std::filesystem::absolute(path, ec);
    if (ec)
    {
        return dbase::Result<std::filesystem::path>(makeFsError("absolute failed", path, ec));
    }

    return result;
}

dbase::Result<std::filesystem::path> canonical(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto result = std::filesystem::canonical(path, ec);
    if (ec)
    {
        return dbase::Result<std::filesystem::path>(makeFsError("canonical failed", path, ec));
    }

    return result;
}

dbase::Result<void> createDirectories(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec)
    {
        return dbase::Result<void>(makeFsError("create_directories failed", path, ec));
    }

    return {};
}

dbase::Result<void> removeFile(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec)
    {
        return dbase::Result<void>(makeFsError("remove failed", path, ec));
    }

    return {};
}

dbase::Result<void> removeAll(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec)
    {
        return dbase::Result<void>(makeFsError("remove_all failed", path, ec));
    }

    return {};
}

dbase::Result<void> rename(const std::filesystem::path& from, const std::filesystem::path& to)
{
    auto ensureRet = ensureParentDirectory(to);
    if (!ensureRet)
    {
        return ensureRet;
    }

    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec)
    {
        return dbase::Result<void>(makeFsError("rename failed", from, to, ec));
    }

    return {};
}

dbase::Result<void> copyFile(
        const std::filesystem::path& from,
        const std::filesystem::path& to,
        bool overwrite)
{
    auto ensureRet = ensureParentDirectory(to);
    if (!ensureRet)
    {
        return ensureRet;
    }

    std::error_code ec;
    const auto options = overwrite
                                 ? std::filesystem::copy_options::overwrite_existing
                                 : std::filesystem::copy_options::none;

    std::filesystem::copy_file(from, to, options, ec);
    if (ec)
    {
        return dbase::Result<void>(makeFsError("copy_file failed", from, to, ec));
    }

    return {};
}

dbase::Result<std::string> readText(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open())
    {
        return dbase::Result<std::string>(makeFsError("open file failed", path));
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();

    if (!ifs.good() && !ifs.eof())
    {
        return dbase::Result<std::string>(makeFsError("read file failed", path));
    }

    return oss.str();
}

dbase::Result<std::vector<std::string>> readLines(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open())
    {
        return dbase::Result<std::vector<std::string>>(makeFsError("open file failed", path));
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        lines.emplace_back(std::move(line));
    }

    if (!ifs.eof() && ifs.fail())
    {
        return dbase::Result<std::vector<std::string>>(makeFsError("read lines failed", path));
    }

    return lines;
}

dbase::Result<void> writeText(const std::filesystem::path& path, std::string_view content)
{
    auto ensureRet = ensureParentDirectory(path);
    if (!ensureRet)
    {
        return ensureRet;
    }

    std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
    {
        return dbase::Result<void>(makeFsError("open file failed", path));
    }

    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();

    if (!ofs.good())
    {
        return dbase::Result<void>(makeFsError("write file failed", path));
    }

    return {};
}

dbase::Result<void> writeTextAtomic(const std::filesystem::path& path, std::string_view content)
{
    return writeAtomicImpl(path, [content](std::ofstream& ofs)
                           { ofs.write(content.data(), static_cast<std::streamsize>(content.size())); });
}

dbase::Result<void> appendText(const std::filesystem::path& path, std::string_view content)
{
    auto ensureRet = ensureParentDirectory(path);
    if (!ensureRet)
    {
        return ensureRet;
    }

    std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::app);
    if (!ofs.is_open())
    {
        return dbase::Result<void>(makeFsError("open file failed", path));
    }

    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();

    if (!ofs.good())
    {
        return dbase::Result<void>(makeFsError("append file failed", path));
    }

    return {};
}

dbase::Result<std::vector<std::byte>> readBytes(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open())
    {
        return dbase::Result<std::vector<std::byte>>(makeFsError("open file failed", path));
    }

    ifs.seekg(0, std::ios::end);
    const auto endPos = ifs.tellg();
    if (endPos < 0)
    {
        return dbase::Result<std::vector<std::byte>>(makeFsError("tellg failed", path));
    }

    ifs.seekg(0, std::ios::beg);

    std::vector<std::byte> data(static_cast<std::size_t>(endPos));
    if (!data.empty())
    {
        ifs.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!ifs)
        {
            return dbase::Result<std::vector<std::byte>>(makeFsError("read file failed", path));
        }
    }

    return data;
}

dbase::Result<void> writeBytes(const std::filesystem::path& path, const std::vector<std::byte>& data)
{
    auto ensureRet = ensureParentDirectory(path);
    if (!ensureRet)
    {
        return ensureRet;
    }

    std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
    {
        return dbase::Result<void>(makeFsError("open file failed", path));
    }

    if (!data.empty())
    {
        ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    ofs.flush();

    if (!ofs.good())
    {
        return dbase::Result<void>(makeFsError("write file failed", path));
    }

    return {};
}

dbase::Result<void> writeBytesAtomic(const std::filesystem::path& path, const std::vector<std::byte>& data)
{
    return writeAtomicImpl(path, [&data](std::ofstream& ofs)
                           {
        if (!data.empty())
        {
            ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        } });
}

dbase::Result<std::uintmax_t> fileSize(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec)
    {
        return dbase::Result<std::uintmax_t>(makeFsError("file_size failed", path, ec));
    }

    return size;
}

dbase::Result<std::vector<std::filesystem::path>> listFiles(
        const std::filesystem::path& path,
        bool recursive)
{
    std::vector<std::filesystem::path> result;
    std::error_code ec;

    if (!std::filesystem::exists(path, ec))
    {
        if (ec)
        {
            return dbase::Result<std::vector<std::filesystem::path>>(makeFsError("exists failed", path, ec));
        }
        return result;
    }

    if (!std::filesystem::is_directory(path, ec))
    {
        if (ec)
        {
            return dbase::Result<std::vector<std::filesystem::path>>(makeFsError("is_directory failed", path, ec));
        }
        return dbase::Result<std::vector<std::filesystem::path>>(makeFsError("not a directory", path));
    }

    if (recursive)
    {
        for (std::filesystem::recursive_directory_iterator it(path, ec), end; it != end; it.increment(ec))
        {
            if (ec)
            {
                return dbase::Result<std::vector<std::filesystem::path>>(makeFsError("recursive_directory_iterator failed", path, ec));
            }

            if (it->is_regular_file(ec))
            {
                if (ec)
                {
                    return dbase::Result<std::vector<std::filesystem::path>>(makeFsError("is_regular_file failed", it->path(), ec));
                }

                result.emplace_back(it->path());
            }
        }
    }
    else
    {
        for (std::filesystem::directory_iterator it(path, ec), end; it != end; it.increment(ec))
        {
            if (ec)
            {
                return dbase::Result<std::vector<std::filesystem::path>>(makeFsError("directory_iterator failed", path, ec));
            }

            if (it->is_regular_file(ec))
            {
                if (ec)
                {
                    return dbase::Result<std::vector<std::filesystem::path>>(makeFsError("is_regular_file failed", it->path(), ec));
                }

                result.emplace_back(it->path());
            }
        }
    }

    return result;
}

}  // namespace dbase::fs