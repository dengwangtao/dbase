#include "dbase/app/application.h"
#include "dbase/log/log.h"

#include <chrono>
#include <thread>

int main(int argc, char** argv)
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::app::Application::Options options;
    options.name = "dbase-app-example";
    options.version = "0.1.0";
    options.workingDirectory = "tmp/app_example";
    options.pidFile = "tmp/app_example/app.pid";
    options.createPidFile = true;
    options.handleSignals = true;

    dbase::app::Application app(argc, argv, std::move(options));

    app.setStartupCallback([](dbase::app::Application& self) -> dbase::Result<void>
                           {
        DBASE_LOG_INFO("app start");
        DBASE_LOG_INFO("name={}", self.name());
        DBASE_LOG_INFO("version={}", self.version());
        DBASE_LOG_INFO("workdir={}", self.workingDirectory().string());
        DBASE_LOG_INFO("pidFile={}", self.pidFile().string());
        DBASE_LOG_INFO("configFile={}", self.configFile().string());

        for (const auto& [k, v] : self.cliOptions())
        {
            DBASE_LOG_INFO("cli option {}={}", k, v);
        }

        for (const auto& flag : self.cliFlags())
        {
            DBASE_LOG_INFO("cli flag {}", flag);
        }

        for (const auto& arg : self.positionalArgs())
        {
            DBASE_LOG_INFO("positional {}", arg);
        }

        std::thread worker([&self]() {
            for (int i = 0; i < 20 && !self.stopRequested(); ++i)
            {
                DBASE_LOG_INFO("worker tick {}", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            self.requestStop();
        });
        worker.detach();

        return {}; });

    app.setShutdownCallback([](dbase::app::Application&)
                            { DBASE_LOG_INFO("app shutdown"); });

    return app.run();
}