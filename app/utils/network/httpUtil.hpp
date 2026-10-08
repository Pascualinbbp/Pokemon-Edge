#pragma once
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <fstream>
#include <memory>
#include <string>
#include <windows.h>
#include <winhttp.h>
#include "../core/fileUtil.hpp"
#pragma comment(lib, "winhttp.lib")

class HttpUtil {
    private:
    struct HandleCloser {
        void operator()(HINTERNET h) const { WinHttpCloseHandle(h); }
    };
    using Handle = std::unique_ptr<void, HandleCloser>; // RAII: cierra los handles solo

    // Sesión + conexión + petición GET ya enviada y con respuesta 200 recibida.
    struct Response {
        Handle session, connection, request;
        uint64_t length = 0; // Content-Length (0 si no se conoce)
        explicit operator bool() const { return static_cast<bool>(request); }
    };

    static Response open(const std::wstring& url, int receiveTimeoutMs) {
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

        Response r;
        r.session.reset(WinHttpOpen(L"PokemonEdge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!r.session) return {};
        r.connection.reset(WinHttpConnect(r.session.get(), host.c_str(), uc.nPort, 0));
        if (!r.connection) return {};

        const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        r.request.reset(WinHttpOpenRequest(r.connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
        if (!r.request) return {};

        // Evita que el arranque se quede colgado si no hay red (resolve, connect, send, receive en ms).
        WinHttpSetTimeouts(r.request.get(), 5000, 5000, 10000, receiveTimeoutMs);

        // La política por defecto de WinHTTP ya sigue las redirecciones de GitHub Releases.
        if (!WinHttpSendRequest(r.request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(r.request.get(), nullptr)) return {};

        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(r.request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) || status != 200) return {};

        wchar_t len[32] = {};
        size = sizeof(len);
        if (WinHttpQueryHeaders(r.request.get(), WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, len, &size, WINHTTP_NO_HEADER_INDEX))
            r.length = _wcstoui64(len, nullptr, 10);
        return r;
    }

    // Lee el cuerpo por bloques; sink devuelve false para abortar.
    template <typename Sink>
    static bool readBody(Response& r, Sink&& sink) {
        std::string chunk;
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(r.request.get(), &available) && available > 0) {
            chunk.resize(available);
            DWORD read = 0;
            if (!WinHttpReadData(r.request.get(), chunk.data(), available, &read)) return false;
            if (!sink(chunk.data(), read)) return false;
        }
        return true;
    }

    public:
    using Progress = std::function<void(uint64_t done, uint64_t total)>; // total = 0 si se desconoce

    // Devuelve el cuerpo de la respuesta, o "" si hay error o el estado HTTP no es 200.
    static std::string get(const std::wstring& url) {
        Response r = open(url, 10000);
        if (!r) return {};
        std::string response;
        readBody(r, [&](const char* data, DWORD n) { response.append(data, n); return true; });
        return response;
    }

    // Descarga a un archivo informando del avance en cada bloque. Devuelve false si falla (borra el parcial).
    static bool download(const std::wstring& url, const fs::path& target, const Progress& progress) {
        Response r = open(url, 30000);
        if (!r || !FileUtil::ensureParentDirectory(target)) return false;
        std::ofstream file(target, std::ios::binary);
        if (!file) return false;

        uint64_t done = 0;
        if (progress) progress(0, r.length);
        const bool ok = readBody(r, [&](const char* data, DWORD n) {
            file.write(data, n);
            done += n;
            if (progress) progress(done, r.length);
            return file.good();
        });
        file.close();
        if (!ok || (r.length && done != r.length)) { FileUtil::remove(target); return false; }
        return true;
    }
};
