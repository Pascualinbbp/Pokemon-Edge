#pragma once
#include <exception>
#include <initializer_list>
#include <optional>
#include <string>
#include "../nlohmann/json.hpp"
#include "../core/fileUtil.hpp"
#include "../core/loggerUtil.hpp"

using json = nlohmann::json;

// Todo el manejo de JSON de la aplicación: parsear, cargar, guardar y leer valores sin excepciones.
class JsonUtil {
    public:
    static json parseString(const std::string& content) {
        try {
            return json::parse(content);
        } catch (const std::exception& e) {
            Logger::logError("JSON_UTIL", std::string("Error al parsear JSON: ") + e.what());
            return json{};
        }
    }

    // Devuelve un json vacío (null) si el archivo no existe o no es válido.
    static json loadFromFile(const fs::path& filePath) {
        const std::optional<std::string> content = FileUtil::readText(filePath);
        if (!content) {
            Logger::logError("JSON_UTIL", "No se pudo abrir el archivo JSON: " + filePath.string());
            return json{};
        }
        return parseString(*content);
    }

    static bool saveToFile(const fs::path& filePath, const json& j, int indent = 4) {
        try {
            if (FileUtil::writeText(filePath, j.dump(indent))) return true;
            Logger::logError("JSON_UTIL", "No se pudo escribir el archivo JSON: " + filePath.string());
        } catch (const std::exception& e) {
            Logger::logError("JSON_UTIL", std::string("Error al guardar JSON: ") + e.what());
        }
        return false;
    }

    // Lee un valor anidado: find<int>(j, {"player", "stats", "level"}). Si falta alguna clave o el tipo
    // no coincide, devuelve nullopt en lugar de lanzar una excepción.
    template <typename T>
    static std::optional<T> find(const json& root, std::initializer_list<const char*> keys) {
        const json* node = &root;
        for (const char* key : keys) {
            if (!node->is_object()) return std::nullopt;
            const auto it = node->find(key);
            if (it == node->end()) return std::nullopt;
            node = &*it;
        }

        try {
            return node->get<T>();
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
};