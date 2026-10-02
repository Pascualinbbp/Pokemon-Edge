#pragma once
#include <filesystem>
#include <string>
#include <windows.h>

namespace fs = std::filesystem;

class PathsUtil {
    private:
    static fs::path initExePath() {
        wchar_t buffer[MAX_PATH];
        const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return fs::path(std::wstring(buffer, len));
    }
    
    public:
    inline static const fs::path EXE_PATH = initExePath();
    inline static const fs::path BASE_DIR = EXE_PATH.parent_path();
    inline static const fs::path APP_DIR = BASE_DIR / "app";
    inline static const fs::path DATA_DIR = APP_DIR / "data";
    
    inline static const fs::path VERSION_JSON_PATH = DATA_DIR / "version.json";
    inline static const fs::path LOGO_PATH = DATA_DIR / "logo.png";
    inline static const fs::path ICON_PATH = DATA_DIR / "icon.ico";
    inline static const fs::path DB_DIR = DATA_DIR / "db";
    inline static const fs::path DB_PATH = DB_DIR / "pokemonEdge.db";
    inline static const fs::path LOG_DIR = APP_DIR / "logs";
    inline static const fs::path LOG_FILE_PATH = LOG_DIR / "app.log";
    
    inline static const fs::path DOWNLOAD_FOLDER = BASE_DIR / "temp_download";
    inline static const fs::path TEMP_ZIP_PATH = DOWNLOAD_FOLDER / "update.zip";
    
    static constexpr const wchar_t* REMOTE_VERSION_JSON_URL = L"https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/version.json";
    static constexpr const wchar_t* LATEST_ZIP_URL = L"https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/PokemonEdge.zip";
};