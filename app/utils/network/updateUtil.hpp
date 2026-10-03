#pragma once
#include <cstdlib>
#include <string>
#include <vector>
#include <windows.h>
#include "httpUtil.hpp"
#include "../core/fileUtil.hpp"
#include "../core/loggerUtil.hpp"
#include "../core/pathsUtil.hpp"
#include "../core/processUtil.hpp"
#include "../core/stringUtil.hpp"
#include "../data/jsonUtil.hpp"

class UpdateUtil {
    public:
    // Devuelve "" si no se pudo obtener la versión remota.
    static std::string fetchRemoteVersionString() {
        const std::string response = HttpUtil::get(PathsUtil::REMOTE_VERSION_JSON_URL);
        if (response.empty()) return "";
        return readVersion(JsonUtil::parseString(response), "");
    }

    static std::string getLocalVersionString() {
        if (!FileUtil::exists(PathsUtil::VERSION_JSON_PATH)) return "0.0.0";
        return readVersion(JsonUtil::loadFromFile(PathsUtil::VERSION_JSON_PATH), "0.0.0");
    }

    // Lanza el script de actualización y cierra la app. Si no se puede lanzar, la app sigue.
    static void executeUpdateScript() {
        if (!ProcessUtil::launchHidden(PathsUtil::POWERSHELL_EXE, buildScript())) {
            Logger::logError("UPDATE_UTIL", "No se pudo lanzar el script de actualización.");
            return;
        }
        std::exit(0);
    }

    private:
    static std::string readVersion(const json& j, const std::string& fallback) {
        return JsonUtil::find<std::string>(j, { "version" }).value_or(fallback);
    }

    static std::wstring buildScript() {
        using StringUtil::quote;

        const std::wstring downloadDir = quote(PathsUtil::DOWNLOAD_FOLDER.wstring());
        const std::wstring downloadContents = quote(PathsUtil::DOWNLOAD_FOLDER.wstring() + L"\\*");
        const std::wstring downloadName = quote(PathsUtil::DOWNLOAD_FOLDER.filename().wstring());
        const std::wstring zipPath = quote(PathsUtil::TEMP_ZIP_PATH.wstring());
        const std::wstring baseDir = quote(PathsUtil::BASE_DIR.wstring());
        const std::wstring exePath = quote(PathsUtil::EXE_PATH.wstring());
        const std::wstring appName = quote(PathsUtil::APP_DIR.filename().wstring());

        // Carpetas de datos del usuario (logs, partidas...) que se conservan dentro de 'app'.
        std::vector<std::wstring> keptNames;
        for (const fs::path& directory : PathsUtil::USER_DATA_DIRS) keptNames.push_back(quote(directory.filename().wstring()));
        const std::wstring kept = StringUtil::join(keptNames, L",");

        std::wstring script = L"-NoProfile -WindowStyle Hidden -Command \"$ProgressPreference='SilentlyContinue'; ";
        script += L"New-Item -ItemType Directory -Force -Path " + downloadDir + L"; ";
        script += L"Invoke-WebRequest -Uri " + quote(PathsUtil::LATEST_ZIP_URL) + L" -OutFile " + zipPath + L"; ";
        script += L"Expand-Archive -Path " + zipPath + L" -DestinationPath " + downloadDir + L" -Force; ";
        script += L"Remove-Item -Path " + zipPath + L" -Force; ";
        script += L"(Get-Process -Id " + std::to_wstring(GetCurrentProcessId()) + L" -ErrorAction SilentlyContinue).WaitForExit(); ";
        script += L"Get-ChildItem -Path " + baseDir + L" | ForEach-Object { ";
        script += L"  if ($_.Name -ne " + downloadName + L") { ";
        script += L"    if ($_.Name -eq " + appName + L") { ";
        script += L"      Get-ChildItem -Path $_.FullName | Where-Object { $_.Name -notin " + kept + L" } | Remove-Item -Recurse -Force ";
        script += L"    } else { Remove-Item -Path $_.FullName -Recurse -Force } ";
        script += L"  } ";
        script += L"}; ";
        script += L"Copy-Item -Path " + downloadContents + L" -Destination " + baseDir + L" -Recurse -Force; ";
        script += L"Remove-Item -Path " + downloadDir + L" -Recurse -Force; ";
        script += L"Start-Process -FilePath " + exePath + L" -WorkingDirectory " + baseDir + L";\"";
        return script;
    }
};