#pragma once

#include "dbase/config/config.h"

#include <filesystem>
#include <string_view>

namespace dbase::config
{
class JsonConfig : public Config
{
    public:
        JsonConfig() = default;

        [[nodiscard]] static dbase::Result<JsonConfig> fromFile(const std::filesystem::path& path);
        [[nodiscard]] static dbase::Result<JsonConfig> fromString(std::string_view content);

    private:
        [[nodiscard]] static dbase::Result<JsonConfig> parse(std::string_view content);
};
}  // namespace dbase::config