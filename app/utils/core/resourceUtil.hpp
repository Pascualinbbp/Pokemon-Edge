#pragma once
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <windows.h>
#include "loggerUtil.hpp"
#include "resourceIds.h"

// Recursos incrustados en el ejecutable (los IDs fijos están en resourceIds.h).
namespace ResourceUtil {
    struct Blob {
        const unsigned char* data = nullptr;
        size_t size = 0;
        explicit operator bool() const { return data != nullptr && size > 0; }
    };

    // Datos crudos (RCDATA) de un recurso. Apuntan a la memoria del ejecutable: no hay que liberarlos.
    inline Blob loadRaw(int id) {
        HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
        if (!resource) return {};

        HGLOBAL handle = LoadResource(nullptr, resource);
        if (!handle) return {};

        const void* data = LockResource(handle);
        const DWORD size = SizeofResource(nullptr, resource);
        if (!data || size == 0) return {};
        return { static_cast<const unsigned char*>(data), size };
    }

    namespace detail {
        // Índice "id ruta" por línea que genera el workflow con todo lo que hay en assets/ (resource IDs numéricos).
        inline const std::unordered_map<std::string, int>& assetIds() {
            static const std::unordered_map<std::string, int> ids = [] {
                std::unordered_map<std::string, int> map;
                if (const Blob index = loadRaw(IDR_ASSET_INDEX)) {
                    std::istringstream lines(std::string(reinterpret_cast<const char*>(index.data), index.size));
                    std::string line;
                    while (std::getline(lines, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        const size_t space = line.find(' ');
                        if (space == std::string::npos) continue;
                        map[line.substr(space + 1)] = std::atoi(line.substr(0, space).c_str());
                    }
                }
                return map;
            }();
            return ids;
        }
    }

    // Fichero de assets/ incrustado, pedido por su ruta dentro de assets/ ("types/1.png", "logos/logo.png"...).
    inline Blob loadAsset(const std::string& path) {
        const auto it = detail::assetIds().find(path);
        const Blob blob = it == detail::assetIds().end() ? Blob{} : loadRaw(it->second);
        if (!blob) Logger::logError("ASSETS", "Asset no incrustado: " + path);
        return blob;
    }

    inline HICON loadIcon(int id) {
        return LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(id));
    }
}
