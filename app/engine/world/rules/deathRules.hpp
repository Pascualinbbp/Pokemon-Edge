#pragma once
#include <string>

// Por qué se desmaya el jugador y qué se le cuenta en la pantalla de carga: cada daño a la vida lleva su causa; si el equipo entero cae, la causa es TEAM.
namespace DeathRules {
    enum class Cause { UNKNOWN, FALL, DROWN, LAVA, BURN, SPRING, TEAM };

    inline const char* title(Cause cause) {
        switch (cause) {
            case Cause::FALL:   return "Caíste desde demasiado alto";
            case Cause::DROWN:  return "Te quedaste sin aire bajo el agua";
            case Cause::LAVA:   return "La lava fue demasiado para ti";
            case Cause::BURN:   return "Las quemaduras acabaron contigo";
            case Cause::SPRING: return "El agua hirviendo te venció";
            case Cause::TEAM:   return "Todos tus pokémon se han debilitado";
            default:            return "Te has desmayado";
        }
    }

    // Segunda línea: dónde se reaparece (más adelante, el último centro pokémon visitado) y qué pasa con el equipo.
    inline std::string detail(const std::string& place) { return "Despertarás en " + place + " con tu equipo curado."; }
}
