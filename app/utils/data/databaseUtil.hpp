#pragma once
#include <vector>
#include "cipherUtil.hpp"
#include "../core/fileUtil.hpp"
#include "../core/loggerUtil.hpp"
#include "../core/pathsUtil.hpp"
#include "../core/resourceUtil.hpp"

// Mismo valor por defecto que usa el workflow cuando no hay secret.
#ifndef XOR_KEY_VAL
#define XOR_KEY_VAL 90
#endif

class DatabaseUtil {
    private:
    static constexpr unsigned char XOR_KEY = static_cast<unsigned char>(XOR_KEY_VAL);

    public:
    static void extractDatabaseIfNeeded() {
        if (FileUtil::exists(PathsUtil::DB_PATH)) return;

        Logger::logInfo("DB_UTIL", "Extrayendo y descifrando base de datos embebida...");

        const ResourceUtil::Blob blob = ResourceUtil::loadRaw(IDR_DATABASE);
        if (!blob) {
            Logger::logError("DB_UTIL", "No se pudo cargar el recurso de la base de datos del EXE.");
            return;
        }

        const std::vector<unsigned char> database = CipherUtil::xorTransform(blob.data, blob.size, XOR_KEY);
        if (FileUtil::writeBinary(PathsUtil::DB_PATH, database.data(), database.size())) {
            Logger::logInfo("DB_UTIL", "Base de datos descifrada y lista en: " + PathsUtil::DB_PATH.string());
        } else {
            Logger::logError("DB_UTIL", "No se pudo escribir la base de datos en: " + PathsUtil::DB_PATH.string());
        }
    }
};