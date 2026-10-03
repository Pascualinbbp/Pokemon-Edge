#pragma once
#include <string>
#include <windows.h>
#include <shellapi.h>

namespace ProcessUtil {
    // Lanza un programa sin ventana y sin esperar a que termine. Devuelve false si no se pudo lanzar.
    inline bool launchHidden(const std::wstring& file, const std::wstring& parameters) {
        SHELLEXECUTEINFOW info = { sizeof(info) };
        info.lpVerb = L"open";
        info.lpFile = file.c_str();
        info.lpParameters = parameters.c_str();
        info.nShow = SW_HIDE;
        return ShellExecuteExW(&info) != FALSE;
    }
}