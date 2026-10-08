#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Clima de cada hábitat. Cada uno tiene su propio contador: al cumplirse (entre su tiempo mínimo y máximo) sortea un clima
// de su tabla. Cada hábitat guarda como mucho 2 climas a la vez: si llega un tercero, el más antiguo se desvanece poco a
// poco y el nuevo aparece. Dos climas a la vez dan uno fusionado (tabla weather_fusion). En un punto del mundo el clima es
// la mezcla de los de los hábitats cercanos según su peso, y ahí también se fusionan los más fuertes.
class WeatherSystem {
    public:
    static constexpr int MAX_ACTIVE = 2;       // climas simultáneos por hábitat
    static constexpr float FADE_TIME = 8.0f;   // segundos que tarda un clima en aparecer o desaparecer
    static constexpr float MIN_STRENGTH = 0.08f;
    static constexpr float FUSION_SHARE = 0.3f; // el segundo clima de una zona fronteriza debe tener al menos este peso

    struct Local {
        int weatherId = -1;
        float strength = 0.0f; // 0..1
    };

    void setup(const GameData& data) {
        m_data = &data;
        m_zones.assign(data.habitats.size(), {});
        for (size_t i = 0; i < m_zones.size(); ++i) m_zones[i].timer = RandomUtil::range(0.0f, data.habitats[i].minWeatherWait * 0.5f);
    }

    void update(float dt) {
        for (size_t i = 0; i < m_zones.size(); ++i) {
            Zone& zone = m_zones[i];
            for (Slot& slot : zone.slots) slot.alpha = std::clamp(slot.alpha + (slot.leaving ? -dt : dt) / FADE_TIME, 0.0f, 1.0f);
            zone.slots.erase(std::remove_if(zone.slots.begin(), zone.slots.end(), [](const Slot& s) { return s.leaving && s.alpha <= 0.0f; }), zone.slots.end());

            zone.timer -= dt;
            if (zone.timer > 0.0f) continue;
            const Habitat& habitat = m_data->habitats[i];
            zone.timer = RandomUtil::range(habitat.minWeatherWait, (std::max)(habitat.minWeatherWait, habitat.maxWeatherWait));
            if (const WeatherChance* pick = RandomUtil::pick(habitat.weather, [](const WeatherChance& c) { return c.probability; })) push(zone, pick->weatherId);
        }
    }

    // Climas presentes en un punto, de más a menos fuerte. 'habitatWeights' = pesos de HabitatMap::weights.
    std::vector<Local> at(const std::vector<float>& habitatWeights) const {
        std::map<int, float> strength;
        for (size_t i = 0; i < m_zones.size() && i < habitatWeights.size(); ++i) {
            const float w = habitatWeights[i];
            if (w <= 0.01f) continue;
            std::vector<const Slot*> staying;
            for (const Slot& slot : m_zones[i].slots) {
                strength[slot.weatherId] += w * slot.alpha;
                if (!slot.leaving) staying.push_back(&slot);
            }
            if (staying.size() == MAX_ACTIVE) fuse(strength, staying[0]->weatherId, staying[1]->weatherId, w * (std::min)(staying[0]->alpha, staying[1]->alpha));
        }

        // Fusión espacial: en la frontera entre dos hábitats con clima distinto se mezclan los dos más fuertes.
        const std::vector<Local> sorted = sortedStrengths(strength);
        if (sorted.size() >= 2 && sorted[1].strength >= FUSION_SHARE * sorted[0].strength) {
            fuse(strength, sorted[0].weatherId, sorted[1].weatherId, (std::min)(sorted[0].strength, sorted[1].strength));
        }
        return sortedStrengths(strength);
    }

    private:
    struct Slot {
        int weatherId = -1;
        float alpha = 0.0f;
        bool leaving = false;
    };
    struct Zone {
        std::vector<Slot> slots;
        float timer = 0.0f;
    };

    // Un clima nuevo entra (si no está ya); si había 2, el más antiguo empieza a desvanecerse.
    static void push(Zone& zone, int weatherId) {
        std::vector<Slot*> staying;
        for (Slot& slot : zone.slots) if (!slot.leaving) staying.push_back(&slot);
        for (const Slot* slot : staying) if (slot->weatherId == weatherId) return;
        if (static_cast<int>(staying.size()) >= MAX_ACTIVE) staying.front()->leaving = true;
        zone.slots.push_back({ weatherId, 0.0f, false });
    }

    // Convierte 'amount' de fuerza de a y b en el clima fusionado (si existe).
    void fuse(std::map<int, float>& strength, int a, int b, float amount) const {
        const int result = m_data->fusionOf(a, b);
        if (result < 0 || a == b) return;
        strength[a] = (std::max)(0.0f, strength[a] - amount);
        strength[b] = (std::max)(0.0f, strength[b] - amount);
        strength[result] += amount;
    }

    static std::vector<Local> sortedStrengths(const std::map<int, float>& strength) {
        std::vector<Local> list;
        for (const auto& [id, value] : strength) if (value >= MIN_STRENGTH) list.push_back({ id, (std::min)(value, 1.0f) });
        std::sort(list.begin(), list.end(), [](const Local& a, const Local& b) { return a.strength > b.strength; });
        return list;
    }

    const GameData* m_data = &GameData::empty();
    std::vector<Zone> m_zones;
};
