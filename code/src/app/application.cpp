#include "dbase/app/application.h"
#include "dbase/error/error.h"
#include "dbase/fs/fs.h"
#include <chrono>
#include <csignal>
#include <fstream>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace dbase::app
{
Application* Application::s_instance = nullptr;

namespace
{
volatile std::sig_atomic_t s_stopSignalRequested = 0;

[[nodiscard]] dbase::Result<void> makeInvalidArgument(std::string message)
{
    return dbase::makeError(dbase::ErrorCode::InvalidArgument, std::move(message));
}
}  // namespace

Application::Application(int argc, char** argv, Options options)
    : m_argc(argc),
      m_options(std::move(options))
{
    m_argv.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i)
    {
        m_argv.emplace_back(argv[i] == nullptr ? "" : argv[i]);
    }
}

Application::~Application()
{
    removePidFileIfNeeded();
    if (s_instance == this)
    {
        s_instance = nullptr;
    }
}

void Application::setStartupCallback(StartupCallback cb)
{
    m_startupCallback = std::move(cb);
}

void Application::setShutdownCallback(ShutdownCallback cb)
{
    m_shutdownCallback = std::move(cb);
}

int Application::run()
{
    if (s_instance != nullptr && s_instance != this)
    {
        return 2;
    }

    s_instance = this;
    s_stopSignalRequested = 0;
    m_stopRequested.store(false, std::memory_order_release);

    auto ret = parseCommandLine();
    if (!ret)
    {
        return 2;
    }

    ret = applyBuiltinOptions();
    if (!ret)
    {
        return 2;
    }

    ret = prepareWorkingDirectory();
    if (!ret)
    {
        return 3;
    }

    ret = createPidFileIfNeeded();
    if (!ret)
    {
        return 4;
    }

    ret = installSignalHandlers();
    if (!ret)
    {
        return 5;
    }

    if (m_startupCallback)
    {
        ret = m_startupCallback(*this);
        if (!ret)
        {
            return 6;
        }
    }

    for (;;)
    {
        {
            std::unique_lock<std::mutex> lock(m_stopMutex);
            m_stopCv.wait_for(
                    lock,
                    std::chrono::milliseconds(200),
                    [this]()
                    {
                        return m_stopRequested.load(std::memory_order_acquire);
                    });
        }

        if (m_stopRequested.load(std::memory_order_acquire))
        {
            break;
        }

        if (s_stopSignalRequested != 0)
        {
            requestStop();
            break;
        }
    }

    if (m_shutdownCallback)
    {
        m_shutdownCallback(*this);
    }

    return 0;
}

void Application::requestStop() noexcept
{
    const bool alreadyStopped = m_stopRequested.exchange(true, std::memory_order_acq_rel);
    if (!alreadyStopped)
    {
        m_stopCv.notify_all();
    }
}

bool Application::stopRequested() const noexcept
{
    return m_stopRequested.load(std::memory_order_acquire);
}

int Application::argc() const noexcept
{
    return m_argc;
}

const std::vector<std::string>& Application::argv() const noexcept
{
    return m_argv;
}

const Application::Options& Application::options() const noexcept
{
    return m_options;
}

const std::string& Application::name() const noexcept
{
    return m_options.name;
}

const std::string& Application::version() const noexcept
{
    return m_options.version;
}

const std::filesystem::path& Application::workingDirectory() const noexcept
{
    return m_options.workingDirectory;
}

const std::filesystem::path& Application::pidFile() const noexcept
{
    return m_options.pidFile;
}

const std::filesystem::path& Application::configFile() const noexcept
{
    return m_options.configFile;
}

bool Application::hasFlag(std::string_view name) const
{
    for (const auto& flag : m_cliFlags)
    {
        if (flag == name)
        {
            return true;
        }
    }
    return false;
}

bool Application::hasOption(std::string_view name) const
{
    return m_cliOptions.find(std::string(name)) != m_cliOptions.end();
}

std::string Application::getOption(std::string_view name, std::string defaultValue) const
{
    const auto it = m_cliOptions.find(std::string(name));
    if (it == m_cliOptions.end())
    {
        return defaultValue;
    }
    return it->second;
}

