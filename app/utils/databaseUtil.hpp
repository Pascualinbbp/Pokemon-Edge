#pragma once
#include <windows.h>
#include <fstream>
#include <vector>
#include "pathsUtil.hpp"
#include "loggerUtil.hpp"

#ifndef XOR_KEY_VAL
#error "XOR_KEY_VAL no esta definido. Debe pasarse como flag de compilacion desde el entorno de compilacion."
#endif

class DatabaseUtil {
private:
    static constexpr unsigned char XOR_KEY = static_cast<unsigned char>(XOR_KEY_VAL);

    static void decryptAndSave(const char* data, DWORD size, const fs::path& outputPath) {
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open()) return;

        std::vector<char> buffer(size);
        std::memcpy(buffer.data(), data, size);

        for (DWORD i = 0; i < size; ++i) {
            buffer[i] = buffer[i] ^ XOR_KEY;
        }

        file.write(buffer.data(), size);
    }

public:
    static void extractDatabaseIfNeeded() {
        if (fs::exists(PathsUtil::DB_PATH)) {
            return;
        }

        Logger::logInfo("DB_UTIL", "Extrayendo y descifrando base de datos embebida...");

        HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(101), (LPCWSTR)RT_RCDATA);
        if (!hRes) {
            Logger::logError("DB_UTIL", "No se pudo encontrar el recurso de la base de datos en el EXE.");
            return;
        }

        HGLOBAL hData = LoadResource(NULL, hRes);
        if (!hData) {
            Logger::logError("DB_UTIL", "No se pudo cargar el recurso de la base de datos.");
            return;
        }

        DWORD dataSize = SizeofResource(NULL, hRes);
        void* pData = LockResource(hData);

        if (pData && dataSize > 0) {
            if (!fs::exists(PathsUtil::DB_DIR)) {
                fs::create_directories(PathsUtil::DB_DIR);
            }

            decryptAndSave(static_cast<char*>(pData), dataSize, PathsUtil::DB_PATH);
            Logger::logInfo("DB_UTIL", "Base de datos descifrada y lista en: " + PathsUtil::DB_PATH.string());
        }
    }
};