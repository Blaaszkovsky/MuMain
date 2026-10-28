#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "Core/Platform/WinCompat.h"
#include "Core/Platform/SecureCrt.h"
#include "Core/Platform/WinApiShims.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Core/Utilities/Log/SdlLogBridge.h"

#include <SDL3/SDL.h>

std::shared_ptr<spdlog::logger> GetEarlyLogger();

namespace
{
std::filesystem::path TestDirectory(const char* name)
{
    return std::filesystem::temp_directory_path() /
           (std::string(name) + "_" + std::to_string(GetCurrentProcessId()));
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

class StdoutCapture
{
public:
    StdoutCapture()
        : m_capture(std::tmpfile())
    {
        REQUIRE(m_capture != nullptr);
        std::fflush(stdout);
#ifdef _WIN32
        m_stdoutCopy = _dup(_fileno(stdout));
        REQUIRE(m_stdoutCopy != -1);
        REQUIRE(_dup2(_fileno(m_capture), _fileno(stdout)) == 0);
#else
        m_stdoutCopy = dup(fileno(stdout));
        REQUIRE(m_stdoutCopy != -1);
        REQUIRE(dup2(fileno(m_capture), fileno(stdout)) != -1);
#endif
    }

    ~StdoutCapture()
    {
        std::fflush(stdout);
#ifdef _WIN32
        _dup2(m_stdoutCopy, _fileno(stdout));
        _close(m_stdoutCopy);
#else
        dup2(m_stdoutCopy, fileno(stdout));
        close(m_stdoutCopy);
#endif
        std::fclose(m_capture);
    }

    std::string Read() const
    {
        std::fflush(stdout);
        std::fseek(m_capture, 0, SEEK_END);
        const long size = std::ftell(m_capture);
        std::rewind(m_capture);
        std::string output(static_cast<std::size_t>(size), '\0');
        if (!output.empty())
        {
            std::fread(output.data(), 1, output.size(), m_capture);
        }
        return output;
    }

private:
    FILE* m_capture = nullptr;
    int m_stdoutCopy = -1;
};
} // namespace

TEST_CASE("early logger adopts explicit log directory")
{
    const auto directory = TestDirectory("mu_logger_test");
    const auto ignoredDirectory = TestDirectory("mu_logger_test_ignored");
    std::filesystem::remove_all(directory);
    std::filesystem::remove_all(ignoredDirectory);

    const auto early = GetEarlyLogger();
    REQUIRE(early != nullptr);
    mu::log::Init(directory);
    mu::log::Init(ignoredDirectory);
    const auto core = mu::log::Get("core");
    const auto network = mu::log::Get("network");
    CHECK(core != nullptr);
    CHECK(network != nullptr);
    CHECK(core == early);
    CHECK(core != network);
    MU_LOG_ERROR(early, "mu logger early-init self-check");
    early->flush();
    mu::log::Shutdown();

    const std::string logContents = ReadFile(directory / "MuError.log");
    CHECK(logContents.find("mu logger early-init self-check") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(ignoredDirectory));
    std::filesystem::remove_all(directory);
}

TEST_CASE("logger shutdown allows explicit reinitialization")
{
    const auto firstDirectory = TestDirectory("mu_logger_reinit_first");
    const auto secondDirectory = TestDirectory("mu_logger_reinit_second");
    std::filesystem::remove_all(firstDirectory);
    std::filesystem::remove_all(secondDirectory);

    mu::log::Init(firstDirectory);
    mu::log::Shutdown();
    mu::log::Init(secondDirectory);
    const auto logger = mu::log::Get("core");
    REQUIRE(logger != nullptr);
    MU_LOG_ERROR(logger, "mu logger reinit self-check");
    logger->flush();
    mu::log::Shutdown();

    const std::string logContents = ReadFile(secondDirectory / "MuError.log");
    CHECK(logContents.find("mu logger reinit self-check") != std::string::npos);
    std::filesystem::remove_all(firstDirectory);
    std::filesystem::remove_all(secondDirectory);
}

TEST_CASE("logger writes to MuError.log without console output")
{
    const auto directory = TestDirectory("mu_logger_file_only");
    std::filesystem::remove_all(directory);

    std::string consoleOutput;
    {
        StdoutCapture capture;
        mu::log::Init(directory);
        const auto logger = mu::log::Get("core");
        MU_LOG_ERROR(logger, "file-only logger self-check");
        logger->flush();
        mu::log::Shutdown();
        consoleOutput = capture.Read();
    }

    const std::string logContents = ReadFile(directory / "MuError.log");
    CHECK(logContents.find("file-only logger self-check") != std::string::npos);
    CHECK(consoleOutput.empty());
    std::filesystem::remove_all(directory);
}

TEST_CASE("SDL bridge writes named diagnostics to MuError.log")
{
    const auto directory = TestDirectory("sdl_log_bridge");
    std::filesystem::remove_all(directory);

    mu::log::Init(directory);
    {
        SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_INFO);
        Core::Log::Sdl::ScopedLogOutput logOutput;
        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "SDL log bridge self-check");
        {
            Core::Log::Sdl::ScopedLogOutput nested;
        }
        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "SDL log bridge survives a nested guard");
    }
    mu::log::Shutdown();

    SDL_ResetLogPriorities();
    const std::string logContents = ReadFile(directory / "MuError.log");
    CHECK(logContents.find("[sdl] [info] video: SDL log bridge self-check") != std::string::npos);
    CHECK(logContents.find("[sdl] [info] video: SDL log bridge survives a nested guard") != std::string::npos);
    std::filesystem::remove_all(directory);
}

TEST_CASE("rotating logger retains recent records")
{
    const char* sharedDirectory = std::getenv("MU_TEST_LOG_DIRECTORY");
    const auto directory = sharedDirectory ? std::filesystem::path(sharedDirectory) : TestDirectory("mu_logger_rotate");
    if (!sharedDirectory)
        std::filesystem::remove_all(directory);
    mu::log::Init(directory);
    // The multiprocess regression runner starts peers during this ownership window.
    if (sharedDirectory)
        std::this_thread::sleep_for(std::chrono::seconds(2));
    const auto logger = mu::log::Get("core");
    int errors = 0;
    logger->set_error_handler([&](const std::string&) { ++errors; });
    const std::string payload(100000, 'x');
    for (int record = 0; record < 220; ++record)
        logger->info("writer={} record={} {}", GetCurrentProcessId(), record, payload);
    logger->info("rotation-complete={}", GetCurrentProcessId());
    mu::log::Shutdown();
    CHECK(errors == 0);
    if (!sharedDirectory)
    {
        CHECK(ReadFile(directory / "MuError.log").find("rotation-complete=") != std::string::npos);
        CHECK(std::filesystem::exists(directory / "MuError.3.log"));
        CHECK_FALSE(std::filesystem::exists(directory / "MuError.4.log"));
        std::filesystem::remove_all(directory);
    }
}
