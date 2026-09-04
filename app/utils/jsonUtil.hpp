#pragma once
#include <fstream>
#include <string>
#include <filesystem>
#include <iomanip>
#include "nlohmann/json.hpp"
#include "loggerUtil.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class JsonUtil {
public:
    // Carga un archivo JSON de forma segura y devuelve el objeto json
    static json loadFromFile(const fs::path& filePath) {
        try {
            if (!fs::exists(filePath)) {
                Logger::logError("JSON_UTIL", "El archivo JSON no existe: " + filePath.string());
                return json{};
            }
            std::ifstream file(filePath);
            if (!file.is_open()) {
                Logger::logError("JSON_UTIL", "No se pudo abrir el archivo JSON: " + filePath.string());
                return json{};
            }
            json j;
            file >> j;
            return j;
        } catch (const std::exception& e) {
            Logger::logError("JSON_UTIL", std::string("Error al parsear JSON desde archivo: ") + e.what());
            return json{};
        }
    }

    // Guarda un objeto json en un archivo de forma segura, creando directorios si no existen
    static bool saveToFile(const fs::path& filePath, const json& j, int indent = 4) {
        try {
            if (filePath.has_parent_path() && !fs::exists(filePath.parent_path())) {
                fs::create_directories(filePath.parent_path());
            }
            std::ofstream file(filePath);
            if (!file.is_open()) {
                Logger::logError("JSON_UTIL", "No se pudo abrir el archivo para escribir JSON: " + filePath.string());
                return false;
            }
            file << std::setw(indent) << j;
            return true;
        } catch (const std::exception& e) {
            Logger::logError("JSON_UTIL", std::string("Error al guardar JSON en archivo: ") + e.what());
            return false;
        }
    }

    // Parsea una cadena de texto a JSON de forma segura
    static json parseString(const std::string& content) {
        try {
            return json::parse(content);
        } catch (const std::exception& e) {
            Logger::logError("JSON_UTIL", std::string("Error al parsear cadena JSON: ") + e.what());
            return json{};
        }
    }
};