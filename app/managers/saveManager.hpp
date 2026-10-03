#pragma once
#include <array>
#include <ctime>
#include <exception>
#include <optional>
#include <string>
#include "../models/saveData.hpp"
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

        if (!JsonUtil::saveToFile(PathsUtil::saveSlotPath(slot), j)) return false;
        s_slots[slot] = { true, TimeUtil::formatLocal(now) };
        return true;
    }

    static std::optional<SaveData> load(int slot) {
        if (!validSlot(slot) || !s_slots[slot].used) return std::nullopt;

        const json j = JsonUtil::loadFromFile(PathsUtil::saveSlotPath(slot));
        try {
            SaveData data;
            data.playerPosition = j.at("player").at("position").get<std::array<float, 3>>();
            return data;
        } catch (const std::exception& e) {
            Logger::logError("SAVE_MANAGER", std::string("Partida guardada no válida: ") + e.what());
            return std::nullopt;
        }
    }

    static bool remove(int slot) {
        if (!validSlot(slot)) return false;

        std::error_code ec;
        fs::remove(PathsUtil::saveSlotPath(slot), ec);
        if (ec) {
            Logger::logError("SAVE_MANAGER", "No se pudo eliminar la partida: " + ec.message());
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
        if (!fs::exists(path)) {
            s_slots[slot] = {};
            return;
        }

        long long savedAt = 0;
        try {
            const json j = JsonUtil::loadFromFile(path);
            if (j.is_object()) savedAt = j.value("savedAt", 0LL);
        } catch (const std::exception&) {
            // Archivo dañado: la ranura sigue contando como ocupada, solo sin fecha.
        }
        s_slots[slot] = { true, TimeUtil::formatLocal(static_cast<std::time_t>(savedAt)) };
    }
};