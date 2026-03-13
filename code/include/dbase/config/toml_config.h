#pragma once

#include "dbase/config/config.h"

#include <filesystem>
#include <string_view>

namespace dbase::config
{
class TomlConfig : public Config
{
    public:
        TomlConfig() = default;

        [[nodiscard]] static dbase::Result<TomlConfig> fromFile(const std::filesystem::path& path);
        [[nodiscard]] static dbase::Result<TomlConfig> fromString(std::string_view content);

    private:
        [[nodiscard]] static dbase::Result<TomlConfig> parse(std::string_view content);
};
}  // namespace dbase::config