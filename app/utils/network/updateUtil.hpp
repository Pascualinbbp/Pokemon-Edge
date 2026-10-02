#pragma once
#include <cstdlib>
#include <string>
#include <windows.h>
#include <shellapi.h>
#include "../core/pathsUtil.hpp"
#include "../core/loggerUtil.hpp"
#include "../data/jsonUtil.hpp"
#include "httpUtil.hpp"

class UpdateUtil {
public:
    // Devuelve "" si no se pudo obtener la versión remota.
    static std::string fetchRemoteVersionString() {
        const std::string response = HttpUtil::get(PathsUtil::REMOTE_VERSION_JSON_URL);
        if (response.empty()) return "";
        return readVersion(JsonUtil::parseString(response), "");
    }

    static std::string getLocalVersionString() {
        if (!fs::exists(PathsUtil::VERSION_JSON_PATH)) return "0.0.0";
        return readVersion(JsonUtil::loadFromFile(PathsUtil::VERSION_JSON_PATH), "0.0.0");
    }

    // Lanza el script de actualización y cierra la app. Si no se puede lanzar, la app sigue.
    static void executeUpdateScript() {
        const std::wstring downloadDir = quote(PathsUtil::DOWNLOAD_FOLDER.wstring());
        const std::wstring zipPath = quote(PathsUtil::TEMP_ZIP_PATH.wstring());
        const std::wstring baseDir = quote(PathsUtil::BASE_DIR.wstring());
        const std::wstring exePath = quote(PathsUtil::EXE_PATH.wstring());
        const std::wstring downloadName = quote(PathsUtil::DOWNLOAD_FOLDER.filename().wstring());

        std::wstring script = L"-NoProfile -WindowStyle Hidden -Command \"$ProgressPreference='SilentlyContinue'; ";
        script += L"New-Item -ItemType Directory -Force -Path " + downloadDir + L"; ";
        script += L"Invoke-WebRequest -Uri " + quote(PathsUtil::LATEST_ZIP_URL) + L" -OutFile " + zipPath + L"; ";
        script += L"Expand-Archive -Path " + zipPath + L" -DestinationPath " + downloadDir + L" -Force; ";
        script += L"Remove-Item -Path " + zipPath + L" -Force; ";
        script += L"(Get-Process -Id " + std::to_wstring(GetCurrentProcessId()) + L" -ErrorAction SilentlyContinue).WaitForExit(); ";
        script += L"Get-ChildItem -Path " + baseDir + L" | ForEach-Object { ";
        script += L"  if ($_.Name -ne " + downloadName + L") { ";
        script += L"    if ($_.Name -eq 'app') { ";
        script += L"      Get-ChildItem -Path $_.FullName | Where-Object { $_.Name -ne 'logs' } | Remove-Item -Recurse -Force ";
        script += L"    } else { Remove-Item -Path $_.FullName -Recurse -Force } ";
        script += L"  } ";
        script += L"}; ";
        script += L"Copy-Item -Path '" + PathsUtil::DOWNLOAD_FOLDER.wstring() + L"\\*' -Destination " + baseDir + L" -Recurse -Force; ";
        script += L"Remove-Item -Path " + downloadDir + L" -Recurse -Force; ";
        script += L"Start-Process -FilePath " + exePath + L" -WorkingDirectory " + baseDir + L";\"";

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"open";
        sei.lpFile = L"powershell.exe";
        sei.lpParameters = script.c_str();
        sei.nShow = SW_HIDE;

        if (!ShellExecuteExW(&sei)) {
            Logger::logError("UPDATE_UTIL", "No se pudo lanzar el script de actualización.");
            return;
        }
        std::exit(0);
    }

private:
    static std::wstring quote(const std::wstring& s) { return L"'" + s + L"'"; }

    static std::string readVersion(const json& j, const std::string& fallback) {
        return j.is_object() ? j.value("version", fallback) : fallback;
    }
};