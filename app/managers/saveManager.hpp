#pragma once
#include <array>
#include <ctime>
#include <optional>
#include <string>
#include "../models/saveData.hpp"
#include "../utils/core/fileUtil.hpp"
#include "../utils/core/loggerUtil.hpp"
#include "../utils/core/pathsUtil.hpp"
#include "../utils/core/timeUtil.hpp"
#include "../utils/data/jsonUtil.hpp"

class SaveManager {
    public:
    static constexpr int MAX_SLOTS = 4;
    using Slots = std::array<SaveSlotInfo, MAX_SLOTS>;

    // Se lee el disco una sola vez; después los menús consultan la caché en memoria.
    static void init() {
        for (int slot = 0; slot < MAX_SLOTS; ++slot) readSlotInfo(slot);
    }

    static const Slots& slots() { return s_slots; }

    static bool hasSaves() {
        for (const SaveSlotInfo& info : s_slots) if (info.used) return true;
        return false;
    }

    // Primera ranura libre, o -1 si están todas ocupadas.
    static int freeSlot() {
        for (int slot = 0; slot < MAX_SLOTS; ++slot) if (!s_slots[slot].used) return slot;
        return -1;
    }

    static bool save(int slot, const SaveData& data) {
        if (!validSlot(slot)) return false;

        const std::time_t now = std::time(nullptr);
        json j;
        j["version"] = SAVE_VERSION;
        j["savedAt"] = static_cast<long long>(now);
        j["player"]["position"] = data.playerPosition;
        j["world"]["time"] = data.worldTime;

        if (!JsonUtil::saveToFile(PathsUtil::saveSlotPath(slot), j)) return false;
        s_slots[slot] = { true, TimeUtil::formatLocal(now) };
        return true;
    }

    static std::optional<SaveData> load(int slot) {
        if (!validSlot(slot) || !s_slots[slot].used) return std::nullopt;

        const json j = JsonUtil::loadFromFile(PathsUtil::saveSlotPath(slot));
        const auto position = JsonUtil::find<std::array<float, 3>>(j, { "player", "position" });
        if (!position) {
            Logger::logError("SAVE_MANAGER", "Partida guardada no válida en la ranura " + std::to_string(slot + 1));
            return std::nullopt;
        }

        SaveData data;
        data.playerPosition = *position;
        data.worldTime = JsonUtil::find<float>(j, { "world", "time" }).value_or(-1.0f); // partidas antiguas: sin dato
        return data;
    }

    static bool remove(int slot) {
        if (!validSlot(slot)) return false;

        if (!FileUtil::remove(PathsUtil::saveSlotPath(slot))) {
            Logger::logError("SAVE_MANAGER", "No se pudo eliminar la partida de la ranura " + std::to_string(slot + 1));
            return false;
        }
        s_slots[slot] = {};
        return true;
    }

    private:
    static constexpr int SAVE_VERSION = 1;
    inline static Slots s_slots;

    static bool validSlot(int slot) { return slot >= 0 && slot < MAX_SLOTS; }

    static void readSlotInfo(int slot) {
        const fs::path path = PathsUtil::saveSlotPath(slot);
        if (!FileUtil::exists(path)) {
            s_slots[slot] = {};
            return;
        }

        // Si el archivo está dañado la ranura sigue contando como ocupada, solo que sin fecha.
        const json j = JsonUtil::loadFromFile(path);
        const long long savedAt = JsonUtil::find<long long>(j, { "savedAt" }).value_or(0);
        s_slots[slot] = { true, TimeUtil::formatLocal(static_cast<std::time_t>(savedAt)) };
    }
};
