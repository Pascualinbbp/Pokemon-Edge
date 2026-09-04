#pragma once
#include <filesystem>
#include <string>
#include <windows.h>

namespace fs = std::filesystem;

class PathsUtil {
public:
    static fs::path getBaseDir() {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(NULL, buffer, MAX_PATH);
        return fs::path(buffer).parent_path();
    }

    static fs::path getExePath() {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(NULL, buffer, MAX_PATH);
        return fs::path(buffer);
    }

    static fs::path getVersionJsonPath() {
        return getBaseDir() / "app" / "data" / "version.json";
    }

    static fs::path getDownloadFolder() {
        return getBaseDir() / "temp_download";
    }

    static fs::path getTempZipPath() {
        return getDownloadFolder() / "update.zip";
    }

    // ================= Rutas Remotas =================
    // Apuntamos a los assets directos del Release, igual que en HermesApp
    static constexpr const char* REMOTE_VERSION_JSON = "https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/version.json";
    static constexpr const char* LATEST_ZIP_URL = "https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/PokemonEdge.zip";
};