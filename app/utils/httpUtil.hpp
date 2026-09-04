#pragma once
#include <string>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

class HttpUtil {
public:
    static std::string get(const std::wstring& url) {
        std::string response;
        
        URL_COMPONENTS urlComp = { 0 };
        urlComp.dwStructSize = sizeof(urlComp);
        wchar_t hostName[256];
        wchar_t urlPath[1024];
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = 256;
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = 1024;

        if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) return "";

        HINTERNET hSession = WinHttpOpen(L"PokemonEdge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        HINTERNET hConnect = WinHttpConnect(hSession, hostName, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
        
        // Habilitar seguimiento automático de redirecciones (Vital para GitHub Releases)
        DWORD dwOption = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &dwOption, sizeof(dwOption));

        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            if (WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD dwSize = 0, dwDownloaded = 0;
                do {
                    if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                        char* buffer = new char[dwSize + 1];
                        ZeroMemory(buffer, dwSize + 1);
                        if (WinHttpReadData(hRequest, (LPVOID)buffer, dwSize, &dwDownloaded)) {
                            response.append(buffer, dwDownloaded);
                        }
                        delete[] buffer;
                    }
                } while (dwSize > 0);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }
};