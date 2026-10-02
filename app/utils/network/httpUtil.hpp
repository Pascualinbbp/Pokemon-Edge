#pragma once
#include <memory>
#include <string>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

class HttpUtil {
    private:
    struct HandleCloser {
        void operator()(HINTERNET h) const { WinHttpCloseHandle(h); }
    };
    using Handle = std::unique_ptr<void, HandleCloser>; // RAII: cierra los handles solo
    
    public:
    // Devuelve el cuerpo de la respuesta, o "" si hay error o el estado HTTP no es 200.
    static std::string get(const std::wstring& url) {
        std::wstring host(256, L'\0');
        std::wstring path(2048, L'\0');
        
        URL_COMPONENTS uc = {};
        uc.dwStructSize = sizeof(uc);
        uc.lpszHostName = host.data();
        uc.dwHostNameLength = static_cast<DWORD>(host.size());
        uc.lpszUrlPath = path.data();
        uc.dwUrlPathLength = static_cast<DWORD>(path.size());
        
        if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &uc)) return {};
        host.resize(uc.dwHostNameLength);
        path.resize(uc.dwUrlPathLength);
        
        Handle session(WinHttpOpen(L"PokemonEdge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
            if (!session) return {};
            
            Handle connection(WinHttpConnect(session.get(), host.c_str(), uc.nPort, 0));
            if (!connection) return {};
            
            const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
            Handle request(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
            if (!request) return {};
            
            // Evita que el arranque se quede colgado si no hay red (resolve, connect, send, receive en ms).
            WinHttpSetTimeouts(request.get(), 5000, 5000, 10000, 10000);
            
            // La política por defecto de WinHTTP ya sigue las redirecciones de GitHub Releases.
            if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request.get(), nullptr)) {
                return {};
            }
            
            DWORD status = 0, statusSize = sizeof(status);
            if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) ||
            status != 200) {
                return {};
            }
            
            std::string response;
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request.get(), &available) && available > 0) {
                const size_t oldSize = response.size();
                response.resize(oldSize + available);
                DWORD read = 0;
                if (!WinHttpReadData(request.get(), response.data() + oldSize, available, &read)) {
                    response.resize(oldSize);
                    break;
                }
                response.resize(oldSize + read);
            }
            return response;
        }
    };