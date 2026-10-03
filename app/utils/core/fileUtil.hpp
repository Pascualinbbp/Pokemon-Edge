#pragma once
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// Operaciones de sistema de archivos. No registra errores: devuelve el resultado y quien llama decide
// qué hacer (así no depende del Logger, que a su vez usa este util).
class FileUtil {
    public:
    static bool exists(const fs::path& path) {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    static bool ensureDirectory(const fs::path& directory) {
        std::error_code ec;
        fs::create_directories(directory, ec);
        return !ec;
    }

    static bool ensureParentDirectory(const fs::path& file) {
        return !file.has_parent_path() || ensureDirectory(file.parent_path());
    }

    // Borrar un archivo que no existe cuenta como éxito.
    static bool remove(const fs::path& path) {
        std::error_code ec;
        fs::remove(path, ec);
        return !ec;
    }

    static std::optional<std::string> readText(const fs::path& path) {
        return readAll<std::string>(path);
    }

    static std::optional<std::vector<unsigned char>> readBinary(const fs::path& path) {
        return readAll<std::vector<unsigned char>>(path);
    }

    static bool writeText(const fs::path& path, const std::string& content) {
        return writeBinary(path, content.data(), content.size());
    }

    static bool writeBinary(const fs::path& path, const void* data, size_t size) {
        if (!ensureParentDirectory(path)) return false;
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        return file.good();
    }

    // Abre un archivo para añadir al final (por ejemplo, el log), creando su carpeta si hace falta.
    static std::ofstream openAppend(const fs::path& path) {
        ensureParentDirectory(path);
        return std::ofstream(path, std::ios::app);
    }

    private:
    template <typename Container>
    static std::optional<Container> readAll(const fs::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return std::nullopt;

        const std::streamsize size = file.tellg();
        if (size < 0) return std::nullopt;

        Container data;
        data.resize(static_cast<size_t>(size));
        file.seekg(0);
        if (size > 0 && !file.read(reinterpret_cast<char*>(data.data()), size)) return std::nullopt;
        return data;
    }
};