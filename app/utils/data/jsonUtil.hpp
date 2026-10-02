#pragma once
#include <fstream>
#include <string>
#include "../nlohmann/json.hpp"
#include "../core/pathsUtil.hpp"
#include "../core/loggerUtil.hpp"

using json = nlohmann::json;

class JsonUtil {
public:
    static json loadFromFile(const fs::path& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            Logger::logError("JSON_UTIL", "No se pudo abrir el archivo JSON: " + filePath.string());
            return json{};
        }
        try {
            return json::parse(file);
        } catch (const std::exception& e) {
            Logger::logError("JSON_UTIL", std::string("Error al parsear JSON desde archivo: ") + e.what());
            return json{};
        }
    }

    static bool saveToFile(const fs::path& filePath, const json& j, int indent = 4) {
        try {
            if (filePath.has_parent_path()) {
                std::error_code ec;
                fs::create_directories(filePath.parent_path(), ec);
            }
            std::ofstream file(filePath);
            if (!file.is_open()) {
                Logger::logError("JSON_UTIL", "No se pudo abrir el archivo para escribir JSON: " + filePath.string());
                return false;
            }
            file << j.dump(indent);
            return file.good();
        } catch (const std::exception& e) {
            Logger::logError("JSON_UTIL", std::string("Error al guardar JSON en archivo: ") + e.what());
            return false;
        }
    }

    static json parseString(const std::string& content) {
        try {
            return json::parse(content);
        } catch (const std::exception& e) {
            Logger::logError("JSON_UTIL", std::string("Error al parsear cadena JSON: ") + e.what());
            return json{};
        }
    }
};