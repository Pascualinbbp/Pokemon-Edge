#pragma once
#include <iostream>
#include <string>

class Logger {
public:
    static void logInfo(const std::string& module, const std::string& message) {
        std::cout << "[INFO][" << module << "] " << message << std::endl;
    }

    static void logError(const std::string& module, const std::string& message) {
        std::cerr << "[ERROR][" << module << "] " << message << std::endl;
    }
};