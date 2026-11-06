#pragma once
#include <string>
#include <vector>

// Entrada de un grupo de recompensas (tabla reward): todo lo que se reparte (cofres, objetos del suelo, hitos de la Pokédex, regalos de la tienda)
// son grupos de esta tabla. Al sortear un grupo sale una sola entrada, elegida al azar según su probabilidad.
struct RewardEntry {
    enum class Kind { ITEM, MATERIAL, XP }; // ITEM: id de item · MATERIAL: id de material · XP: experiencia del jugador (sin objetivo)

    int groupId = -1;
    Kind kind = Kind::ITEM;
    int targetId = 0;
    float probability = 1.0f;
    int minQuantity = 1;
    int maxQuantity = 1;

    static Kind parse(const std::string& text) { return text == "MATERIAL" ? Kind::MATERIAL : text == "XP" ? Kind::XP : Kind::ITEM; }
};

// Grupos con significado fijo para el código (el resto los referencian las demás tablas: chest.reward_id, dex_milestone.reward_id, item.gift_reward_id).
namespace RewardGroup {
    inline constexpr int GROUND = 10; // objetos sueltos por el mundo
}
