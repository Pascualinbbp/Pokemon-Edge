#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "paths_util.hpp"

class Logger {
public:
    static void logInfo(const std::string& module, const std::string& message) {
        writeLog("INFO", module, message);
    }

    static void logError(const std::string& module, const std::string& message) {
        writeLog("ERROR", module, message);
    }

private:
    static void writeLog(const std::string& level, const std::string& module, const std::string& message) {
        if (level == "ERROR") {
            std::cerr << "[" << level << "][" << module << "] " << message << std::endl;
        } else {
            std::cout << "[" << level << "][" << module << "] " << message << std::endl;
        }

        try {
            fs::path logDir = PathsUtil::getBaseDir() / "app" / "logs";
            if (!fs::exists(logDir)) {
                fs::create_directories(logDir);
            }
            fs::path logFile = logDir / "app.log";
            std::ofstream file(logFile, std::ios::app);
            if (file.is_open()) {
                file << "[" << level << "][" << module << "] " << message << std::endl;
            }
        } catch (...) {
        }
    }
};