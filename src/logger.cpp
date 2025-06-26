#include "logger.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <cstdlib>

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
    
    // Process any remaining log entries before shutting down
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    
    std::cout << "Logger shutdown complete" << std::endl;
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
    // Create log directory if it doesn't exist
    fs::create_directories(log_directory);
    
    while (running.load() || !log_queue.empty()) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, [&]() { return !log_queue.empty() || !running.load(); });

        while (!log_queue.empty()) {
            auto entry = log_queue.front();
            log_queue.pop();
            lock.unlock();
            
            // Check if log rotation is needed before writing
            checkLogRotation();
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

    // Use write-and-close strategy to avoid file locking issues
    std::string log_path = log_directory + "/app.log";
    std::ofstream log_file(log_path, std::ios::app);
    if (log_file.is_open()) {
        log_file << "[" << timestamp_stream.str() << "] "
                 << "[" << level_to_string(entry.level) << "] "
                 << "[" << entry.module << "] "
                 << entry.message << std::endl;
        log_file.flush();
        log_file.close(); // Immediately close after writing
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

void Logger::checkLogRotation() {
    std::string log_path = log_directory + "/app.log";
    
    if (fs::exists(log_path) && fs::file_size(log_path) > 1 * 1024 * 1024) {
        // Rotate log files
        for (int i = 4; i > 0; --i) {
            std::string old_file = log_directory + "/app.log." + std::to_string(i);
            std::string new_file = log_directory + "/app.log." + std::to_string(i + 1);
            if (fs::exists(old_file)) {
                fs::rename(old_file, new_file);
            }
        }
        fs::rename(log_path, log_directory + "/app.log.1");
    }
}

void Logger::forceCleanup() {
    Logger& logger = instance();
    logger.shutdown();
}

void init_logger(bool debug_enabled, bool debug_to_console, const std::string& log_dir) {
    Logger::instance().init(debug_enabled, debug_to_console, log_dir);
    
    // Register cleanup function to be called on normal program exit
    std::atexit(Logger::forceCleanup);
}