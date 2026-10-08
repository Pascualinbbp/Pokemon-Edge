#pragma once
#include <functional>
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

    // Lanza un programa sin ventana y espera a que termine llamando a tick cada ~150 ms. true si salió con código 0.
    inline bool runHidden(const std::wstring& file, const std::wstring& parameters, const std::function<void()>& tick) {
        SHELLEXECUTEINFOW info = { sizeof(info) };
        info.fMask = SEE_MASK_NOCLOSEPROCESS;
        info.lpVerb = L"open";
        info.lpFile = file.c_str();
        info.lpParameters = parameters.c_str();
        info.nShow = SW_HIDE;
        if (!ShellExecuteExW(&info) || !info.hProcess) return false;
        while (WaitForSingleObject(info.hProcess, 150) == WAIT_TIMEOUT) if (tick) tick();
        if (tick) tick();
        DWORD code = 1;
        GetExitCodeProcess(info.hProcess, &code);
        CloseHandle(info.hProcess);
        return code == 0;
    }
}
