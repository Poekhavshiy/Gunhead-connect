#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <unordered_set>
#include <atomic>
#include <chrono>

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

// Overload operator<< for LogLevel
inline std::ostream& operator<<(std::ostream& os, LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: os << "TRACE"; break;
        case LogLevel::DEBUG: os << "DEBUG"; break;
        case LogLevel::INFO: os << "INFO"; break;
        case LogLevel::WARNING: os << "WARNING"; break;
        case LogLevel::ERROR: os << "ERROR"; break;
        default: os << "UNKNOWN"; break;
    }
    return os;
}

struct LogEntry {
    LogLevel level;
    std::string module;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

class Logger {
public:
    static Logger& instance();

    void init(bool debug_enabled = false, bool to_console = false, const std::string& log_dir = "data/");
    void shutdown();
    
    // Force cleanup - can be called from signal handlers or atexit
    static void forceCleanup();

    void log(LogLevel level, const std::string& module, const std::string& message);
    void enable_module(const std::string& module);
    void disable_module(const std::string& module);
    void setModuleFilter(const std::string& moduleName);

    template<typename... Args>
    static std::string concat_message(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << args);
        return oss.str();
    }

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void worker();
    std::string level_to_string(LogLevel level);
    void checkLogRotation();
    void writeLog(const LogEntry& entry);

    bool debug_enabled = false;
    bool console_output = false;
    std::string log_directory;

    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::queue<LogEntry> log_queue;
    std::thread worker_thread;
    std::atomic<bool> running{false};

    std::mutex filter_mutex;
    std::string module_filter;

    std::unordered_set<std::string> enabled_modules;
};

// Level-specific helper macros or inline functions
template<typename... Args>
void log_error(const std::string& module, Args&&... args) {
    Logger::instance().log(LogLevel::ERROR, module, Logger::concat_message(std::forward<Args>(args)...));
}

template<typename... Args>
void log_warn(const std::string& module, Args&&... args) {
    Logger::instance().log(LogLevel::WARNING, module, Logger::concat_message(std::forward<Args>(args)...));
}

template<typename... Args>
void log_info(const std::string& module, Args&&... args) {
    Logger::instance().log(LogLevel::INFO, module, Logger::concat_message(std::forward<Args>(args)...));
}

template<typename... Args>
void log_debug(const std::string& module, Args&&... args) {
    Logger::instance().log(LogLevel::DEBUG, module, Logger::concat_message(std::forward<Args>(args)...));
}

template<typename... Args>
void log_trace(const std::string& module, Args&&... args) {
    Logger::instance().log(LogLevel::TRACE, module, Logger::concat_message(std::forward<Args>(args)...));
}

void init_logger(bool debug_enabled, bool to_console, const std::string& log_dir = "data/");