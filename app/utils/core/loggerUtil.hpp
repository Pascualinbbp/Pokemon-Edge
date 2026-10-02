#pragma once
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include "pathsUtil.hpp"

class Logger {
    public:
    static void logInfo(std::string_view module, std::string_view message) {
        write(false, "INFO", module, message);
    }
    
    static void logError(std::string_view module, std::string_view message) {
        write(true, "ERROR", module, message);
    }
    
    private:
    // El fichero se abre una sola vez (antes se abría y cerraba en cada línea).
    static std::ofstream& logFile() {
        static std::ofstream file = [] {
            std::error_code ec;
            fs::create_directories(PathsUtil::LOG_DIR, ec);
            return std::ofstream(PathsUtil::LOG_FILE_PATH, std::ios::app);
        }();
        return file;
    }
    
    static void write(bool isError, std::string_view level, std::string_view module, std::string_view message) {
        static std::mutex mutex;
        
        std::string line;
        line.reserve(level.size() + module.size() + message.size() + 6);
        line.append("[").append(level).append("][").append(module).append("] ").append(message).append("\n");
        
        std::lock_guard<std::mutex> lock(mutex);
        (isError ? std::cerr : std::cout) << line;
        if (std::ofstream& file = logFile(); file.is_open()) {
            file << line << std::flush;
        }
    }
};