#pragma once
#include <algorithm>
#include <fstream>
#include <vector>
#include <windows.h>
#include "../core/pathsUtil.hpp"
#include "../core/loggerUtil.hpp"

// Mismo valor por defecto que usa el workflow cuando no hay secret.
#ifndef XOR_KEY_VAL
#define XOR_KEY_VAL 90
#endif

class DatabaseUtil {
    private:
    static constexpr unsigned char XOR_KEY = static_cast<unsigned char>(XOR_KEY_VAL);
    
    static bool decryptAndSave(const unsigned char* data, size_t size, const fs::path& outputPath) {
        std::vector<char> buffer(size);
        std::transform(data, data + size, buffer.begin(),
        [](unsigned char b) { return static_cast<char>(b ^ XOR_KEY); });
        
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open()) return false;
        file.write(buffer.data(), static_cast<std::streamsize>(size));
        return file.good();
    }
    
    public:
    static void extractDatabaseIfNeeded() {
        if (fs::exists(PathsUtil::DB_PATH)) return;
        
        Logger::logInfo("DB_UTIL", "Extrayendo y descifrando base de datos embebida...");
        
        HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(101), (LPCWSTR)RT_RCDATA);
        if (!hRes) {
            Logger::logError("DB_UTIL", "No se pudo encontrar el recurso de la base de datos en el EXE.");
            return;
        }
        HGLOBAL hData = LoadResource(nullptr, hRes);
        if (!hData) {
            Logger::logError("DB_UTIL", "No se pudo cargar el recurso de la base de datos.");
            return;
        }
        
        const DWORD dataSize = SizeofResource(nullptr, hRes);
        const void* pData = LockResource(hData);
        if (!pData || dataSize == 0) return;
        
        std::error_code ec;
        fs::create_directories(PathsUtil::DB_DIR, ec);
        
        if (decryptAndSave(static_cast<const unsigned char*>(pData), dataSize, PathsUtil::DB_PATH)) {
            Logger::logInfo("DB_UTIL", "Base de datos descifrada y lista en: " + PathsUtil::DB_PATH.string());
        } else {
            Logger::logError("DB_UTIL", "No se pudo escribir la base de datos en: " + PathsUtil::DB_PATH.string());
        }
    }
};