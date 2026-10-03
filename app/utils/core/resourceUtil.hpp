#pragma once
#include <cstddef>
#include <windows.h>
#include "resourceIds.h"

// Recursos incrustados en el ejecutable (los IDs están en resourceIds.h).
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

    inline HICON loadIcon(int id) {
        return LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(id));
    }
}