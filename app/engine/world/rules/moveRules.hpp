#pragma once
#include <algorithm>
#include <array>
#include <vector>
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"

// Aprendizaje de movimientos (única definición). Cada pokémon intenta aprender un movimiento al azar cada LEARN_EVERY niveles; solo de sus tipos
// (o de los genéricos cuya etiqueta de aprendizaje comparte), con la probabilidad relativa de cada movimiento (move.learn_weight). Lo que ya intentó
// aprender queda "desbloqueado" y no vuelve a salir. Conoce como mucho MAX_KNOWN.
namespace MoveRules {
    inline constexpr int MAX_KNOWN = 4;
    inline constexpr int LEARN_EVERY = 2;
    using Slots = std::array<int, MAX_KNOWN>;

    inline bool attemptsAt(int level) { return level > 0 && level % LEARN_EVERY == 0; }

    inline bool contains(const std::vector<int>& list, int id) { return std::find(list.begin(), list.end(), id) != list.end(); }

    // ¿Puede esta especie aprender ese movimiento? Con etiquetas de aprendizaje, si comparte alguna; sin ellas, si es de uno de sus tipos.
    inline bool canLearn(const PokemonSpecies& species, const Move& move) {
        if (!move.learnTags.empty()) {
            for (const int tag : move.learnTags) if (contains(species.learnTags, tag)) return true;
            return false;
        }
        return contains(species.types, move.typeId);
    }

    // Movimientos que aún puede intentar aprender (sin los ya desbloqueados).
    inline std::vector<const Move*> pool(const GameData& data, const PokemonSpecies& species, const std::vector<int>& unlocked) {
        std::vector<const Move*> result;
        for (const Move& move : data.moves) if (move.learnWeight > 0.0f && canLearn(species, move) && !contains(unlocked, move.id)) result.push_back(&move);
        return result;
    }

    // Elige uno al azar según su peso (-1 si no queda ninguno).
    inline int roll(const std::vector<const Move*>& options) {
        float total = 0.0f;
        for (const Move* move : options) total += move->learnWeight;
        if (options.empty() || total <= 0.0f) return -1;
        float pick = RandomUtil::range(0.0f, total);
        for (const Move* move : options) {
            if (pick < move->learnWeight) return move->id;
            pick -= move->learnWeight;
        }
        return options.back()->id;
    }

    inline int known(const Slots& slots) { return static_cast<int>(std::count_if(slots.begin(), slots.end(), [](int id) { return id >= 0; })); }
    inline int freeSlot(const Slots& slots) { for (int i = 0; i < MAX_KNOWN; ++i) if (slots[i] < 0) return i; return -1; }
}
