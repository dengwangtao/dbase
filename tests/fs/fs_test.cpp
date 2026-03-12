#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "dbase/error/error.h"
#include "dbase/fs/fs.h"

namespace
{
using dbase::ErrorCode;
namespace fs = dbase::fs;

std::filesystem::path makeTempRoot()
{
    const auto root = std::filesystem::temp_directory_path() / "dbase_fs_test_root";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    return root;
}

std::vector<std::byte> makeBytes(std::initializer_list<unsigned int> values)
{
    std::vector<std::byte> out;
    out.reserve(values.size());
    for (const auto v : values)
    {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}
}  // namespace

TEST_CASE("fs exists isFile isDirectory basic behavior", "[fs]")
{
    const auto root = makeTempRoot();
    const auto dir = root / "dir";
    const auto file = dir / "a.txt";

    REQUIRE(fs::createDirectories(dir).hasValue());
    REQUIRE(fs::writeText(file, "hello").hasValue());

    REQUIRE(fs::exists(root));
    REQUIRE(fs::exists(dir));
    REQUIRE(fs::exists(file));

    REQUIRE(fs::isDirectory(root));
    REQUIRE(fs::isDirectory(dir));
    REQUIRE_FALSE(fs::isDirectory(file));

    REQUIRE(fs::isFile(file));
    REQUIRE_FALSE(fs::isFile(dir));

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs absolute returns absolute path", "[fs]")
{
    const auto result = fs::absolute(".");

    REQUIRE(result.hasValue());
    REQUIRE(result.value().is_absolute());
}

TEST_CASE("fs canonical resolves existing path", "[fs]")
{
    const auto root = makeTempRoot();
    const auto dir = root / "dir";
    const auto file = dir / "a.txt";

    REQUIRE(fs::createDirectories(dir).hasValue());
    REQUIRE(fs::writeText(file, "hello").hasValue());

    const auto result = fs::canonical(file);

    REQUIRE(result.hasValue());
    REQUIRE(result.value().is_absolute());
    REQUIRE(result.value().filename() == "a.txt");

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs canonical returns error for missing path", "[fs]")
{
    const auto missing = std::filesystem::temp_directory_path() / "dbase_fs_missing_file";

    const auto result = fs::canonical(missing);

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::IOError);
}

TEST_CASE("fs createDirectories creates nested directories", "[fs]")
{
    const auto root = makeTempRoot();
    const auto nested = root / "a" / "b" / "c";

    const auto result = fs::createDirectories(nested);

    REQUIRE(result.hasValue());
    REQUIRE(fs::exists(nested));
    REQUIRE(fs::isDirectory(nested));

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs writeText and readText round trip", "[fs]")
{
    const auto root = makeTempRoot();
    const auto file = root / "text.txt";

    REQUIRE(fs::writeText(file, "line1\nline2").hasValue());

    const auto read = fs::readText(file);
    REQUIRE(read.hasValue());
    REQUIRE(read.value() == "line1\nline2");

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs readText returns error for missing file", "[fs]")
{
    const auto file = std::filesystem::temp_directory_path() / "dbase_fs_missing_text.txt";

    const auto result = fs::readText(file);

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::IOError);
}

TEST_CASE("fs readLines splits text by lines", "[fs]")
{
    const auto root = makeTempRoot();
    const auto file = root / "lines.txt";

    REQUIRE(fs::writeText(file, "alpha\nbeta\ngamma").hasValue());

    const auto lines = fs::readLines(file);

    REQUIRE(lines.hasValue());
    REQUIRE(lines.value().size() == 3);
    REQUIRE(lines.value()[0] == "alpha");
    REQUIRE(lines.value()[1] == "beta");
    REQUIRE(lines.value()[2] == "gamma");

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs appendText appends to existing file", "[fs]")
{
    const auto root = makeTempRoot();
    const auto file = root / "append.txt";

    REQUIRE(fs::writeText(file, "hello").hasValue());
    REQUIRE(fs::appendText(file, " world").hasValue());

    const auto read = fs::readText(file);
    REQUIRE(read.hasValue());
    REQUIRE(read.value() == "hello world");

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs writeTextAtomic writes final content", "[fs]")
{
    const auto root = makeTempRoot();
    const auto file = root / "atomic.txt";

    REQUIRE(fs::writeTextAtomic(file, "atomic content").hasValue());

    const auto read = fs::readText(file);
    REQUIRE(read.hasValue());
    REQUIRE(read.value() == "atomic content");

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs writeBytes and readBytes round trip", "[fs]")
{
    const auto root = makeTempRoot();
    const auto file = root / "bytes.bin";
    const auto data = makeBytes({0x00, 0x7F, 0x80, 0xFF});

    REQUIRE(fs::writeBytes(file, data).hasValue());

    const auto read = fs::readBytes(file);
    REQUIRE(read.hasValue());
    REQUIRE(read.value().size() == data.size());
    REQUIRE(read.value() == data);

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs writeBytesAtomic writes final binary content", "[fs]")
{
    const auto root = makeTempRoot();
    const auto file = root / "bytes_atomic.bin";
    const auto data = makeBytes({1, 2, 3, 4, 5});

    REQUIRE(fs::writeBytesAtomic(file, data).hasValue());

    const auto read = fs::readBytes(file);
    REQUIRE(read.hasValue());
    REQUIRE(read.value() == data);

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs fileSize returns size for existing file", "[fs]")
{
    const auto root = makeTempRoot();
    const auto file = root / "size.txt";

    REQUIRE(fs::writeText(file, "hello").hasValue());

    const auto size = fs::fileSize(file);
    REQUIRE(size.hasValue());
    REQUIRE(size.value() == 5);

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs copyFile copies file contents", "[fs]")
{
    const auto root = makeTempRoot();
    const auto from = root / "from.txt";
    const auto to = root / "to.txt";

    REQUIRE(fs::writeText(from, "copy me").hasValue());

    const auto copy = fs::copyFile(from, to, true);
    REQUIRE(copy.hasValue());

    const auto read = fs::readText(to);
    REQUIRE(read.hasValue());
    REQUIRE(read.value() == "copy me");

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs rename moves file", "[fs]")
{
    const auto root = makeTempRoot();
    const auto from = root / "from.txt";
    const auto to = root / "renamed.txt";

    REQUIRE(fs::writeText(from, "rename me").hasValue());
    REQUIRE(fs::rename(from, to).hasValue());

    REQUIRE_FALSE(fs::exists(from));
    REQUIRE(fs::exists(to));

    const auto read = fs::readText(to);
    REQUIRE(read.hasValue());
    REQUIRE(read.value() == "rename me");

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs removeFile removes a file", "[fs]")
{
    const auto root = makeTempRoot();
    const auto file = root / "delete.txt";

    REQUIRE(fs::writeText(file, "bye").hasValue());
    REQUIRE(fs::exists(file));

    REQUIRE(fs::removeFile(file).hasValue());
    REQUIRE_FALSE(fs::exists(file));

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs removeAll removes directory tree", "[fs]")
{
    const auto root = makeTempRoot();
    const auto nested = root / "a" / "b";
    const auto file = nested / "c.txt";

    REQUIRE(fs::createDirectories(nested).hasValue());
    REQUIRE(fs::writeText(file, "hello").hasValue());
    REQUIRE(fs::exists(root));

    REQUIRE(fs::removeAll(root).hasValue());
    REQUIRE_FALSE(fs::exists(root));
}

TEST_CASE("fs listFiles lists direct children when non recursive", "[fs]")
{
    const auto root = makeTempRoot();
    const auto nested = root / "nested";
    const auto f1 = root / "a.txt";
    const auto f2 = nested / "b.txt";

    REQUIRE(fs::createDirectories(nested).hasValue());
    REQUIRE(fs::writeText(f1, "a").hasValue());
    REQUIRE(fs::writeText(f2, "b").hasValue());

    const auto files = fs::listFiles(root, false);
    REQUIRE(files.hasValue());

    bool foundA = false;
    bool foundB = false;
    for (const auto& p : files.value())
    {
        if (p.filename() == "a.txt")
        {
            foundA = true;
        }
        if (p.filename() == "b.txt")
        {
            foundB = true;
        }
    }

    REQUIRE(foundA);
    REQUIRE_FALSE(foundB);

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs listFiles lists nested children when recursive", "[fs]")
{
    const auto root = makeTempRoot();
    const auto nested = root / "nested";
    const auto f1 = root / "a.txt";
    const auto f2 = nested / "b.txt";

    REQUIRE(fs::createDirectories(nested).hasValue());
    REQUIRE(fs::writeText(f1, "a").hasValue());
    REQUIRE(fs::writeText(f2, "b").hasValue());

    const auto files = fs::listFiles(root, true);
    REQUIRE(files.hasValue());

    bool foundA = false;
    bool foundB = false;
    for (const auto& p : files.value())
    {
        if (p.filename() == "a.txt")
        {
            foundA = true;
        }
        if (p.filename() == "b.txt")
        {
            foundB = true;
        }
    }

    REQUIRE(foundA);
    REQUIRE(foundB);

    REQUIRE(fs::removeAll(root).hasValue());
}

TEST_CASE("fs operations return IOError for clearly missing sources", "[fs]")
{
    const auto root = makeTempRoot();
    const auto missing = root / "missing.txt";
    const auto dst = root / "dst.txt";

    const auto copy = fs::copyFile(missing, dst, true);
    REQUIRE(copy.hasError());
    REQUIRE(copy.error().code() == ErrorCode::IOError);

    const auto remove = fs::removeFile(missing);
    REQUIRE_FALSE(remove.hasError());

    REQUIRE(fs::removeAll(root).hasValue());
}