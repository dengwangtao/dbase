#pragma once
#include "dbase/error/error.h"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dbase::app
{
class Application
{
    public:
        struct Options
        {
                std::string name;
                std::string version;
                std::filesystem::path workingDirectory;
                std::filesystem::path pidFile;
                std::filesystem::path configFile;
                bool handleSignals{true};
                bool createPidFile{false};
        };

        using StartupCallback = std::function<dbase::Result<void>(Application&)>;
        using ShutdownCallback = std::function<void(Application&)>;

        Application(int argc, char** argv, Options options);
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        ~Application();

        void setStartupCallback(StartupCallback cb);
        void setShutdownCallback(ShutdownCallback cb);

        [[nodiscard]] int run();
        void requestStop() noexcept;
        [[nodiscard]] bool stopRequested() const noexcept;

        [[nodiscard]] int argc() const noexcept;
        [[nodiscard]] const std::vector<std::string>& argv() const noexcept;
        [[nodiscard]] const Options& options() const noexcept;
        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] const std::string& version() const noexcept;
        [[nodiscard]] const std::filesystem::path& workingDirectory() const noexcept;
        [[nodiscard]] const std::filesystem::path& pidFile() const noexcept;
        [[nodiscard]] const std::filesystem::path& configFile() const noexcept;

        [[nodiscard]] bool hasFlag(std::string_view name) const;
        [[nodiscard]] bool hasOption(std::string_view name) const;
        [[nodiscard]] std::string getOption(std::string_view name, std::string defaultValue = {}) const;

        [[nodiscard]] const std::unordered_map<std::string, std::string>& cliOptions() const noexcept;
        [[nodiscard]] const std::vector<std::string>& cliFlags() const noexcept;
        [[nodiscard]] const std::vector<std::string>& positionalArgs() const noexcept;

        static Application* instance() noexcept;

    private:
        [[nodiscard]] dbase::Result<void> parseCommandLine();
        [[nodiscard]] dbase::Result<void> applyBuiltinOptions();
        [[nodiscard]] dbase::Result<void> prepareWorkingDirectory();
        [[nodiscard]] dbase::Result<void> createPidFileIfNeeded();
        void removePidFileIfNeeded() noexcept;
        [[nodiscard]] dbase::Result<void> installSignalHandlers();
        static void onSignal(int signal) noexcept;

    private:
        int m_argc{0};
        std::vector<std::string> m_argv;
        Options m_options;
        std::unordered_map<std::string, std::string> m_cliOptions;  // --option=value
        std::vector<std::string> m_cliFlags;                        // --flag
        std::vector<std::string> m_positionalArgs;                  // a b c -d
        StartupCallback m_startupCallback;
        ShutdownCallback m_shutdownCallback;

        std::atomic<bool> m_stopRequested{false};
        mutable std::mutex m_stopMutex;
        std::condition_variable m_stopCv;

        bool m_pidFileCreated{false};

        static Application* s_instance;
};
}  // namespace dbase::app