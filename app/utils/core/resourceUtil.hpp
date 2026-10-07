#pragma once
#include <cstddef>
#include <cwctype>
#include <string>
#include <windows.h>
#include "resourceIds.h"

// Recursos incrustados en el ejecutable (los IDs están en resourceIds.h; los de assets/ se identifican por su ruta).
namespace ResourceUtil {
    struct Blob {
        const unsigned char* data = nullptr;
        size_t size = 0;
        explicit operator bool() const { return data != nullptr && size > 0; }
    };

    namespace detail {
        inline Blob load(LPCWSTR name) {
            HRSRC resource = FindResourceW(nullptr, name, (LPCWSTR)RT_RCDATA);
            if (!resource) return {};

            HGLOBAL handle = LoadResource(nullptr, resource);
            if (!handle) return {};

            const void* data = LockResource(handle);
            const DWORD size = SizeofResource(nullptr, resource);
            if (!data || size == 0) return {};
            return { static_cast<const unsigned char*>(data), size };
        }
    }

    // Datos crudos (RCDATA) de un recurso. Apuntan a la memoria del ejecutable: no hay que liberarlos.
    inline Blob loadRaw(int id) {
        return detail::load(MAKEINTRESOURCEW(id));
    }

    // Fichero de assets/ incrustado por el workflow (generateAssets): se pide por su ruta dentro de assets/,
    // por ejemplo "types/1.png" o "logos/logo.png".
    inline Blob loadAsset(const std::string& path) {
        std::wstring name(path.begin(), path.end());
        for (wchar_t& c : name) c = c == L'\\' ? L'/' : static_cast<wchar_t>(std::towupper(c)); // rc.exe guarda los nombres en mayúsculas
        return detail::load(name.c_str());
    }

    inline HICON loadIcon(int id) {
        return LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(id));
    }
}
