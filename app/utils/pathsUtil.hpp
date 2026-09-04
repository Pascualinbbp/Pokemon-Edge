#pragma once
#include <filesystem>
#include <string>
#include <windows.h>

namespace fs = std::filesystem;

class PathsUtil {
private:
    static fs::path initBaseDir() {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(NULL, buffer, MAX_PATH);
        return fs::path(buffer).parent_path();
    }

    static fs::path initExePath() {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(NULL, buffer, MAX_PATH);
        return fs::path(buffer);
    }

public:
    inline static const fs::path BASE_DIR = initBaseDir();
    inline static const fs::path EXE_PATH = initExePath();
    inline static const fs::path VERSION_JSON_PATH = BASE_DIR / "app" / "data" / "version.json";
    inline static const fs::path DOWNLOAD_FOLDER = BASE_DIR / "temp_download";
    inline static const fs::path TEMP_ZIP_PATH = DOWNLOAD_FOLDER / "update.zip";
    inline static const fs::path LOG_DIR = BASE_DIR / "app" / "logs";
    inline static const fs::path LOG_FILE_PATH = LOG_DIR / "app.log";
    inline static const fs::path DB_DIR = BASE_DIR / "app" / "data" / "db";
    inline static const fs::path DB_PATH = DB_DIR / "pokemonEdge.db";

    static constexpr const char* REMOTE_VERSION_JSON_URL = "https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/version.json";
    static constexpr const char* LATEST_ZIP_URL = "https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/PokemonEdge.zip";
};