#include "dbase/fs/fs.h"
#include "dbase/log/log.h"
#include "dbase/thread/current_thread.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    const std::filesystem::path root = "tmp/fs_example";
    const auto textFile = root / "a.txt";
    const auto textFile2 = root / "b.txt";
    const auto binFile = root / "data.bin";

    auto ret = dbase::fs::createDirectories(root);
    if (!ret)
    {
        DBASE_LOG_ERROR("createDirectories failed: {}", ret.error().toString());
        return 1;
    }

    ret = dbase::fs::writeText(textFile, "hello\nworld\n");
    if (!ret)
    {
        DBASE_LOG_ERROR("writeText failed: {}", ret.error().toString());
        return 1;
    }

    ret = dbase::fs::appendText(textFile, "append-line\n");
    if (!ret)
    {
        DBASE_LOG_ERROR("appendText failed: {}", ret.error().toString());
        return 1;
    }

    auto textRet = dbase::fs::readText(textFile);
    if (!textRet)
    {
        DBASE_LOG_ERROR("readText failed: {}", textRet.error().toString());
        return 1;
    }
    DBASE_LOG_INFO("text content:\n{}", textRet.value());

    auto linesRet = dbase::fs::readLines(textFile);
    if (!linesRet)
    {
        DBASE_LOG_ERROR("readLines failed: {}", linesRet.error().toString());
        return 1;
    }
    DBASE_LOG_INFO("line count={}", linesRet.value().size());

    std::vector<std::byte> bytes;
    for (std::uint8_t i = 0; i < 16; ++i)
    {
        bytes.emplace_back(static_cast<std::byte>(i));
    }

    ret = dbase::fs::writeBytesAtomic(binFile, bytes);
    if (!ret)
    {
        DBASE_LOG_ERROR("writeBytesAtomic failed: {}", ret.error().toString());
        return 1;
    }

    auto bytesRet = dbase::fs::readBytes(binFile);
    if (!bytesRet)
    {
        DBASE_LOG_ERROR("readBytes failed: {}", bytesRet.error().toString());
        return 1;
    }
    DBASE_LOG_INFO("bytes size={}", bytesRet.value().size());

    ret = dbase::fs::copyFile(textFile, textFile2, true);
    if (!ret)
    {
        DBASE_LOG_ERROR("copyFile failed: {}", ret.error().toString());
        return 1;
    }

    auto listRet = dbase::fs::listFiles(root, true);
    if (!listRet)
    {
        DBASE_LOG_ERROR("listFiles failed: {}", listRet.error().toString());
        return 1;
    }

    for (const auto& p : listRet.value())
    {
        DBASE_LOG_INFO("file={}", p.string());
    }

    auto sizeRet = dbase::fs::fileSize(textFile);
    if (!sizeRet)
    {
        DBASE_LOG_ERROR("fileSize failed: {}", sizeRet.error().toString());
        return 1;
    }
    DBASE_LOG_INFO("fileSize={} bytes", sizeRet.value());


    dbase::thread::current_thread::sleepForMs(4000);

    ret = dbase::fs::removeAll(root);
    if (!ret)
    {
        DBASE_LOG_ERROR("removeAll failed: {}", ret.error().toString());
        return 1;
    }

    return 0;
}