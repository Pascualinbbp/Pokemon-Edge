#pragma once
#include <array>
#include <exception>
#include <optional>
#include <string>
#include "../models/saveData.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "../utils/core/pathsUtil.hpp"
#include "../utils/data/jsonUtil.hpp"

class SaveManager {
    public:
    // Se comprueba una sola vez: el menú consulta hasSave() en cada redibujado y no debe tocar el disco.
    static void init() { s_hasSave = fs::exists(PathsUtil::SAVE_PATH); }

    static bool hasSave() { return s_hasSave; }

    static bool save(const SaveData& data) {
        json j;
        j["version"] = SAVE_VERSION;
        j["player"]["position"] = data.playerPosition;

        const bool ok = JsonUtil::saveToFile(PathsUtil::SAVE_PATH, j);
        if (ok) s_hasSave = true;
        return ok;
    }

    static std::optional<SaveData> load() {
        const json j = JsonUtil::loadFromFile(PathsUtil::SAVE_PATH);
        try {
            SaveData data;
            data.playerPosition = j.at("player").at("position").get<std::array<float, 3>>();
            return data;
        } catch (const std::exception& e) {
            Logger::logError("SAVE_MANAGER", std::string("Partida guardada no válida: ") + e.what());
            return std::nullopt;
        }
    }

    private:
    static constexpr int SAVE_VERSION = 1;
    inline static bool s_hasSave = false;
};