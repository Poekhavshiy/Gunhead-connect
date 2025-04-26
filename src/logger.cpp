#include "logger.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#undef ERROR // Avoid conflict with LogLevel::ERROR
#endif

namespace fs = std::filesystem;

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() = default;

Logger::~Logger() {
    shutdown();
}

void Logger::init(bool debug_enabled, bool to_console, const std::string& log_dir) {
    this->debug_enabled = debug_enabled;
    this->console_output = to_console;
    this->log_directory = log_dir;
    running.store(true);
    worker_thread = std::thread(&Logger::worker, this);
}

void Logger::shutdown() {
    running.store(false);
    queue_cv.notify_all();
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    if (logfile.is_open()) {
        logfile.close();
    }
}

void Logger::setModuleFilter(const std::string& moduleName) {
    std::lock_guard<std::mutex> lock(filter_mutex);
    module_filter = moduleName;
}

void Logger::log(LogLevel level, const std::string& module, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(filter_mutex);
        if (!module_filter.empty() && module != module_filter)
            return;
    }

    if (!debug_enabled && level == LogLevel::DEBUG)
        return;

    auto now = std::chrono::system_clock::now();
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        log_queue.push({level, module, message, now});
    }
    queue_cv.notify_one();
}

void Logger::worker() {
    ensureLogFile();

    while (running.load() || !log_queue.empty()) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, [&]() { return !log_queue.empty() || !running.load(); });

        while (!log_queue.empty()) {
            auto entry = log_queue.front();
            log_queue.pop();
            lock.unlock();
            writeLog(entry);
            lock.lock();
        }
    }
}

void Logger::writeLog(const LogEntry& entry) {
    std::ostringstream timestamp_stream;
    auto now_c = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm* tm_local = std::localtime(&now_c);
    timestamp_stream << std::put_time(tm_local, "%Y-%m-%d %H:%M:%S");

    if (console_output) {
        std::cout << "[" << timestamp_stream.str() << "] "
                  << "[" << level_to_string(entry.level) << "] "
                  << "[" << entry.module << "] "
                  << entry.message << std::endl;
    }

    if (logfile.is_open()) {
        logfile << "[" << timestamp_stream.str() << "] "
                << "[" << level_to_string(entry.level) << "] "
                << "[" << entry.module << "] "
                << entry.message << std::endl;
    }
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::ensureLogFile() {
    fs::create_directories(log_directory);
    std::string log_path = log_directory + "/app.log";

    if (fs::exists(log_path) && fs::file_size(log_path) > 1 * 1024 * 1024) {
        for (int i = 4; i > 0; --i) {
            std::string old_file = log_directory + "/app.log." + std::to_string(i);
            std::string new_file = log_directory + "/app.log." + std::to_string(i + 1);
            if (fs::exists(old_file)) {
                fs::rename(old_file, new_file);
            }
        }
        fs::rename(log_path, log_directory + "/app.log.1");
    }

    logfile.open(log_path, std::ios::app);
}

void init_logger(bool debug_enabled, bool debug_to_console, const std::string& log_dir) {
    Logger::instance().init(debug_enabled, debug_to_console, log_dir);
}