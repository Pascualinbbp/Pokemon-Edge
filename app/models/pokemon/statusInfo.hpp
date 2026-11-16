#pragma once
#include <string>
#include "effectSpec.hpp"

// Estado alterado (tabla status): nombre, tipo asociado y efecto que lo aplica. Un estado no afecta a los tipos a los que su tipo asociado no hace efecto (tabla de tipos).
struct StatusInfo {
    int id = -1;
    std::string name;
    int typeId = -1;
    EffectSpec::Status code = EffectSpec::Status::NONE;
};
