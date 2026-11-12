#pragma once
#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../../models/gameData.hpp"
#include "../../../utils/core/randomUtil.hpp"
#include "dexProgress.hpp"
#include "../rules/companionRules.hpp"
#include "../rules/ivRules.hpp"
#include "../rules/statRules.hpp"
#include "../rules/moveRules.hpp"
#include "../rules/pokemonRules.hpp"
#include "../rules/researchRules.hpp"

// Un pokémon del jugador. Su potencial (rango, topes de IV) está oculto hasta analizarlo en la máquina de investigación.
struct OwnedPokemon {
    int uid = 0;                    // identifica a este pokémon concreto durante la partida (no se guarda)
    int speciesId = -1;
    int level = 1;
    bool shiny = false;
    int ballId = -1;                // pokéball con la que se capturó (tabla pokeball)
    bool analyzed = false;          // ya pasó por la máquina de investigación
    IvRules::Ivs ivs = {};          // IV actuales (0..31): empiezan en 0 y suben con vitaminas
    IvRules::Ivs ivCaps = {};       // topes de IV con los que salió (su potencial); las chapas los suben a 31
    StatRules::Evs evs = {};        // EVs repartidos por el jugador (máx. 252 por estadística y 510 en total)
    int nature = 0;                 // naturaleza (id de nature; 0 = ninguna: neutra). El jugador puede cambiarla cuando quiera
    int xp = 0;                     // experiencia hacia el siguiente nivel (no se guarda)
    int hp = -1;                    // PS actuales (-1 = al máximo); 0 = debilitado
    int heldItemId = -1;            // objeto equipable que lleva (id de item; -1 = ninguno)
    MoveRules::Slots moves = { -1, -1, -1, -1 }; // movimientos que conoce (id de move; -1 = hueco)
    MoveRules::Slots pp = { -1, -1, -1, -1 };    // PP actuales de cada uno (-1 = al máximo; no se guardan)
    std::vector<int> unlocked;      // movimientos que ya intentó aprender (aunque no los lleve puestos): no vuelven a salir
    std::vector<int> pendingLearn;  // movimientos que quiere aprender y esperan la decisión del jugador (no se guarda)
};

// Resultado de un análisis de la máquina de investigación.
struct AnalysisEntry {
    int speciesId = -1;
    int level = 1;
    bool shiny = false;
    IvRules::Rank rank = IvRules::Rank::D;
    int reward = 0;
    bool released = false; // liberado automáticamente por tener menos potencial del pedido
};

struct AnalysisReport {
    std::vector<AnalysisEntry> entries;
    int total = 0; // pokémonedas ganadas
};

// Pokémon del jugador: el equipo (hasta 6; el primero es el que lo acompaña por el mundo) y el PC donde se guardan
// los demás.
class PokemonStorage {
    public:
    static constexpr int TEAM_SIZE = 6;
    static constexpr int PC_CAPACITY = 120;

    enum class LevelResult { NONE, LEVELED, EVOLVED };

    void setData(const GameData& data) {
        m_data = &data;
        m_team.clear();
        m_pc.clear();
        m_autoMin = IvRules::Rank::D;
        m_active = 0;
        m_dex.assign(data.species.size(), DEX_UNSEEN);
        dexProgress.setData(data);
    }

    DexProgress dexProgress; // logros por especie y sus recompensas (la Pokédex las muestra)

    // --- Pokédex: cada especie está sin ver, vista o capturada (una vez capturada no vuelve atrás). ---
    enum DexState : unsigned char { DEX_UNSEEN = 0, DEX_SEEN = 1, DEX_CAUGHT = 2 };
    DexState dexState(int speciesId) const {
        const int index = m_data->speciesIndex(speciesId);
        return index >= 0 && index < static_cast<int>(m_dex.size()) ? static_cast<DexState>(m_dex[index]) : DEX_UNSEEN;
    }
    void markSeen(int speciesId) { markDex(speciesId, DEX_SEEN); }
    int dexCount(DexState atLeast) const {
        int count = 0;
        for (const unsigned char state : m_dex) count += state >= atLeast ? 1 : 0;
        return count;
    }

    const std::vector<OwnedPokemon>& team() const { return m_team; }
    const std::vector<OwnedPokemon>& pc() const { return m_pc; }
    bool teamFull() const { return static_cast<int>(m_team.size()) >= TEAM_SIZE; }
    bool pcFull() const { return static_cast<int>(m_pc.size()) >= PC_CAPACITY; }
    bool empty() const { return m_team.empty() && m_pc.empty(); }

