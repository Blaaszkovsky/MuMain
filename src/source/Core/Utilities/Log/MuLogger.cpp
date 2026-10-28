#include "stdafx.h"

#include "MuLogger.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#ifdef _WIN32
#include <share.h>
#else
#include <sys/file.h>
#endif

namespace mu::log
{
namespace
{
constexpr std::array<const char*, 11> kLoggerNames = {"core",  "network",  "render", "data",     "gameplay", "ui",
                                                      "audio", "platform", "dotnet", "gameshop", "scenes"};
constexpr size_t MaxLogBytes = 5 * 1024 * 1024;
constexpr size_t LogHistoryCount = 3;
constexpr int MaxConcurrentLogs = 64;
using FileLock = std::unique_ptr<FILE, decltype(&std::fclose)>;

FileLock LockLogFile(const std::filesystem::path& path)
{
#ifdef _WIN32
    return FileLock(_wfsopen(path.c_str(), L"a", _SH_DENYRW), &std::fclose);
#else
    FileLock file(std::fopen(path.c_str(), "a"), &std::fclose);
    if (file && flock(fileno(file.get()), LOCK_EX | LOCK_NB) != 0)
    {
        const int error = errno;
        file.reset();
        errno = error;
    }
    return file;
#endif
}

struct LoggerState
{
    std::mutex mutex;
    FileLock fileLock{nullptr, &std::fclose};
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers;
    std::shared_ptr<spdlog::sinks::dist_sink_mt> sink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    std::filesystem::path directory;
    bool sinksInitialized = false;
    bool explicitlyInitialized = false;
};

LoggerState& State()
{
    static LoggerState state;
    return state;
}

std::shared_ptr<spdlog::logger> CreateLogger(LoggerState& state, const std::string& name)
{
    auto logger = std::make_shared<spdlog::logger>(name, state.sink);
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::err);
    spdlog::register_logger(logger);
    state.loggers.emplace(name, logger);
    return logger;
}

void ConfigureSinks(LoggerState& state, const std::filesystem::path& directory)
{
    const auto absoluteDirectory = std::filesystem::absolute(directory).lexically_normal();
    if (state.sinksInitialized && state.directory == absoluteDirectory)
        return;

    // A process owns its rotation set for the sink's lifetime; OS locks also release after a crash.
    for (int slot = 0; slot < MaxConcurrentLogs; ++slot)
    {
        const std::string stem = slot == 0 ? "MuError" : "MuError-" + std::to_string(slot);
        auto fileLock = LockLogFile(absoluteDirectory / (stem + ".lock"));
        if (!fileLock)
        {
            if (errno == EACCES || errno == EAGAIN)
                continue;
            throw spdlog::spdlog_ex("Cannot lock client log", errno);
        }
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (absoluteDirectory / (stem + ".log")).string(), MaxLogBytes, LogHistoryCount));
        state.sink->flush();
        state.sink->set_sinks(std::move(sinks));
        state.fileLock = std::move(fileLock);
        state.directory = absoluteDirectory;
        state.sinksInitialized = true;
        return;
    }
    throw spdlog::spdlog_ex("All client log rotation sets are in use");
}
} // namespace

void Init(const std::filesystem::path& logDirectory)
{
    LoggerState& state = State();
    std::lock_guard lock(state.mutex);
    if (state.explicitlyInitialized)
        return;

    const std::filesystem::path directory = logDirectory.empty() ? std::filesystem::current_path() : logDirectory;
    std::error_code error;
    std::filesystem::create_directories(directory, error);

    ConfigureSinks(state, directory);

    for (const char* name : kLoggerNames)
    {
        if (!state.loggers.contains(name))
            CreateLogger(state, name);
    }

    state.explicitlyInitialized = true;
}

void Shutdown()
{
    LoggerState& state = State();
    std::lock_guard lock(state.mutex);
    for (auto& entry : state.loggers)
        entry.second->flush();
    state.loggers.clear();
    state.sink->set_sinks({});
    state.fileLock.reset();
    state.directory.clear();
    spdlog::shutdown();
    state.sinksInitialized = false;
    state.explicitlyInitialized = false;
}

std::shared_ptr<spdlog::logger> Get(const std::string& name)
{
    LoggerState& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.sinksInitialized)
        ConfigureSinks(state, std::filesystem::current_path());

    const auto found = state.loggers.find(name);
    return found != state.loggers.end() ? found->second : CreateLogger(state, name);
}
} // namespace mu::log
