#pragma once
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <windows.h>
#include <shellapi.h>
#include "paths_util.hpp"
#include "http_util.hpp"
#include "logger_util.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class UpdateUtil {
private:
    // Convierte std::string a std::wstring para las APIs de Windows
    static std::wstring s2ws(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

public:
    static bool isLatestVersion() {
        Logger::logInfo("UPDATER", "Verificando si existe una nueva versión disponible...");
        try {
            fs::path localJsonPath = PathsUtil::getVersionJsonPath();
            if (!fs::exists(localJsonPath)) return true;

            std::ifstream file(localJsonPath);
            json localJson = json::parse(file);
            std::string localVer = localJson.value("version", "0.0.0");

            std::wstring remoteUrl = s2ws(PathsUtil::REMOTE_VERSION_JSON);
            std::string response = HttpUtil::get(remoteUrl);
            
            if (response.empty()) {
                Logger::logError("UPDATER", "Error de red: No se pudo obtener version.json remoto.");
                return true; 
            }

            json remoteJson = json::parse(response);
            std::string remoteVer = remoteJson.value("version", "0.0.0");

            Logger::logInfo("UPDATER", "Versión local: " + localVer + " | Versión remota: " + remoteVer);
            
            bool upToDate = (localVer == remoteVer);
            if (upToDate) {
                Logger::logInfo("UPDATER", "La aplicación ya está en la última versión.");
            } else {
                Logger::logInfo("UPDATER", "Nueva versión detectada. Se requiere actualización.");
            }
            
            return upToDate;
        } catch (const std::exception& e) {
            Logger::logError("UPDATER", std::string("No se pudo verificar versión: ") + e.what());
            return true; 
        }
    }

    static void launchUpdateProcess() {
        fs::path baseDir = PathsUtil::getBaseDir();
        fs::path exePath = PathsUtil::getExePath();
        fs::path tempFolder = PathsUtil::getDownloadFolder();
        fs::path tempZip = PathsUtil::getTempZipPath();
        
        std::wstring downloadUrl = s2ws(PathsUtil::LATEST_ZIP_URL);
        DWORD pid = GetCurrentProcessId();

        Logger::logInfo("UPDATER", "Iniciando proceso de actualización del sistema...");
        Logger::logInfo("UPDATER", "Lanzando proceso externo (PowerShell) y cerrando aplicación actual.");

        // Script idéntico a tu lógica robusta de limpieza en Python
        std::wstring psScript = 
            L"-NoProfile -WindowStyle Hidden -Command \""
            L"New-Item -ItemType Directory -Force -Path '" + tempFolder.wstring() + L"'; "
            L"Invoke-WebRequest -Uri '" + downloadUrl + L"' -OutFile '" + tempZip.wstring() + L"'; "
            L"Expand-Archive -Path '" + tempZip.wstring() + L"' -DestinationPath '" + tempFolder.wstring() + L"' -Force; "
            L"(Get-Process -Id " + std::to_wstring(pid) + L" -ErrorAction SilentlyContinue).WaitForExit(); "
            L"Get-ChildItem -Path '" + baseDir.wstring() + L"' | ForEach-Object { "
            L"  if ($_.Name -ne '" + tempFolder.filename().wstring() + L"') { "
            L"      if ($_.Name -eq 'app') { "
            L"          Get-ChildItem -Path $_.FullName | Where-Object { $_.Name -ne 'logs' } | Remove-Item -Recurse -Force "
            L"      } else { Remove-Item -Path $_.FullName -Recurse -Force } "
            L"  } "
            L"}; "
            L"Copy-Item -Path '" + tempFolder.wstring() + L"\\*' -Destination '" + baseDir.wstring() + L"' -Recurse -Force; "
            L"Remove-Item -Path '" + tempFolder.wstring() + L"' -Recurse -Force; "
            L"Start-Process -FilePath '" + exePath.wstring() + L"' -WorkingDirectory '" + baseDir.wstring() + L"';\"";

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"open";
        sei.lpFile = L"powershell.exe";
        sei.lpParameters = psScript.c_str();
        sei.nShow = SW_HIDE;
        
        ShellExecuteExW(&sei);
        exit(0);
    }
};