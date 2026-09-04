#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include "pathsUtil.hpp"

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
            if (!fs::exists(PathsUtil::LOG_DIR)) {
                fs::create_directories(PathsUtil::LOG_DIR);
            }
            std::ofstream file(PathsUtil::LOG_FILE_PATH, std::ios::app);
            if (file.is_open()) {
                file << "[" << level << "][" << module << "] " << message << std::endl;
            }
        } catch (...) {
        }
    }
};