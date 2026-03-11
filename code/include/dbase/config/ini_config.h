#pragma once

#include "dbase/config/config.h"

#include <filesystem>
#include <string_view>

namespace dbase::config
{
class IniConfig : public Config
{
    public:
        IniConfig() = default;

        [[nodiscard]] static dbase::Result<IniConfig> fromFile(const std::filesystem::path& path);
        [[nodiscard]] static dbase::Result<IniConfig> fromString(std::string_view content);

    private:
        [[nodiscard]] static dbase::Result<IniConfig> parse(std::string_view content);
};
}  // namespace dbase::config