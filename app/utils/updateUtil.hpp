#pragma once
#include <string>
#include <windows.h>
#include <shellapi.h>
#include "pathsUtil.hpp"
#include "httpUtil.hpp"
#include "jsonUtil.hpp"
#include "loggerUtil.hpp"

class UpdateUtil {
    public:
    static std::wstring s2ws(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }
    
    static std::string fetchRemoteVersionString() {
        std::wstring remoteUrl = s2ws(PathsUtil::REMOTE_VERSION_JSON_URL);
        std::string response = HttpUtil::get(remoteUrl);
        if (response.empty()) return "";
        json remoteJson = JsonUtil::parseString(response);
        return remoteJson.value("version", "0.0.0");
    }
    
    static std::string getLocalVersionString() {
        if (!fs::exists(PathsUtil::VERSION_JSON_PATH)) return "0.0.0";
        json localJson = JsonUtil::loadFromFile(PathsUtil::VERSION_JSON_PATH);
        return localJson.value("version", "0.0.0");
    }
    
    static void executeUpdateScript() {
        std::wstring downloadUrl = s2ws(PathsUtil::LATEST_ZIP_URL);
        DWORD pid = GetCurrentProcessId();
        
        std::wstring psScript = 
        L"-NoProfile -WindowStyle Hidden -Command \""
        L"New-Item -ItemType Directory -Force -Path '" + PathsUtil::DOWNLOAD_FOLDER.wstring() + L"'; "
        L"Invoke-WebRequest -Uri '" + downloadUrl + L"' -OutFile '" + PathsUtil::TEMP_ZIP_PATH.wstring() + L"'; "
        L"Expand-Archive -Path '" + PathsUtil::TEMP_ZIP_PATH.wstring() + L"' -DestinationPath '" + PathsUtil::DOWNLOAD_FOLDER.wstring() + L"' -Force; "
        L"(Get-Process -Id " + std::to_wstring(pid) + L" -ErrorAction SilentlyContinue).WaitForExit(); "
        L"Get-ChildItem -Path '" + PathsUtil::BASE_DIR.wstring() + L"' | ForEach-Object { "
        L"  if ($_.Name -ne '" + PathsUtil::DOWNLOAD_FOLDER.filename().wstring() + L"') { "
        L"      if ($_.Name -eq 'app') { "
        L"          Get-ChildItem -Path $_.FullName | Where-Object { $_.Name -ne 'logs' } | Remove-Item -Recurse -Force "
        L"      } else { Remove-Item -Path $_.FullName -Recurse -Force } "
        L"  } "
        L"}; "
        L"Copy-Item -Path '" + PathsUtil::DOWNLOAD_FOLDER.wstring() + L"\\*' -Destination '" + PathsUtil::BASE_DIR.wstring() + L"' -Recurse -Force; "
        L"Remove-Item -Path '" + PathsUtil::DOWNLOAD_FOLDER.wstring() + L"' -Recurse -Force; "
        L"Start-Process -FilePath '" + PathsUtil::EXE_PATH.wstring() + L"' -WorkingDirectory '" + PathsUtil::BASE_DIR.wstring() + L"';\"";
        
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