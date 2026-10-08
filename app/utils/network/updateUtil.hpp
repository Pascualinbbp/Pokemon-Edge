#pragma once
#include <cstdio>
#include <cstdlib>
#include <functional>
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

    // Paso 1: descarga el zip de la última versión. progress(descargado, total).
    static bool downloadPackage(const HttpUtil::Progress& progress) {
        return HttpUtil::download(PathsUtil::LATEST_ZIP_URL, PathsUtil::TEMP_ZIP_PATH, progress);
    }

    // Paso 2: descomprime el zip en la carpeta temporal. progress(archivos hechos, archivos totales).
    static bool extractPackage(const std::function<void(int, int)>& progress) {
        using StringUtil::quote;
        const std::wstring zip = quote(PathsUtil::TEMP_ZIP_PATH.wstring());
        const std::wstring dest = quote(PathsUtil::DOWNLOAD_FOLDER.wstring());
        const std::wstring prog = quote(PathsUtil::EXTRACT_PROGRESS_PATH.wstring());

        std::wstring script = L"-NoProfile -WindowStyle Hidden -Command \"$ErrorActionPreference='Stop'; ";
        script += L"Add-Type -AssemblyName System.IO.Compression.FileSystem; ";
        script += L"$root=" + dest + L"; $z=[IO.Compression.ZipFile]::OpenRead(" + zip + L"); $n=$z.Entries.Count; $i=0; ";
        script += L"foreach($e in $z.Entries){ $t=Join-Path $root $e.FullName; ";
        script += L"if($e.FullName.EndsWith('/') -or $e.FullName.EndsWith('\\')){ New-Item -ItemType Directory -Force -Path $t | Out-Null } ";
        script += L"else { New-Item -ItemType Directory -Force -Path (Split-Path $t) | Out-Null; [IO.Compression.ZipFileExtensions]::ExtractToFile($e,$t,$true) } ";
        script += L"$i++; Set-Content -Path " + prog + L" -Value ('{0} {1}' -f $i,$n) }; $z.Dispose()\"";

        FileUtil::remove(PathsUtil::EXTRACT_PROGRESS_PATH);
        const bool ok = ProcessUtil::runHidden(PathsUtil::POWERSHELL_EXE, script, [&] {
            const auto text = FileUtil::readText(PathsUtil::EXTRACT_PROGRESS_PATH);
            int done = 0, total = 0;
            if (text && std::sscanf(text->c_str(), "%d %d", &done, &total) == 2 && total > 0) progress(done, total);
        });
        FileUtil::remove(PathsUtil::EXTRACT_PROGRESS_PATH);
        return ok;
    }

    // Paso 3: lanza el script que espera a que la app cierre, sustituye los archivos y la reinicia. Cierra la app.
    static bool installAndRestart() {
        if (!ProcessUtil::launchHidden(PathsUtil::POWERSHELL_EXE, buildInstallScript())) {
            Logger::logError("UPDATE_UTIL", "No se pudo lanzar el script de instalación.");
            return false;
        }
        std::exit(0);
    }

    private:
    static std::string readVersion(const json& j, const std::string& fallback) {
        return JsonUtil::find<std::string>(j, { "version" }).value_or(fallback);
    }

    static std::wstring buildInstallScript() {
        using StringUtil::quote;

        const std::wstring downloadDir = quote(PathsUtil::DOWNLOAD_FOLDER.wstring());
        const std::wstring downloadContents = quote(PathsUtil::DOWNLOAD_FOLDER.wstring() + L"\\*");
        const std::wstring downloadName = quote(PathsUtil::DOWNLOAD_FOLDER.filename().wstring());
        const std::wstring baseDir = quote(PathsUtil::BASE_DIR.wstring());
        const std::wstring exePath = quote(PathsUtil::EXE_PATH.wstring());
        const std::wstring appName = quote(PathsUtil::APP_DIR.filename().wstring());

        // Carpetas de datos del usuario (logs, partidas...) que se conservan dentro de 'app'.
        std::vector<std::wstring> keptNames;
        for (const fs::path& directory : PathsUtil::USER_DATA_DIRS) keptNames.push_back(quote(directory.filename().wstring()));
        const std::wstring kept = StringUtil::join(keptNames, L",");

        std::wstring script = L"-NoProfile -WindowStyle Hidden -Command \"";
        script += L"(Get-Process -Id " + std::to_wstring(GetCurrentProcessId()) + L" -ErrorAction SilentlyContinue).WaitForExit(); ";
        script += L"Remove-Item -Path " + quote(PathsUtil::TEMP_ZIP_PATH.wstring()) + L" -Force -ErrorAction SilentlyContinue; ";
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
