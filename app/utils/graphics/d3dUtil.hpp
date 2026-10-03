#pragma once
#include <cstring>
#include <stdexcept>
#include <string>
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "../core/loggerUtil.hpp"
#include "../core/stringUtil.hpp"

namespace D3dUtil {
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    // Comprueba un HRESULT: si falla, lo registra en el log y lanza excepción.
    inline void check(HRESULT hr, const char* what) {
        if (FAILED(hr)) {
            char message[128];
            StringUtil::formatTo(message, "%s (HRESULT 0x%08X)", what, static_cast<unsigned>(hr));
            Logger::logError("D3D", message);
            throw std::runtime_error(message);
        }
    }

    // Crea un buffer de GPU. 'data' puede ser nullptr (buffers dinámicos o de constantes).
    // Los buffers D3D11_USAGE_DYNAMIC se crean con acceso de escritura de la CPU.
    inline ComPtr<ID3D11Buffer> createBuffer(ID3D11Device* device, UINT byteWidth, UINT bindFlags,
                                             const void* data, D3D11_USAGE usage, const char* what) {
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = usage;
        desc.ByteWidth = byteWidth;
        desc.BindFlags = bindFlags;
        if (usage == D3D11_USAGE_DYNAMIC) desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        const D3D11_SUBRESOURCE_DATA initial = { data, 0, 0 };
        ComPtr<ID3D11Buffer> buffer;
        check(device->CreateBuffer(&desc, data ? &initial : nullptr, &buffer), what);
        return buffer;
    }

    // Compila un punto de entrada de un shader escrito como texto HLSL.
    inline ComPtr<ID3DBlob> compileShader(const char* source, const char* entry, const char* target) {
        ComPtr<ID3DBlob> blob, errors;
        const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, target,
            D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors);
        if (FAILED(hr) && errors) {
            Logger::logError("D3D", std::string("Error de shader: ") + static_cast<const char*>(errors->GetBufferPointer()));
        }
        check(hr, "D3DCompile");
        return blob;
    }
}