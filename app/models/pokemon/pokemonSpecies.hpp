#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "spawnProfile.hpp"

// Estadísticas base de un pokémon, en el orden de la base de datos.
struct BaseStats {
    static constexpr int COUNT = 6;

    int hp = 0;
    int attack = 0;
    int spAttack = 0;
    int defense = 0;
    int spDefense = 0;
    int speed = 0;

    // Por índice (0 = PS, 1 = ataque, 2 = ataque especial, 3 = defensa, 4 = defensa especial, 5 = velocidad).
    int at(int index) const {
        const int values[COUNT] = { hp, attack, spAttack, defense, spDefense, speed };
        return values[index];
    }

    static const char* name(int index) {
        static constexpr const char* NAMES[COUNT] = { "PS", "Ataque", "At. esp.", "Defensa", "Def. esp.", "Velocidad" };
        return NAMES[index];
    }
};

// Cómo evoluciona un pokémon (tabla evolution).
struct Evolution {
    enum class Method { LEVEL, STONE, FAINT, WATERFALL };
    int toId = -1;
    Method method = Method::LEVEL;
    int level = 0;       // LEVEL: nivel que hay que alcanzar; FAINT: nivel mínimo al ser derrotado
    int materialId = -1; // STONE: mineral que se usa

    static Method parse(const std::string& text) {
        return text == "STONE" ? Method::STONE : text == "FAINT" ? Method::FAINT : text == "WATERFALL" ? Method::WATERFALL : Method::LEVEL;
    }
};

// Pokémon (tabla pokemon): todos sus datos en una fila.
struct PokemonSpecies {
    int id = -1;
    std::string name;
    std::string description;
    int skillId = -1;       // habilidad del mundo (talar, picar, regar...)
    int skillLevel = 1;     // y su nivel
    int abilityId = -1;     // habilidad activa
    int passiveId = -1;     // habilidad pasiva (-1 = no tiene)
    std::vector<int> types; // uno o dos id de la tabla type
    BaseStats stats;
    SpawnProfile spawn;       // en qué hábitats y con qué condiciones aparece salvaje
    float spawnWeight = 1.0f; // rareza: suma de sus pesos en los hábitats (cuanto menor, más raro); la rellena el DAO
    int catchRate = 45;       // ratio de captura fijo de la especie (1..255)
    int minLevel = 1;         // nivel al aparecer salvaje
    int maxLevel = 1;
    int generationId = -1;    // generación (tabla generation)
    std::vector<Evolution> evolutions; // vacío = no evoluciona
    bool swims = false;       // acuático: solo aparece donde hay agua y se mueve por ella
    bool starter = false;     // se puede elegir como pokémon inicial

    // Evolución por nivel que corresponde a 'level'; si hay varias posibles, elige una según 'roll' (0..1). nullptr si no toca.
    const Evolution* levelEvolution(int level, float roll) const {
        int count = 0;
        for (const Evolution& evolution : evolutions) if (evolution.method == Evolution::Method::LEVEL && level >= evolution.level) ++count;
        if (count == 0) return nullptr;
        int pick = (std::min)(static_cast<int>(roll * static_cast<float>(count)), count - 1);
        for (const Evolution& evolution : evolutions) {
            if (evolution.method == Evolution::Method::LEVEL && level >= evolution.level && pick-- == 0) return &evolution;
        }
        return nullptr;
    }

    // Nivel que tiene en una habilidad del mundo (0 = no la tiene).
    int levelIn(int skill) const { return skill == skillId ? skillLevel : 0; }
};
