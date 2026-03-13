#pragma once

#include "dbase/config/config.h"

#include <filesystem>
#include <string_view>

namespace dbase::config
{
class XmlConfig : public Config
{
    public:
        XmlConfig() = default;

        [[nodiscard]] static dbase::Result<XmlConfig> fromFile(const std::filesystem::path& path);
        [[nodiscard]] static dbase::Result<XmlConfig> fromString(std::string_view content);

    private:
        [[nodiscard]] static dbase::Result<XmlConfig> parse(std::string_view content);
};
}  // namespace dbase::config