const std::unordered_map<std::string, std::string>& Application::cliOptions() const noexcept
{
    return m_cliOptions;
}

const std::vector<std::string>& Application::cliFlags() const noexcept
{
    return m_cliFlags;
}

const std::vector<std::string>& Application::positionalArgs() const noexcept
{
    return m_positionalArgs;
}

Application* Application::instance() noexcept
{
    return s_instance;
}

dbase::Result<void> Application::parseCommandLine()
{
    for (int i = 1; i < m_argc; ++i)
    {
        const std::string arg = m_argv[static_cast<std::size_t>(i)];
        if (arg.rfind("--", 0) == 0)
        {
            const auto eqPos = arg.find('=');
            if (eqPos == std::string::npos)
            {
                m_cliFlags.emplace_back(arg.substr(2));
            }
            else
            {
                const auto key = arg.substr(2, eqPos - 2);
                const auto value = arg.substr(eqPos + 1);
                if (key.empty())
                {
                    return makeInvalidArgument("empty cli option name");
                }
                m_cliOptions[key] = value;
            }
            continue;
        }

        m_positionalArgs.emplace_back(arg);
    }

    return {};
}

dbase::Result<void> Application::applyBuiltinOptions()
{
    if (hasOption("config"))
    {
        m_options.configFile = getOption("config");
    }
    if (hasOption("workdir"))
    {
        m_options.workingDirectory = getOption("workdir");
    }
    if (hasOption("pid-file"))
    {
        m_options.pidFile = getOption("pid-file");
        m_options.createPidFile = true;
    }
    if (hasOption("name"))
    {
        m_options.name = getOption("name");
    }
    if (hasOption("version"))
    {
        m_options.version = getOption("version");
    }

    if (m_options.name.empty())
    {
        if (!m_argv.empty())
        {
            m_options.name = std::filesystem::path(m_argv.front()).stem().string();
        }
        if (m_options.name.empty())
        {
            m_options.name = "application";
        }
    }

    return {};
}

dbase::Result<void> Application::prepareWorkingDirectory()
{
    if (m_options.workingDirectory.empty())
    {
        return {};
    }

    auto createRet = dbase::fs::createDirectories(m_options.workingDirectory);
    if (!createRet)
    {
        return createRet;
    }

    std::error_code ec;
    std::filesystem::current_path(m_options.workingDirectory, ec);
    if (ec)
    {
        return dbase::makeError(
                dbase::ErrorCode::IOError,
                "set current_path failed: " + m_options.workingDirectory.string() + ": " + ec.message());
    }

    return {};
}

dbase::Result<void> Application::createPidFileIfNeeded()
{
    if (!m_options.createPidFile || m_options.pidFile.empty())
    {
        return {};
    }

    auto ensureRet = dbase::fs::createDirectories(m_options.pidFile.parent_path());
    if (!ensureRet)
    {
        return ensureRet;
    }

    std::ofstream ofs(m_options.pidFile, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
    {
        return dbase::makeError(
                dbase::ErrorCode::IOError,
                "open pid file failed: " + m_options.pidFile.string());
    }

#if defined(_WIN32)
    ofs << static_cast<unsigned long>(::GetCurrentProcessId()) << '\n';
#else
    ofs << static_cast<long>(::getpid()) << '\n';
#endif

    ofs.flush();
    if (!ofs.good())
    {
        return dbase::makeError(
                dbase::ErrorCode::IOError,
                "write pid file failed: " + m_options.pidFile.string());
    }

    m_pidFileCreated = true;
    return {};
}

void Application::removePidFileIfNeeded() noexcept
{
    if (!m_pidFileCreated || m_options.pidFile.empty())
    {
        return;
    }

    (void)dbase::fs::removeFile(m_options.pidFile);
    m_pidFileCreated = false;
}

dbase::Result<void> Application::installSignalHandlers()
{
    if (!m_options.handleSignals)
    {
        return {};
    }

    std::signal(SIGINT, &Application::onSignal);
    std::signal(SIGTERM, &Application::onSignal);
    return {};
}

void Application::onSignal(int signal) noexcept
{
    s_stopSignalRequested = signal;
}
}  // namespace dbase::app