    // El pokémon que acompaña al jugador: uno del equipo. Cambiarlo no altera el orden del equipo.
    const OwnedPokemon* active() const { return m_team.empty() ? nullptr : &m_team[m_active]; }
    OwnedPokemon* activeMut() { return m_team.empty() ? nullptr : &m_team[m_active]; }
    int activeIndex() const { return m_active; }

    // --- PS ---
    // Al cambiar los PS máximos (EVs) los PS actuales conservan lo que faltaba por curar; un pokémon debilitado sigue debilitado.
    void keepHpRatio(OwnedPokemon& owned, int maxBefore, int hpBefore) const {
        if (owned.hp < 0 || hpBefore <= 0) return;
        owned.hp = (std::clamp)(hpBefore + (maxHp(owned) - maxBefore), 1, maxHp(owned));
    }
    int maxHp(const OwnedPokemon& owned) const {
        const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
        return species ? PokemonRules::maxHp(*species, owned.level, owned.ivs[0], owned.evs[0]) : 1;
    }
    // Estadística 'stat' (orden de BaseStats: 0 = PS) real de un pokémon: base, nivel, IV, EV y naturaleza.
    int statOf(const OwnedPokemon& owned, int stat) const {
        const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
        return species ? StatRules::of(species->stats, stat, owned.level, owned.ivs[static_cast<size_t>(stat)], owned.evs[static_cast<size_t>(stat)], m_data->nature(owned.nature)) : 1;
    }
    // Rango de potencial de un pokémon (por los topes de IV).
    IvRules::Rank rankOf(const OwnedPokemon& owned) const { return IvRules::rank(owned.ivCaps); }
    int hpOf(const OwnedPokemon& owned) const { return owned.hp < 0 ? maxHp(owned) : (std::min)(owned.hp, maxHp(owned)); }
    float hpFraction(const OwnedPokemon& owned) const { return static_cast<float>(hpOf(owned)) / static_cast<float>((std::max)(1, maxHp(owned))); }
    bool fainted(const OwnedPokemon& owned) const { return hpOf(owned) <= 0; }
    // Quita PS (al menos 1 si 'amount' > 0).
    void hurt(OwnedPokemon& owned, int amount) { owned.hp = (std::max)(0, hpOf(owned) - (std::max)(amount, 0)); }
    // Poción: devuelve PS (0 = todos). Solo a uno vivo y herido; false si no hace nada.
    bool heal(bool inTeam, int index, int amount) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || fainted(*owned) || hpOf(*owned) >= maxHp(*owned)) return false;
        owned->hp = amount <= 0 ? maxHp(*owned) : (std::min)(maxHp(*owned), hpOf(*owned) + amount);
        return true;
    }
    // Poción usada con éxito desde el menú: el mundo la recoge (takeMedicine) para mostrarla sobre el acompañante.
    struct MedicineUse { bool inTeam = false; int index = 0; int trainingId = -1; bool evolved = false; bool onCompanion = false; }; // trainingId: objeto de training_item usado (-1 si fue una evolución)
    void noteMedicine(bool inTeam, int index, int trainingId, bool evolved, bool onCompanion) { m_medicine = { inTeam, index, trainingId, evolved, onCompanion }; m_medicinePending = true; }
    bool takeMedicine(MedicineUse& out) { if (!m_medicinePending) return false; m_medicinePending = false; out = m_medicine; return true; }
    // Revivir: solo a uno debilitado. Deja la mitad de sus PS (redondeando a la baja) o todos.
    bool revive(bool inTeam, int index, bool full) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || !fainted(*owned)) return false;
        owned->hp = full ? maxHp(*owned) : (std::max)(1, maxHp(*owned) / 2);
        return true;
    }
    // Un debilitado que evoluciona por un objeto vuelve con un pequeño porcentaje de sus PS y sale al mundo (si es del equipo).
    bool reviveEvolved(bool inTeam, int index, int percent) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || !fainted(*owned)) return false;
        owned->hp = (std::max)(1, maxHp(*owned) * percent / 100);
        if (inTeam) setActive(index);
        return true;
    }
    // Un pokémon debilitado no se puede sacar: setActive lo rechaza y cycleActive lo salta.
    void setActive(int teamIndex) { if (teamIndex >= 0 && teamIndex < static_cast<int>(m_team.size()) && !fainted(m_team[teamIndex])) m_active = teamIndex; }
    void cycleActive(int direction) {
        const int n = static_cast<int>(m_team.size());
        for (int step = 1; step <= n; ++step) {
            const int next = ((m_active + direction * step) % n + n) % n;
            if (!fainted(m_team[next])) { m_active = next; return; }
        }
    }
    // Primer pokémon del equipo en condiciones de salir (-1 si todos están debilitados).
    int nextUsable() const {
        for (int i = 0; i < static_cast<int>(m_team.size()); ++i) if (!fainted(m_team[i])) return i;
        return -1;
    }
    bool teamWiped() const { return !m_team.empty() && nextUsable() < 0; }
    // Centro pokémon: todo el equipo recupera sus PS.
    void healTeam() { for (OwnedPokemon& owned : m_team) { owned.hp = -1; owned.pp = { -1, -1, -1, -1 }; } }

    // Un pokémon recién capturado va al equipo y, si está lleno, al PC. Devuelve false si no hay sitio en ninguno.
    bool add(const OwnedPokemon& owned, bool& sentToPc) {
        sentToPc = teamFull();
        OwnedPokemon stored = owned;
        stored.uid = m_nextUid++;
        ensureMoves(stored);
        markDex(owned.speciesId, DEX_CAUGHT);
        dexProgress.captured(owned.speciesId, owned.shiny, owned.level);
        if (owned.analyzed) dexProgress.ranked(owned.speciesId, IvRules::rank(owned.ivCaps));
        if (!sentToPc) m_team.push_back(stored);
        else if (!pcFull()) m_pc.push_back(stored);
        else return false;
        return true;
    }

    // Un lugar donde puede estar un pokémon: una posición del equipo o del PC. Un índice igual al tamaño de la lista es
    // "al final" (casilla vacía).
    struct Place {
        bool inTeam = true;
        int index = 0;
        bool operator==(const Place& other) const { return inTeam == other.inTeam && index == other.index; }
    };

    // Mueve un pokémon a otro lugar: si el destino está ocupado intercambian sus sitios; si está vacío pasa al final de esa
    // lista. El equipo nunca se queda sin pokémon ni pasa de 6, ni el PC de su capacidad. Si el que acompaña cambia de posición en
    // el equipo, sale el pokémon por el que se cambió (el que ocupa ahora su puesto); solo si ese está debilitado sigue el original. Devuelve false si no se pudo y deja en 'landed' dónde ha quedado.
    bool move(const Place& from, const Place& to, Place& landed) {
        std::vector<OwnedPokemon>& source = from.inTeam ? m_team : m_pc;
        std::vector<OwnedPokemon>& target = to.inTeam ? m_team : m_pc;
        if (from.index < 0 || from.index >= static_cast<int>(source.size()) || to.index < 0 || from == to) return false;

        if (to.index < static_cast<int>(target.size())) { // intercambio
            std::swap(source[from.index], target[to.index]);
            // Si el acompañante cambia de posición, sale el pokémon por el que se cambió (el que ocupa ahora su sitio); solo si ese está
            // debilitado sigue acompañando el original en su nueva posición.
            if (from.inTeam && to.inTeam && (m_active == from.index || m_active == to.index) && fainted(m_team[m_active])) m_active = m_active == from.index ? to.index : from.index;
            landed = to;
            return true;
        }
        if (&source == &target) { // al final de su misma lista
            std::rotate(source.begin() + from.index, source.begin() + from.index + 1, source.end());
            if (from.inTeam) m_active = m_active == from.index ? static_cast<int>(source.size()) - 1 : m_active > from.index ? m_active - 1 : m_active;
            landed = { from.inTeam, static_cast<int>(source.size()) - 1 };
            return true;
        }
        const bool full = to.inTeam ? teamFull() : pcFull();
        if (full || (from.inTeam && m_team.size() <= 1)) return false;
        target.push_back(source[from.index]);
        if (from.inTeam) eraseFromTeam(from.index);
        else source.erase(source.begin() + from.index);
        landed = { to.inTeam, static_cast<int>(target.size()) - 1 };
        return true;
    }

    // ¿Merece confirmación antes de liberarlo? Los variocolor y los de mucho potencial (A o más, ya analizados).
    bool valuable(const OwnedPokemon& owned) const {
        return owned.shiny || (owned.analyzed && IvRules::rank(owned.ivCaps) >= IvRules::Rank::A);
    }

    // --- Liberar ---
    // El pokémon inicial (el que lleva la pokéball exclusiva) nunca se libera; el equipo no puede quedarse vacío.
    bool bound(const OwnedPokemon& owned) const {
        const PokeballType* ball = m_data->ball(owned.ballId);
        return ball && !ball->obtainable;
    }

    bool canRelease(bool inTeam, int index) const {
        const std::vector<OwnedPokemon>& list = inTeam ? m_team : m_pc;
        return index >= 0 && index < static_cast<int>(list.size()) && !bound(list[index]) && !(inTeam && m_team.size() <= 1);
    }

    // Libera los pokémon indicados (índices del equipo y del PC). Devuelve cuántos se liberaron.
    int release(std::vector<int> teamIndices, std::vector<int> pcIndices) {
        int released = 0;
        const auto drop = [&](std::vector<OwnedPokemon>& list, std::vector<int>& indices, bool inTeam) {
            std::sort(indices.begin(), indices.end(), std::greater<int>());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
            for (const int index : indices) {
                if (!canRelease(inTeam, index)) continue;
                if (inTeam) eraseFromTeam(index);
                else list.erase(list.begin() + index);
                ++released;
            }
        };
        drop(m_team, teamIndices, true);
        drop(m_pc, pcIndices, false);
        return released;
    }

    // --- Entrenamiento ---
    // Sube un nivel (sin pasar de 'cap', el límite del jugador). Si alcanza el nivel de evolución, evoluciona.
    LevelResult levelUp(bool inTeam, int index, int cap) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || owned->level >= cap) return LevelResult::NONE;
        const int hpBefore = hpOf(*owned), maxBefore = maxHp(*owned);
        ++owned->level;
        if (owned->hp >= 0 && hpBefore > 0) owned->hp = hpBefore + (std::max)(0, maxHp(*owned) - maxBefore); // el aumento de PS máximos también sube los actuales
        dexProgress.leveled(owned->speciesId, owned->level);
        if (MoveRules::attemptsAt(owned->level)) { const int learned = attemptLearn(*owned); if (learned >= 0 && owned->pendingLearn.empty()) m_learnedNow.push_back({ owned->speciesId, learned }); }
        const PokemonSpecies* species = m_data->speciesById(owned->speciesId);
        const Evolution* evolution = species ? species->levelEvolution(owned->level, RandomUtil::range(0.0f, 1.0f)) : nullptr;
        if (evolution && m_data->speciesById(evolution->toId)) {
            owned->speciesId = evolution->toId;
            markDex(owned->speciesId, DEX_CAUGHT);
            return LevelResult::EVOLVED;
        }
        return LevelResult::LEVELED;
    }

    // Evoluciona (equipo o PC) con una piedra: 'itemId' es la piedra usada. Devuelve la especie nueva o nullptr si esa piedra no le hace nada.
    const PokemonSpecies* evolveWithStone(bool inTeam, int index, int itemId) {
        OwnedPokemon* owned = at(inTeam, index);
        const PokemonSpecies* species = owned ? m_data->speciesById(owned->speciesId) : nullptr;
        if (!species) return nullptr;
        for (const Evolution& evolution : species->evolutions) {
            const PokemonSpecies* next = evolution.method == Evolution::Method::STONE && evolution.itemId == itemId ? m_data->speciesById(evolution.toId) : nullptr;
            if (!next) continue;
            owned->speciesId = next->id;
            markDex(next->id, DEX_CAUGHT);
            return next;
        }
        return nullptr;
    }

    // Evoluciona a un pokémon del equipo por un método que no es el nivel (cascada...). Devuelve la especie nueva o nullptr si no toca.
    const PokemonSpecies* evolveBy(int teamIndex, Evolution::Method method) {
        OwnedPokemon* owned = at(true, teamIndex);
        const PokemonSpecies* species = owned ? m_data->speciesById(owned->speciesId) : nullptr;
        if (!species) return nullptr;
        for (const Evolution& evolution : species->evolutions) {
            const PokemonSpecies* next = evolution.method == method ? m_data->speciesById(evolution.toId) : nullptr;
            if (!next) continue;
            owned->speciesId = next->id;
            markDex(next->id, DEX_CAUGHT);
            return next;
        }
        return nullptr;
    }

    // Experiencia de combate: sube de nivel (y evoluciona) sin pasar de 'cap'. Devuelve niveles ganados y si evolucionó.
    struct XpResult { int levels = 0; bool evolved = false; };
    XpResult addXp(int teamIndex, int amount, int cap) {
        XpResult result;
        OwnedPokemon* owned = at(true, teamIndex);
        if (!owned) return result;
        if (owned->level >= cap) { owned->xp = 0; return result; } // en el nivel máximo no gana experiencia
        owned->xp += (std::max)(amount, 0);
        while (owned->level < cap && owned->xp >= PokemonRules::xpToNext(owned->level)) {
            owned->xp -= PokemonRules::xpToNext(owned->level);
            const LevelResult step = levelUp(true, teamIndex, cap);
            if (step == LevelResult::NONE) break;
            ++result.levels;
            result.evolved = result.evolved || step == LevelResult::EVOLVED;
        }
        if (owned->level >= cap) owned->xp = 0;
        return result;
    }

    // --- Movimientos ---
    // Intenta aprender uno nuevo (al azar entre los que no ha desbloqueado). Lo desbloquea; si tiene hueco lo equipa, si no queda pendiente de decisión.
    // Devuelve su id (-1 si ya no queda ninguno).
    int attemptLearn(OwnedPokemon& owned, bool damagingOnly = false) const {
        const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
        if (!species) return -1;
        std::vector<const Move*> options = MoveRules::pool(*m_data, *species, owned.unlocked);
        if (damagingOnly) { std::vector<const Move*> hits; for (const Move* move : options) if (move->damaging()) hits.push_back(move); if (!hits.empty()) options = hits; }
        const int id = MoveRules::roll(options);
        if (id < 0) return -1;
        owned.unlocked.push_back(id);
        const int slot = MoveRules::freeSlot(owned.moves);
        if (slot >= 0) { owned.moves[slot] = id; owned.pp[slot] = -1; }
        else owned.pendingLearn.push_back(id);
        return id;
    }
    // Movimientos iniciales de uno recién creado: el primero siempre, y uno más por cada LEARN_EVERY niveles que ya tiene. Los cuatro primeros los lleva
    // puestos; el resto queda desbloqueado.
    void ensureMoves(OwnedPokemon& owned) const {
        if (MoveRules::known(owned.moves) > 0 || !owned.unlocked.empty()) return;
        for (int i = 0, n = 1 + owned.level / MoveRules::LEARN_EVERY; i < n; ++i) {
            if (attemptLearn(owned, i == 0) < 0) break; // el primero siempre hace daño
            owned.pendingLearn.clear(); // los que no caben no preguntan: solo se desbloquean
        }
    }
    int maxPp(const OwnedPokemon& owned, int slot) const { const Move* move = m_data->move(owned.moves[slot]); return move ? move->pp : 0; }
    int ppOf(const OwnedPokemon& owned, int slot) const { return owned.pp[slot] < 0 ? maxPp(owned, slot) : (std::min)(owned.pp[slot], maxPp(owned, slot)); }
    // Pone 'moveId' (uno desbloqueado) en el hueco 'slot'; si ya lo llevaba en otro hueco, se intercambian.
    bool setMove(bool inTeam, int index, int slot, int moveId) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || slot < 0 || slot >= MoveRules::MAX_KNOWN || !MoveRules::contains(owned->unlocked, moveId)) return false;
        for (int i = 0; i < MoveRules::MAX_KNOWN; ++i) if (i != slot && owned->moves[i] == moveId) { owned->moves[i] = owned->moves[slot]; owned->pp[i] = -1; }
        owned->moves[slot] = moveId;
        owned->pp[slot] = -1;
        return true;
    }
    // Quita el hueco (siempre debe quedar al menos un movimiento).
    bool clearMove(bool inTeam, int index, int slot) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || slot < 0 || slot >= MoveRules::MAX_KNOWN || MoveRules::known(owned->moves) <= 1) return false;
        owned->moves[slot] = -1;
        return true;
    }
    // El primer pokémon (equipo, luego PC) con un movimiento esperando decisión. Devuelve su lugar y el movimiento (-1 si no hay).
    struct LearnRequest { bool inTeam = true; int index = -1; int moveId = -1; };
    LearnRequest pendingLearnRequest() const {
        for (int i = 0; i < static_cast<int>(m_team.size()); ++i) if (!m_team[i].pendingLearn.empty()) return { true, i, m_team[i].pendingLearn.front() };
        for (int i = 0; i < static_cast<int>(m_pc.size()); ++i) if (!m_pc[i].pendingLearn.empty()) return { false, i, m_pc[i].pendingLearn.front() };
        return {};
    }
    // Resuelve el aprendizaje pendiente: slot >= 0 olvida ese movimiento y aprende el nuevo; -1 lo rechaza (queda desbloqueado, se podrá equipar luego).
    void resolveLearn(bool inTeam, int index, int slot) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || owned->pendingLearn.empty()) return;
        const int id = owned->pendingLearn.front();
        owned->pendingLearn.erase(owned->pendingLearn.begin());
        if (slot >= 0 && slot < MoveRules::MAX_KNOWN) { owned->moves[slot] = id; owned->pp[slot] = -1; }
    }
    OwnedPokemon* mutableAt(bool inTeam, int index) { return at(inTeam, index); }

    // --- Objetos equipables ---
    const HeldItem* heldOf(const OwnedPokemon& owned) const { return m_data->held(owned.heldItemId); }
    bool holds(const OwnedPokemon& owned, HeldItem::Effect effect) const { const HeldItem* held = heldOf(owned); return held && held->effect == effect; }
    // Equipa 'itemId' (o -1 para quitarlo) y devuelve el que llevaba antes (-1 si ninguno).
    int setHeld(bool inTeam, int index, int itemId) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned) return -1;
        const int previous = owned->heldItemId;
        owned->heldItemId = itemId;
        return previous;
    }

    // Modo de todo el equipo (qué hace el pokémon que acompaña al jugador; no se guarda).
    CompanionRules::Mode mode = CompanionRules::Mode::COLLECT;
    void cycleMode() { mode = CompanionRules::next(mode); }

    // Vitamina: suma hasta 'amount' IV en una estadística (sin pasar de su tope). Devuelve false si no cambia nada.
    bool trainIv(bool inTeam, int index, int stat, int amount) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || !owned->analyzed || stat < 0 || stat >= BaseStats::COUNT) return false;
        const int gain = (std::min)(amount, owned->ivCaps[stat] - owned->ivs[stat]);
        if (gain <= 0) return false;
        owned->ivs[stat] += gain;
        return true;
    }

    // Chapa plateada: el tope de IV de una estadística pasa a 31 (el valor no cambia; el potencial sube).
    bool raiseIvCap(bool inTeam, int index, int stat) {
        OwnedPokemon* owned = at(inTeam, index);
        return owned && IvRules::raiseCap(owned->ivCaps, stat);
    }

    // Chapa dorada: todos los topes de IV a 31 (potencial S+).
    bool raiseAllIvCaps(bool inTeam, int index) {
        OwnedPokemon* owned = at(inTeam, index);
        return owned && IvRules::raiseAllCaps(owned->ivCaps);
    }

    // EVs: el jugador los reparte a su gusto cuando quiera, sin pasar de 252 por estadística ni de 510 en total. Devuelve el valor que queda.
    int setEv(bool inTeam, int index, int stat, int wanted) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned) return 0;
        const int before = maxHp(*owned);
        const int hpBefore = hpOf(*owned);
        const int value = StatRules::setEv(owned->evs, stat, wanted);
        keepHpRatio(*owned, before, hpBefore);
        return value;
    }
    void resetEvs(bool inTeam, int index) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned) return;
        const int before = maxHp(*owned);
        const int hpBefore = hpOf(*owned);
        owned->evs = {};
        keepHpRatio(*owned, before, hpBefore);
    }

    // Naturaleza: se puede cambiar en cualquier momento por la que se quiera (id de nature).
    bool setNature(bool inTeam, int index, int natureId) {
        OwnedPokemon* owned = at(inTeam, index);
        if (!owned || !m_data->nature(natureId)) return false;
        owned->nature = natureId;
        return true;
    }

    // --- Máquina de investigación ---
    int pendingCount() const {
        return static_cast<int>(std::count_if(m_team.begin(), m_team.end(), [](const OwnedPokemon& p) { return !p.analyzed; }) +
                                std::count_if(m_pc.begin(), m_pc.end(), [](const OwnedPokemon& p) { return !p.analyzed; }));
    }

    // Potencial mínimo que se conserva: los analizados con menos se liberan solos (los variocolor y el inicial, nunca).
    IvRules::Rank autoRelease() const { return m_autoMin; }
    void setAutoRelease(IvRules::Rank rank) { m_autoMin = rank; }

    // Analiza todos los pokémon pendientes: revela sus datos, calcula la recompensa y libera los que no llegan al mínimo.
    AnalysisReport analyze() {
        AnalysisReport report;
        const auto scan = [&](std::vector<OwnedPokemon>& list) {
            for (size_t i = 0; i < list.size();) {
                OwnedPokemon& owned = list[i];
                const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
                if (owned.analyzed || !species) { ++i; continue; }

                owned.analyzed = true;
                AnalysisEntry entry;
                entry.speciesId = owned.speciesId;
                entry.level = owned.level;
                entry.shiny = owned.shiny;
                entry.rank = IvRules::rank(owned.ivCaps);
                dexProgress.ranked(owned.speciesId, entry.rank);
                entry.reward = ResearchRules::reward(*m_data, *species, owned.level, owned.shiny);
                const bool drop = !owned.shiny && !bound(owned) && entry.rank < m_autoMin && (&list != &m_team || m_team.size() > 1);
                entry.released = drop;
                report.total += entry.reward;
                report.entries.push_back(entry);
                if (drop && &list == &m_team) eraseFromTeam(static_cast<int>(i));
                else if (drop) list.erase(list.begin() + i);
                else ++i;
            }
        };
        scan(m_pc);
        scan(m_team);
        return report;
    }

    // --- Guardado ("Especie|nivel|variocolor|bola|analizado|evs|máximos", por nombre) ---
    void store(std::vector<std::string>& team, std::vector<std::string>& pc) const {
        team = encode(m_team);
        pc = encode(m_pc);
    }

    // Pokédex guardada por nombre de especie: las vistas y las capturadas.
    void storeDex(std::vector<std::string>& seen, std::vector<std::string>& caught) const {
        seen.clear();
        caught.clear();
        for (size_t i = 0; i < m_dex.size() && i < m_data->species.size(); ++i) {
            if (m_dex[i] == DEX_CAUGHT) caught.push_back(m_data->species[i].name);
            else if (m_dex[i] == DEX_SEEN) seen.push_back(m_data->species[i].name);
        }
    }

    void restoreDex(const std::vector<std::string>& seen, const std::vector<std::string>& caught) {
        m_dex.assign(m_data->species.size(), DEX_UNSEEN);
        for (const std::string& name : seen) if (const int i = m_data->speciesIndexByName(name); i >= 0) m_dex[i] = DEX_SEEN;
        for (const std::string& name : caught) if (const int i = m_data->speciesIndexByName(name); i >= 0) m_dex[i] = DEX_CAUGHT;
        for (const OwnedPokemon& owned : m_team) markDex(owned.speciesId, DEX_CAUGHT); // partidas antiguas: lo que ya tienes está capturado
        for (const OwnedPokemon& owned : m_pc) markDex(owned.speciesId, DEX_CAUGHT);
    }

    void restore(const std::vector<std::string>& team, const std::vector<std::string>& pc, int autoRank, int active) {
        m_team.clear();
        m_pc.clear();
        for (const std::string& text : team) if (!teamFull()) decode(text, m_team);
        for (const std::string& text : pc) if (!pcFull()) decode(text, m_pc);
        m_autoMin = IvRules::RANKS[(std::clamp)(autoRank, 0, static_cast<int>(std::size(IvRules::RANKS)) - 1)];
        m_active = m_team.empty() ? 0 : (std::clamp)(active, 0, static_cast<int>(m_team.size()) - 1);
    }

    int autoRankIndex() const { return static_cast<int>(m_autoMin); }

    private:
    static constexpr char SEPARATOR = '|';

    void markDex(int speciesId, DexState state) {
        const int index = m_data->speciesIndex(speciesId);
        if (index >= 0 && index < static_cast<int>(m_dex.size()) && m_dex[index] < state) m_dex[index] = state;
    }
    std::vector<unsigned char> m_dex;

    // Quita a un pokémon del equipo manteniendo como activo al mismo (o al que ocupe su lugar si era él).
    void eraseFromTeam(int index) {
        m_team.erase(m_team.begin() + index);
        if (index < m_active) --m_active;
        m_active = m_team.empty() ? 0 : (std::min)(m_active, static_cast<int>(m_team.size()) - 1);
    }

    OwnedPokemon* at(bool inTeam, int index) {
        std::vector<OwnedPokemon>& list = inTeam ? m_team : m_pc;
        return index >= 0 && index < static_cast<int>(list.size()) ? &list[index] : nullptr;
    }

    static std::string join(const std::array<int, BaseStats::COUNT>& values) {
        std::string text;
        for (int i = 0; i < BaseStats::COUNT; ++i) text += (i ? "," : "") + std::to_string(values[i]);
        return text;
    }

    // "a,b,c,d,e,f" -> seis enteros sin límites (cada llamador los recorta a lo que le toca).
    static std::array<int, BaseStats::COUNT> splitRaw(const std::string& text) {
        std::array<int, BaseStats::COUNT> values = {};
        size_t from = 0;
        for (int i = 0; i < BaseStats::COUNT && from <= text.size(); ++i) {
            values[i] = std::atoi(text.c_str() + from);
            const size_t comma = text.find(',', from);
            if (comma == std::string::npos) break;
            from = comma + 1;
        }
        return values;
    }
    static IvRules::Ivs split(const std::string& text) { return IvRules::clamped(splitRaw(text)); }

    std::vector<std::string> encode(const std::vector<OwnedPokemon>& list) const {
        std::vector<std::string> result;
        for (const OwnedPokemon& owned : list) {
            const PokemonSpecies* species = m_data->speciesById(owned.speciesId);
            const PokeballType* ball = m_data->ball(owned.ballId);
            const HeldItem* held = m_data->held(owned.heldItemId);
            const Nature* nature = m_data->nature(owned.nature);
            if (!species) continue;
            result.push_back(species->name + SEPARATOR + std::to_string(owned.level) + SEPARATOR + (owned.shiny ? "1" : "0") + SEPARATOR +
                             (ball ? ball->name : std::string()) + SEPARATOR + (owned.analyzed ? "1" : "0") + SEPARATOR +
                             join(owned.ivs) + SEPARATOR + join(owned.ivCaps) + SEPARATOR + std::to_string(owned.hp) + SEPARATOR +
                             (held ? held->name : std::string()) + SEPARATOR + moveNames(owned.moves) + SEPARATOR + moveNames(owned.unlocked) + SEPARATOR +
                             join(owned.evs) + SEPARATOR + (nature ? nature->name : std::string()));
        }
        return result;
    }

    void decode(const std::string& text, std::vector<OwnedPokemon>& list) const {
        std::vector<std::string> fields;
        for (size_t from = 0;;) {
            const size_t cut = text.find(SEPARATOR, from);
            fields.push_back(text.substr(from, cut == std::string::npos ? std::string::npos : cut - from));
            if (cut == std::string::npos) break;
            from = cut + 1;
        }
        const int index = m_data->speciesIndexByName(fields[0]);
        if (index < 0) return;

        const PokemonSpecies& species = m_data->species[index];
        const auto field = [&](size_t i) -> const std::string& { static const std::string none; return i < fields.size() ? fields[i] : none; };
        OwnedPokemon owned;
        owned.uid = m_nextUid++;
        owned.speciesId = species.id;
        owned.level = (std::max)(1, field(1).empty() ? species.minLevel : std::atoi(field(1).c_str()));
        owned.shiny = field(2) == "1";
        if (const PokeballType* ball = m_data->ballByName(field(3))) owned.ballId = ball->id;
        owned.analyzed = field(4) == "1";
        owned.ivs = split(field(5));
        owned.ivCaps = field(6).empty() ? IvRules::Ivs{} : split(field(6));
        for (int i = 0; i < BaseStats::COUNT; ++i) owned.ivs[i] = (std::min)(owned.ivs[i], owned.ivCaps[i]);
        owned.evs = StatRules::clamped(splitRaw(field(11))); // partidas antiguas: sin EVs repartidos
        for (const Nature& nature : m_data->natures) if (!field(12).empty() && nature.name == field(12)) owned.nature = nature.id;
        if (owned.nature == 0) owned.nature = m_data->randomNatureId(); // partidas antiguas: una al azar (luego se puede cambiar)
        owned.hp = field(7).empty() ? -1 : std::atoi(field(7).c_str());
        for (const HeldItem& held : m_data->heldItems) if (!field(8).empty() && held.name == field(8)) owned.heldItemId = held.id;
        owned.moves = { -1, -1, -1, -1 };
        { int slot = 0; for (const std::string& name : splitNames(field(9))) if (const Move* move = m_data->moveByName(name)) if (slot < MoveRules::MAX_KNOWN) owned.moves[slot++] = move->id; }
        for (const std::string& name : splitNames(field(10))) if (const Move* move = m_data->moveByName(name)) owned.unlocked.push_back(move->id);
        for (const int id : owned.moves) if (id >= 0 && !MoveRules::contains(owned.unlocked, id)) owned.unlocked.push_back(id);
        ensureMoves(owned);
        owned.pendingLearn.clear();
        list.push_back(owned);
    }

    // Nombres de movimiento separados por ';' (los huecos vacíos se omiten).
    std::string moveNames(const MoveRules::Slots& slots) const { return moveNames(std::vector<int>(slots.begin(), slots.end())); }
    std::string moveNames(const std::vector<int>& ids) const {
        std::string text;
        for (const int id : ids) if (const Move* move = m_data->move(id)) text += (text.empty() ? "" : ";") + move->name;
        return text;
    }
    static std::vector<std::string> splitNames(const std::string& text) {
        std::vector<std::string> names;
        for (size_t from = 0; from < text.size();) {
            const size_t cut = text.find(';', from);
            names.push_back(text.substr(from, cut == std::string::npos ? std::string::npos : cut - from));
            if (cut == std::string::npos) break;
            from = cut + 1;
        }
        return names;
    }

    const GameData* m_data = &GameData::empty();
    std::vector<OwnedPokemon> m_team;
    std::vector<OwnedPokemon> m_pc;
    IvRules::Rank m_autoMin = IvRules::Rank::D;
    MedicineUse m_medicine;
    bool m_medicinePending = false;
    int m_active = 0;  // índice en el equipo del pokémon que acompaña al jugador
    mutable int m_nextUid = 1;
    public:
    // Movimientos aprendidos solos (había hueco) desde la última vez que se vaciaron: la escena los avisa. (especie, movimiento)
    std::vector<std::pair<int, int>> m_learnedNow;
};
