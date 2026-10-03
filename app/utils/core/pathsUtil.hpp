#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;

// Único sitio donde se declaran las rutas, URLs y ejecutables externos de la aplicación.
// Si hay que cambiar una ruta, se cambia aquí y en ningún otro archivo.
class PathsUtil {
    private:
    static fs::path initExePath() {
        wchar_t buffer[MAX_PATH];
        const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return fs::path(std::wstring(buffer, len));
    }

    public:
    // --- Carpetas ---
    inline static const fs::path EXE_PATH = initExePath();
    inline static const fs::path BASE_DIR = EXE_PATH.parent_path();
    inline static const fs::path APP_DIR = BASE_DIR / "app";
    inline static const fs::path DATA_DIR = APP_DIR / "data";
    inline static const fs::path DB_DIR = DATA_DIR / "db";
    inline static const fs::path LOG_DIR = APP_DIR / "logs";
    inline static const fs::path SAVE_DIR = APP_DIR / "saves";
    inline static const fs::path DOWNLOAD_FOLDER = BASE_DIR / "temp_download";

    // --- Archivos ---
    inline static const fs::path VERSION_JSON_PATH = DATA_DIR / "version.json";
    inline static const fs::path LOGO_PATH = DATA_DIR / "logo.png";
    inline static const fs::path DB_PATH = DB_DIR / "pokemonEdge.db";
    inline static const fs::path LOG_FILE_PATH = LOG_DIR / "app.log";
    inline static const fs::path TEMP_ZIP_PATH = DOWNLOAD_FOLDER / "update.zip";

    // Carpetas con datos del usuario que la actualización no debe borrar.
    inline static const std::vector<fs::path> USER_DATA_DIRS = { LOG_DIR, SAVE_DIR };

    // --- URLs y programas externos ---
    static constexpr const wchar_t* REMOTE_VERSION_JSON_URL = L"https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/version.json";
    static constexpr const wchar_t* LATEST_ZIP_URL = L"https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/PokemonEdge.zip";
    static constexpr const wchar_t* POWERSHELL_EXE = L"powershell.exe";

    // Archivo de una ranura de partida (slot empieza en 0): slot1.json, slot2.json...
    static fs::path saveSlotPath(int slot) {
        return SAVE_DIR / ("slot" + std::to_string(slot + 1) + ".json");
    }
};