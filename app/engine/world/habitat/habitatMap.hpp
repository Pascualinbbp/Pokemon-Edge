#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include "worldNoise.hpp"
#include "underground.hpp"
#include "../../physics/physicsWorld.hpp"
#include "../../../models/gameData.hpp"

// Forma del mundo de una partida: se genera al empezar (según la semilla) a partir de los hábitats de la base de datos.
//  - El tamaño sale solo de las zonas que hay que colocar: cuanta más tierra pida la base de datos, mayor es la isla principal.
//  - El mundo es un archipiélago de contorno irregular (una isla grande y otras menores rodeadas de mar), nunca un cuadrado.
//  - Las zonas de tierra se colocan de la más alta a la más baja, cada una pegada a la de altura más parecida, y entre zonas no hay
//    corte: cada hábitat tiene un peso que baja suavemente al alejarse, así el relieve pasa gradualmente (montaña → bosque → pradera → playa).
//  - Hay lagos (en cuencas con su propio nivel de agua), ríos que bajan de las montañas al mar, con una cascada en el primer tramo
//    (aguas arriba: río y lago "altos"), y charcas dentro de los pantanos.
//  - Sin portales ni pantallas de carga: las cuevas y minas (muchas, de todos los tamaños) se extienden BAJO el terreno (Underground) y
//    se entra andando por una boca tallada en una ladera; las islas del cielo (también de varios tamaños) son islas de nube en las
//    nubes, más allá del archipiélago, y se llega a ellas por una Corriente Ascendente del mar.
// Todo se calcula una vez en mallas (Physics::Heightfield) que la física, el render y el minimapa consultan.
class HabitatMap {
    public:
    enum Water : uint8_t { DRY = 0, SEA, RIVER, UPPER_RIVER, LAKE, UPPER_LAKE, POND, CASCADE, CLOUD, WATER_KINDS };
    // Capa del mapa: lo que hay bajo tierra (cuevas y minas), la superficie y la isla del cielo.
    enum Layer : int { UNDERGROUND = 0, SURFACE, SKY, LAYERS };

    static constexpr float SNOW_LINE = 24.0f;      // altura (m) desde la que hay nieve y hielo
    static constexpr float SEA_FLOOR = -17.0f;
    static constexpr float PLATEAU_FLAT = 22.0f;   // radio del terreno llano del inicio (m) y hasta dónde se funde con el resto
    static constexpr float PLATEAU_FADE = 40.0f;
    static constexpr float FALLOFF = 1.5f;         // cuanto mayor, más corta la transición entre zonas
    static constexpr float SPACING = 0.5f;         // separación mínima entre centros, en fracción de la suma de radios
    static constexpr int LOBES = 4;
    static constexpr float LOBE_AMPLITUDE[LOBES] = { 0.20f, 0.14f, 0.10f, 0.06f };
    static constexpr int LOBE_HARMONIC[LOBES] = { 2, 3, 5, 8 };
    static constexpr float STREAM_RADIUS = 6.0f;   // radio de la Corriente Ascendente
    static constexpr float STREAM_CLIMB = 26.0f;   // metros sobre el suelo de nube hasta los que sube la corriente
    static constexpr float DARK_DEPTH = 14.0f;     // roca (m) sobre los pies desde la que la oscuridad es total
    static constexpr float MAX_WORLD_RADIUS = 322.0f; // radio máximo del archipiélago (el resto de la malla es mar y las islas del cielo)
    static constexpr float SKY_VOID = -100.0f;     // altura de lo que no existe alrededor de la isla del cielo (no se dibuja)

    struct Zone {
        int habitat = 0; // índice en GameData::habitats
        float x = 0.0f, z = 0.0f, radius = 10.0f;
        float stretch = 1.0f, cosA = 1.0f, sinA = 0.0f;
        float altitude = 3.0f;
        float roughness = 0.5f;
        float lobe[LOBES] = {};
        bool deep = false; // zona subterránea (sistema de cueva o mina)
    };

    // Región especial: sistema de cueva o mina (bajo tierra, con su boca en (mouthX, mouthZ)) o isla del cielo.
    struct Region {
        int habitat = 0;
        HabitatPlacement kind = HabitatPlacement::CAVE;
        float x = 0.0f, z = 0.0f, radius = 60.0f;
        float mouthX = 0.0f, mouthZ = 0.0f, dirX = 1.0f, dirZ = 0.0f; // boca y dirección hacia dentro (solo cueva y mina)
        float floorY = 0.0f;                                           // altura del suelo (en la boca, en cueva y mina; en el cielo, la del suelo de nube)
        float floorRadius = 0.0f, seaRadius = 0.0f;                    // isla del cielo: radio del suelo de nube y del mar de nubes (radius incluye además el talud)
        float lobe[LOBES] = {};                                        // isla del cielo: fases del contorno irregular
        bool dark() const { return kind == HabitatPlacement::CAVE || kind == HabitatPlacement::MINE; }
        Layer layer() const { return kind == HabitatPlacement::SKY ? SKY : UNDERGROUND; }
    };

    void generate(const GameData& data, unsigned seed) {
        m_seed = seed;
        m_habitats = static_cast<int>(data.habitats.size());
        m_noise = WorldNoise(seed);
        m_zones.clear();
        m_sites.clear();
        m_regions.clear();
        m_skyZones.clear();
        m_updrafts.clear();
        m_islands.clear();
        m_hallHabitats.clear();
        m_deepWater = -1;
        m_basins.clear();
        m_bandHabitat.clear();
        for (int& index : m_waterHabitat) index = -1;
        m_peakHabitat = m_riverBank = m_lakeBank = -1;
        std::mt19937 random(seed);
        m_random = &random;

        bindHabitats(data);
        planWorld(data);
        placeZones(data);
        bakeGround();
        carveLakes(data, false);
        carveRivers(data);
        carveLakes(data, true); // charcas
        buildRegions(data);     // antes de cerrar el agua: la isla del cielo tiene su propio mar de nubes
        finishWater();
        m_random = nullptr;
    }

    unsigned seed() const { return m_seed; }
    const std::vector<Zone>& zones() const { return m_zones; }
    // Lugares donde pueden salir cofres: las zonas de tierra y las regiones.
    const std::vector<Zone>& chestSites() const { return m_sites; }
    const std::vector<Region>& regions() const { return m_regions; }
    const Underground& under() const { return m_under; }
    const std::vector<Zone>& skyZones() const { return m_skyZones; }
    const std::vector<Physics::World::Updraft>& updrafts() const { return m_updrafts; }
    int habitatCount() const { return m_habitats; }
    float radius() const { return m_worldRadius; } // radio del archipiélago (m)

    const Physics::Heightfield& ground() const { return m_ground; }
    const Physics::Heightfield& surface() const { return m_surface; }
    const Physics::Heightfield& seaMask() const { return m_sea; }
    const Physics::Heightfield& coast() const { return m_coast; }

    float height(float x, float z) const { return m_ground.sample(x, z); }
    // Distancia a la costa (m): positiva en tierra, negativa en el mar.
    float coastDistance(float x, float z) const { return m_coast.sample(x, z); }

    // Temperatura (°C) a una altura dada: baja con la altitud.
    static float temperature(float altitude) { return 26.0f - 0.95f * (std::max)(altitude, 0.0f); }

    // ¿Se puede estar en este punto? (el archipiélago o la isla del cielo)
    bool playable(float x, float z) const {
        const Region* region = regionAt(x, z);
        if (region && region->kind == HabitatPlacement::SKY) return true;
        return std::sqrt(x * x + z * z) <= m_worldRadius - 4.0f;
    }

    // Región en la que está el punto (nullptr = mundo principal).
    const Region* regionAt(float x, float z) const {
        for (const Region& r : m_regions) {
            if (r.dark()) continue; // cuevas y minas no ocupan la superficie
            const float dx = x - r.x, dz = z - r.z;
            const float shape = skyShape(r, dx, dz);
            if (dx * dx + dz * dz < (r.radius * shape + 8.0f) * (r.radius * shape + 8.0f)) return &r;
        }
        return nullptr;
    }

    // Capa del mapa en la que está el punto de la superficie: el cielo (islas de nube) o la superficie.
    Layer surfaceLayerAt(float x, float z) const { return regionAt(x, z) ? SKY : SURFACE; }

    // Capa del mapa de quien está en (x, z) a la altura 'feet': bajo tierra (galería), el cielo o la superficie.
    Layer layerAt(float x, float z, float feet) const { return m_under.under(x, z, feet) ? UNDERGROUND : surfaceLayerAt(x, z); }
    // Oscuridad (0..1) de quien está en (x, z) a la altura 'feet': crece con la roca que tiene encima, así entrar es gradual.
    float darkness(float x, float z, float feet) const {
        if (!m_under.under(x, z, feet)) return 0.0f;
        return smooth(0.5f, DARK_DEPTH, m_ground.sample(x, z) - feet);
    }
    // Sistema de cueva o mina que contiene el punto (nullptr = ninguno).
    const Underground::System* systemAt(float x, float z) const {
        for (const Underground::System& s : m_under.systems()) if (x >= s.minX && x <= s.maxX && z >= s.minZ && z <= s.maxZ && m_under.cell(x, z)) return &s;
        return nullptr;
    }

    // Tipo de agua dulce o marina en el nodo más cercano (DRY = no hay).
    Water waterKind(float x, float z) const { return static_cast<Water>(m_kind[nearestNode(x, z)]); }
    // Corriente (m/s) en un punto: los ríos arrastran, las cascadas más.
    void flow(float x, float z, float& fx, float& fz) const {
        fx = m_flowX.sample(x, z);
        fz = m_flowZ.sample(x, z);
    }
    const Physics::Heightfield& flowX() const { return m_flowX; }
    const Physics::Heightfield& flowZ() const { return m_flowZ; }
    uint8_t kindAtNode(int col, int row) const { return m_kind[static_cast<size_t>(row) * m_ground.count() + col]; }
    float riverDistance(float x, float z) const { return m_riverDist.sample(x, z); }
    float lakeDistance(float x, float z) const { return m_lakeDist.sample(x, z); }
    // ¿Hay una cascada a menos de 'range' metros? (para el Magikarp que la remonta)
    bool nearCascade(float x, float z, float range) const {
        const int reach = static_cast<int>(std::ceil(range / Physics::Heightfield::CELL));
        const int c = nearestCol(x), r = nearestCol(z);
        for (int dr = -reach; dr <= reach; ++dr) {
            for (int dc = -reach; dc <= reach; ++dc) {
                const int cc = c + dc, rr = r + dr;
                if (cc < 0 || rr < 0 || cc >= m_ground.count() || rr >= m_ground.count()) continue;
                if (kindAtNode(cc, rr) != CASCADE) continue;
                const float ex = m_ground.nodeCoord(cc) - x, ez = m_ground.nodeCoord(rr) - z;
                if (ex * ex + ez * ez <= range * range) return true;
            }
        }
        return false;
    }

    // Peso de cada hábitat en (x, z): suman 1. Vacío si no hay hábitats.
    // Con 'underground' el punto es de la galería que hay bajo (x, z), no de la superficie.
    void weights(float x, float z, std::vector<float>& out, bool underground = false) const {
        out.assign(m_habitats, 0.0f);
        if (underground) {
            const int h = m_under.habitatAt(x, z);
            if (h >= 0 && h < m_habitats) out[h] = 1.0f;
            return;
        }
        if (m_zones.empty() && m_regions.empty()) return;
        const float g = m_ground.sample(x, z);
        const Water kind = waterKind(x, z);
        const float depth = m_surface.sample(x, z) - g;
        if (const Region* region = regionAt(x, z)) {
            if (kind == CLOUD && depth > WET && m_waterHabitat[CLOUD] >= 0) { out[m_waterHabitat[CLOUD]] = 1.0f; return; }
            float land = 0.0f; // islotes de tierra sobre el suelo de nube
            int landHabitat = -1;
            for (const Zone& zone : m_skyZones) {
                const float w = 1.0f - smooth(0.5f, 1.2f, distance2(zone, x, z));
                if (w > land) { land = w; landHabitat = zone.habitat; }
            }
            out[region->habitat] = 1.0f - land;
            if (landHabitat >= 0) out[landHabitat] += land;
            return;
        }
        if (kind != DRY && depth > WET) {
            if (kind == SEA) seaWeights(g, out);
            else if (m_waterHabitat[kind] >= 0) out[m_waterHabitat[kind]] = 1.0f;
            else if (m_waterHabitat[RIVER] >= 0) out[m_waterHabitat[RIVER]] = 1.0f;
            return;
        }
        float w[MAX_ZONES];
        shape(x, z, w);
        float scale = 1.0f;
        const float peak = m_peakHabitat >= 0 ? smooth(SNOW_LINE - 4.0f, SNOW_LINE + 1.0f, g) : 0.0f;
        if (peak > 0.0f) { out[m_peakHabitat] = peak; scale -= peak; }
        float bank[2] = { 0.0f, 0.0f };
        if (m_riverBank >= 0) bank[0] = 1.0f - smooth(1.5f, m_bankWidth[0], m_riverDist.sample(x, z));
        if (m_lakeBank >= 0) bank[1] = 1.0f - smooth(1.5f, m_bankWidth[1], m_lakeDist.sample(x, z));
        if (bank[0] > 0.0f) out[m_riverBank] = bank[0] * scale;
        if (bank[1] > 0.0f) out[m_lakeBank] += bank[1] * scale * (1.0f - bank[0]);
        const float rest = scale * (1.0f - bank[0]) * (1.0f - bank[1]);
        for (size_t i = 0; i < m_zones.size(); ++i) out[m_zones[i].habitat] += w[i] * rest;
    }

    // Índice del hábitat que más pesa en (x, z) (-1 si no hay hábitats).
    int dominant(float x, float z, bool underground = false) const {
        std::vector<float> w;
        weights(x, z, w, underground);
        return w.empty() ? -1 : static_cast<int>(std::max_element(w.begin(), w.end()) - w.begin());
    }

    // Entrega a la física lo que necesita: relieve, agua, corrientes y límites de la zona jugable.
    void applyTo(Physics::World& world) const {
        world.terrain = m_ground;
        world.water = m_surface;
        world.seaMask = m_sea;
        world.flowX = m_flowX;
        world.flowZ = m_flowZ;
        world.archipelago = { 0.0f, 0.0f, m_worldRadius };
        world.caveFloor = m_under.floorField();
        world.caveCeil = m_under.ceilField();
        world.caveWater = m_under.waterField();
        world.caveRects.clear();
        for (const Underground::System& system : m_under.systems()) world.caveRects.push_back({ system.minX - 4.0f, system.maxX + 4.0f, system.minZ - 4.0f, system.maxZ + 4.0f });
        world.regions.clear(); // solo las islas del cielo limitan al jugador: cuevas y minas están dentro del archipiélago
        for (const Region& region : m_regions) if (!region.dark()) world.regions.push_back({ region.x, region.z, region.radius * 1.35f + SKY_EXIT, region.radius * 1.35f + SKY_EXIT + SKY_TRIGGER, region.floorY - SKY_FALL });
        world.updrafts = m_updrafts;
    }

    private:
    static constexpr int MAX_ZONES = 160;
    static constexpr float SKY_TRIGGER = 14.0f; // a menos de esto fuera del borde de la isla del cielo, el jugador queda dentro de ella
    static constexpr float SKY_EXIT = 6.0f;     // margen más allá del talud: el jugador puede salir del suelo de nube y caer
    static constexpr float SKY_FALL = 70.0f;    // por debajo de esta altura bajo el suelo de nube deja de pertenecer a la isla
    static constexpr float WET = 0.25f; // con menos agua que esto cuenta como orilla/tierra

    struct Island { float x, z, r, ox, oz, phase1, phase2; };
    struct SeaBand { int habitat; float mid, width; };
    struct PathPoint { float x, z, terrain, level, bed, halfWidth, tx, tz; bool cascade; };
    struct Basin { float x, z, radius, level; Water kind; };

    static float smooth(float edge0, float edge1, float x) {
        const float t = (std::clamp)((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    float between(float low, float high) { return low + (high - low) * std::uniform_real_distribution<float>(0.0f, 1.0f)(*m_random); }
    int betweenInt(int low, int high) { return std::uniform_int_distribution<int>(low, (std::max)(low, high))(*m_random); }

    size_t nearestNode(float x, float z) const { return static_cast<size_t>(nearestCol(z)) * m_ground.count() + nearestCol(x); }
    int nearestCol(float coordinate) const {
        return (std::clamp)(static_cast<int>(std::lround((coordinate + m_ground.half()) / Physics::Heightfield::CELL)), 0, m_ground.count() - 1);
    }
    float ground(float x, float z) const { return m_ground.sample(x, z); }

    // ---- Datos de la base: qué hábitat es cada cosa ----
    void bindHabitats(const GameData& data) {
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& habitat = data.habitats[h];
            switch (habitat.placement) {
                case HabitatPlacement::SEA: m_bandHabitat.push_back({ h, 0.5f * (habitat.minAltitude + habitat.maxAltitude), (std::max)(0.5f * (habitat.maxAltitude - habitat.minAltitude), 0.2f) }); break;
                case HabitatPlacement::RIVER: m_waterHabitat[RIVER] = h; break;
                case HabitatPlacement::UPPER_RIVER: m_waterHabitat[UPPER_RIVER] = h; break;
                case HabitatPlacement::LAKE: m_waterHabitat[LAKE] = h; break;
                case HabitatPlacement::UPPER_LAKE: m_waterHabitat[UPPER_LAKE] = h; break;
                case HabitatPlacement::POND: m_waterHabitat[POND] = h; break;
                case HabitatPlacement::PEAK: m_peakHabitat = h; break;
                case HabitatPlacement::SKY_SEA: m_waterHabitat[CLOUD] = h; break;
                case HabitatPlacement::HALL: m_hallHabitats.push_back(h); break;
                case HabitatPlacement::UNDERGROUND_WATER: if (m_deepWater < 0) m_deepWater = h; break;
                case HabitatPlacement::BESIDE: {
                    const int host = data.habitatIndex(habitat.besideId);
                    const bool river = host >= 0 && (data.habitats[host].placement == HabitatPlacement::RIVER || data.habitats[host].placement == HabitatPlacement::UPPER_RIVER);
                    (river ? m_riverBank : m_lakeBank) = h;
                    m_bankWidth[river ? 0 : 1] = (std::max)(habitat.maxRadius, 3.0f);
                    break;
                }
                default: break;
            }
        }
        m_waterHabitat[CASCADE] = m_waterHabitat[RIVER];
        std::sort(m_bandHabitat.begin(), m_bandHabitat.end(), [](const SeaBand& a, const SeaBand& b) { return a.mid > b.mid; });
    }

    void seaWeights(float groundHeight, std::vector<float>& out) const {
        if (m_bandHabitat.empty()) return;
        float total = 0.0f;
        for (const SeaBand& band : m_bandHabitat) {
            const float d = (groundHeight - band.mid) / (0.45f * band.width + 0.2f);
            out[band.habitat] = std::exp(-0.5f * d * d);
            total += out[band.habitat];
        }
        if (total < 1.0e-6f) { // fuera de todos los rangos: el más cercano
            const SeaBand* best = &m_bandHabitat.front();
            for (const SeaBand& band : m_bandHabitat) if (std::fabs(groundHeight - band.mid) < std::fabs(groundHeight - best->mid)) best = &band;
            out[best->habitat] = 1.0f;
            return;
        }
        for (const SeaBand& band : m_bandHabitat) out[band.habitat] /= total;
    }

    // ---- Tamaño y forma del archipiélago ----
    void planWorld(const GameData& data) {
        // Área de tierra que piden las zonas: de ahí sale el tamaño de la isla principal.
        float area = 0.0f;
        for (const Habitat& habitat : data.habitats) {
            if (!habitat.land()) continue;
            const float zones = 0.5f * (static_cast<float>((std::max)(1, habitat.minZones)) + static_cast<float>((std::max)(1, habitat.maxZones)));
            const float r = 0.5f * (habitat.minRadius + habitat.maxRadius);
            area += zones * 3.14159f * r * r * 0.5f * (habitat.minStretch + habitat.maxStretch);
        }
        const float mainRadius = (std::clamp)(std::sqrt(area * 0.8f / 3.14159f) * 1.05f, 140.0f, 195.0f);
        m_worldRadius = (std::min)(mainRadius * 2.05f, MAX_WORLD_RADIUS);
        m_islands.push_back({ 0.0f, 0.0f, mainRadius, between(0, 100), between(0, 100), between(0, 6.28f), between(0, 6.28f) });
        const int satellites = (std::clamp)(static_cast<int>(mainRadius / 28.0f), 3, 8);
        for (int i = 0, tries = 0; i < satellites && tries < 400; ++tries) {
            Island island{ 0, 0, mainRadius * between(0.14f, 0.30f), between(0, 100), between(0, 100), between(0, 6.28f), between(0, 6.28f) };
            const float angle = between(0.0f, 6.2831853f);
            const float distance = between(mainRadius * 1.25f + island.r, m_worldRadius - island.r * 1.3f);
            island.x = std::cos(angle) * distance;
            island.z = std::sin(angle) * distance;
            if (std::sqrt(island.x * island.x + island.z * island.z) + island.r * 1.3f > m_worldRadius) continue;
            bool free = true;
            for (const Island& other : m_islands) {
                const float dx = island.x - other.x, dz = island.z - other.z;
                const float need = island.r * 1.4f + other.r * 1.4f + 30.0f;
                if (dx * dx + dz * dz < need * need) free = false;
            }
            if (!free) continue;
            m_islands.push_back(island);
            ++i;
        }
    }

    // Distancia con signo a la costa (m): positiva dentro de una isla. Contorno irregular: lóbulos, bahías y penínsulas.
    float coastSigned(float x, float z) const {
        float best = -1.0e9f;
        for (const Island& island : m_islands) {
            const float dx = x - island.x, dz = z - island.z;
            const float angle = std::atan2(dz, dx);
            const float stretch = 1.0f + 0.16f * std::sin(2.0f * angle + island.phase1) + 0.10f * std::sin(3.0f * angle + island.phase2) + 0.05f * std::sin(5.0f * angle + island.phase1 * 2.0f);
            const float d = std::sqrt(dx * dx + dz * dz) * stretch;
            const float n = m_noise.fbm(x * 0.011f + island.ox, z * 0.011f + island.oz, 4);
            const float fine = m_noise.fbm(x * 0.05f + island.oz, z * 0.05f + island.ox, 2);
            best = (std::max)(best, island.r * (1.0f + 0.32f * n) + 9.0f * fine - d);
        }
        return best;
    }

    // ---- Zonas de tierra ----
    void placeZones(const GameData& data) {
        struct Request { int habitat; float altitude; };
        std::vector<Request> requests;
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& habitat = data.habitats[h];
            if (!habitat.land()) continue;
            const int count = betweenInt((std::max)(1, habitat.minZones), (std::max)(1, habitat.maxZones));
            for (int i = 0; i < count; ++i) requests.push_back({ h, between(habitat.minAltitude, habitat.maxAltitude) });
        }
        // De la más alta a la más baja: cada una se pega a la de altura más parecida y el relieve baja gradualmente.
        std::stable_sort(requests.begin(), requests.end(), [](const Request& a, const Request& b) { return a.altitude > b.altitude; });
        for (const Request& request : requests) {
            const Habitat& habitat = data.habitats[request.habitat];
            Zone zone;
            zone.habitat = request.habitat;
            zone.radius = between(habitat.minRadius, habitat.maxRadius);
            zone.stretch = between(habitat.minStretch, (std::max)(habitat.minStretch, habitat.maxStretch));
            zone.altitude = request.altitude;
            zone.roughness = habitat.roughness;
            for (float& phase : zone.lobe) phase = between(0.0f, 6.2831853f);
            const bool coast = habitat.placement == HabitatPlacement::COAST;
            float bestCost = 1.0e9f, bestX = 0.0f, bestZ = 0.0f, bestAngle = 0.0f;
            for (int attempt = 0; attempt < 160; ++attempt) {
                const Island& island = m_islands[std::uniform_int_distribution<size_t>(0, m_islands.size() - 1)(*m_random)];
                const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f)) * island.r * 1.05f;
                const float x = island.x + std::cos(a) * r, z = island.z + std::sin(a) * r;
                const float s = coastSigned(x, z);
                if (coast ? (s < 8.0f || s > 26.0f) : s < (std::min)(0.45f * zone.radius, 55.0f)) continue;
                if (!coast && x * x + z * z < 30.0f * 30.0f && zone.altitude < 0.9f) continue; // el inicio no se moja
                float cost = 0.0f;
                bool spaced = true;
                int neighbours = 0;
                for (const Zone& other : m_zones) {
                    const float dx = x - other.x, dz = z - other.z;
                    const float reach = zone.radius + other.radius;
                    const float d2 = dx * dx + dz * dz;
                    if (d2 < (reach * SPACING) * (reach * SPACING) && !coast) { spaced = false; break; }
                    if (d2 < (reach * 1.15f) * (reach * 1.15f)) { ++neighbours; cost = (std::max)(cost, std::fabs(zone.altitude - other.altitude)); }
                }
                if (!spaced) continue;
                if (neighbours == 0 && !m_zones.empty()) cost = 60.0f;
                cost += between(0.0f, 1.5f);
                if (cost < bestCost) {
                    bestCost = cost; bestX = x; bestZ = z;
                    const float gx = coastSigned(x + 3.0f, z) - coastSigned(x - 3.0f, z), gz = coastSigned(x, z + 3.0f) - coastSigned(x, z - 3.0f);
                    bestAngle = coast ? std::atan2(gx, -gz) : between(0.0f, 6.2831853f); // las playas se alargan a lo largo de la costa
                }
            }
            if (bestCost > 1.0e8f) { // no cupo con las reglas: dentro de la isla principal
                bestX = between(-0.3f, 0.3f) * m_islands.front().r;
                bestZ = between(-0.3f, 0.3f) * m_islands.front().r;
            }
            zone.x = bestX; zone.z = bestZ;
            zone.cosA = std::cos(bestAngle); zone.sinA = std::sin(bestAngle);
            m_zones.push_back(zone);
        }
        m_sites = m_zones;
    }

    static float distance2(const Zone& zone, float x, float z) {
        const float dx = x - zone.x, dz = z - zone.z;
        const float u = (dx * zone.cosA + dz * zone.sinA) / (zone.radius * zone.stretch);
        const float v = (dz * zone.cosA - dx * zone.sinA) * zone.stretch / zone.radius;
        float lobes = 1.0f; // el radio varía con el ángulo: el contorno deja de ser una elipse perfecta
        const float angle = std::atan2(v, u);
        for (int k = 0; k < LOBES; ++k) lobes += LOBE_AMPLITUDE[k] * std::sin(static_cast<float>(LOBE_HARMONIC[k]) * angle + zone.lobe[k]);
        return (u * u + v * v) / (lobes * lobes);
    }

    // Pesos de las zonas de tierra en (x, z) (suman 1) y altura / rugosidad medias.
    struct Mix { float altitude = 0.0f, roughness = 0.0f; };
    Mix shape(float x, float z, float* w) const {
        const float ox = x, oz = z;
        x += 10.0f * m_noise.fbm(ox * 0.02f + 40.0f, oz * 0.02f, 2); // las fronteras entre zonas serpentean
        z += 10.0f * m_noise.fbm(ox * 0.02f, oz * 0.02f + 40.0f, 2);
        Mix mix;
        const size_t count = (std::min)(m_zones.size(), static_cast<size_t>(MAX_ZONES));
        if (count == 0) return mix;
        float nearest = 1.0e9f, total = 0.0f;
        float d2[MAX_ZONES];
        for (size_t i = 0; i < count; ++i) { d2[i] = distance2(m_zones[i], x, z); nearest = (std::min)(nearest, d2[i]); }
        for (size_t i = 0; i < count; ++i) { w[i] = std::exp(-FALLOFF * (d2[i] - nearest)); total += w[i]; }
        for (size_t i = 0; i < count; ++i) {
            w[i] /= total;
            mix.altitude += w[i] * m_zones[i].altitude;
            mix.roughness += w[i] * m_zones[i].roughness;
        }
        return mix;
    }

    // Altura del terreno natural (sin agua) en (x, z); s = distancia con signo a la costa.
    float landHeight(float x, float z, float s) const {
        constexpr float SEA_SCALE = 140.0f;
        if (s < 0.0f) { // mar: el fondo baja con la distancia a la costa
            const float bed = SEA_FLOOR * (1.0f - std::exp(-(-s) / SEA_SCALE));
            return bed + 1.3f * m_noise.fbm(x * 0.03f, z * 0.03f, 3) * (std::min)(1.0f, -s / 40.0f);
        }
        float w[MAX_ZONES];
        const Mix mix = shape(x, z, w);
        const float mountain = smooth(8.0f, 18.0f, mix.altitude); // solo las zonas altas se erizan en crestas
        float detail = 0.6f * m_noise.fbm(x * 0.018f, z * 0.018f, 5) + 0.25f * m_noise.fbm(x * 0.09f, z * 0.09f, 2);
        detail += mountain * 1.2f * (m_noise.ridge(x * 0.022f, z * 0.022f, 4) - 0.45f);
        float t = mix.altitude + mix.roughness * detail;
        const float start = 1.0f - smooth(PLATEAU_FLAT, PLATEAU_FADE, std::sqrt(x * x + z * z)); // inicio llano
        t += (Physics::World::START_HEIGHT - t) * start;
        // La altura máxima crece con la distancia a la costa: la isla sube gradualmente desde la playa.
        const float cap = 0.18f * s + 0.42f * (std::max)(0.0f, s - 25.0f);
        constexpr float K = 1.2f;
        return -K * std::log(std::exp(-t / K) + std::exp(-cap / K));
    }

    // ---- Relieve base ----
    void bakeGround() {
        const float half = Physics::World::HALF_SIZE;
        m_ground.init(half, SEA_FLOOR);
        m_coast.init(half, -300.0f);
        const int n = m_ground.count();
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                const float x = m_ground.nodeCoord(col), z = m_ground.nodeCoord(row);
                const float s = coastSigned(x, z);
                m_coast.at(col, row) = (std::max)(s, -300.0f);
                m_ground.at(col, row) = landHeight(x, z, s);
            }
        }
        m_surface = m_ground;
        m_sea.init(half, 0.0f);
        m_flowX.init(half, 0.0f);
        m_flowZ.init(half, 0.0f);
        m_riverDist.init(half, 999.0f);
        m_lakeDist.init(half, 999.0f);
        m_level.assign(static_cast<size_t>(n) * n, -1.0e9f);
        m_kind.assign(static_cast<size_t>(n) * n, DRY);
    }

    // ---- Lagos y charcas ----
    // Aplana la zona, excava la cuenca y fija el nivel del agua (que queda siempre por debajo del borde).
    void carveBasin(float cx, float cz, float radius, float depth, float level, Water kind, float h0) {
        const float reach = radius * 2.4f;
        const int c0 = nearestCol(cx - reach), c1 = nearestCol(cx + reach), r0 = nearestCol(cz - reach), r1 = nearestCol(cz + reach);
        const float seed = between(0.0f, 100.0f);
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const float x = m_ground.nodeCoord(col), z = m_ground.nodeCoord(row);
                const float wobble = 1.0f + 0.22f * m_noise.fbm((x - cx) * 0.07f + seed, (z - cz) * 0.07f - seed, 3); // orilla irregular
                const float d = std::sqrt((x - cx) * (x - cx) + (z - cz) * (z - cz)) / wobble;
                if (d > reach) continue;
                float& g = m_ground.at(col, row);
                g += (h0 - g) * (1.0f - smooth(radius, radius * 2.3f, d));
                if (d < radius) {
                    const float u2 = (d / radius) * (d / radius);
                    g = (std::min)(g, level - depth * (1.0f - u2) + 0.55f * u2 * u2 * u2);
                }
                if (d < radius * 1.02f && g < level) {
                    const size_t index = static_cast<size_t>(row) * m_ground.count() + col;
                    m_level[index] = level;
                    m_kind[index] = kind;
                }
            }
        }
    }

    // Altura media del terreno alrededor de un punto.
    float averageHeight(float x, float z, float radius) const {
        float sum = ground(x, z);
        for (int i = 0; i < 8; ++i) sum += ground(x + std::cos(i * 0.785398f) * radius, z + std::sin(i * 0.785398f) * radius);
        return sum / 9.0f;
    }

    void carveLakes(const GameData& data, bool ponds) {
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& habitat = data.habitats[h];
            const bool pondHabitat = habitat.placement == HabitatPlacement::POND;
            const bool lake = habitat.placement == HabitatPlacement::LAKE || habitat.placement == HabitatPlacement::UPPER_LAKE;
            if (pondHabitat != ponds || !(pondHabitat || lake)) continue;
            const Water kind = pondHabitat ? POND : habitat.placement == HabitatPlacement::LAKE ? LAKE : UPPER_LAKE;
            std::vector<Zone> hosts; // charcas: las zonas del hábitat anfitrión (pantano)
            if (pondHabitat) for (const Zone& zone : m_zones) if (data.habitats[zone.habitat].id == habitat.besideId) hosts.push_back(zone);
            const int perHost = betweenInt((std::max)(1, habitat.minZones), (std::max)(1, habitat.maxZones));
            const int total = pondHabitat ? static_cast<int>(hosts.size()) * perHost : perHost;
            for (int i = 0; i < total; ++i) {
                const float radius = between(habitat.minRadius, habitat.maxRadius);
                const float depth = -between(habitat.minAltitude, habitat.maxAltitude);
                float cx = 0.0f, cz = 0.0f, h0 = 0.0f;
                bool found = false;
                for (int attempt = 0; attempt < 250 && !found; ++attempt) {
                    if (pondHabitat) {
                        const Zone& host = hosts[i % hosts.size()];
                        const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f)) * host.radius * 0.8f;
                        cx = host.x + std::cos(a) * r; cz = host.z + std::sin(a) * r;
                    } else {
                        const Island& island = m_islands[std::uniform_int_distribution<size_t>(0, m_islands.size() - 1)(*m_random)];
                        const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f)) * island.r * 0.9f;
                        cx = island.x + std::cos(a) * r; cz = island.z + std::sin(a) * r;
                    }
                    const float s = coastSigned(cx, cz);
                    h0 = averageHeight(cx, cz, radius * 0.8f);
                    if (s < radius * 1.4f + (pondHabitat ? 8.0f : kind == UPPER_LAKE ? 55.0f : 40.0f)) continue;
                    if (cx * cx + cz * cz < (PLATEAU_FADE + radius) * (PLATEAU_FADE + radius)) continue;
                    if (kind == UPPER_LAKE ? (h0 < 9.0f || h0 > 20.0f) : kind == LAKE ? (h0 < 1.5f || h0 > 9.0f) : h0 > 2.0f) continue;
                    bool apart = true;
                    for (const Basin& other : m_basins) {
                        const float dx = cx - other.x, dz = cz - other.z, need = (radius + other.radius) * 1.8f + 10.0f;
                        if (dx * dx + dz * dz < need * need) apart = false;
                    }
                    found = apart;
                }
                if (!found) continue;
                const float level = h0 - (pondHabitat ? 0.2f : 0.4f);
                carveBasin(cx, cz, radius, depth, level, kind, h0);
                m_basins.push_back({ cx, cz, radius, level, kind });
            }
        }
    }

    // ---- Ríos ----
    // Camino de un río: baja por la máxima pendiente (con meandros) hasta el mar.
    std::vector<PathPoint> tracePath(float x, float z) {
        std::vector<PathPoint> path;
        float dx = 0.0f, dz = 0.0f;
        const float seed = between(0.0f, 100.0f);
        for (int step = 0; step < 1100; ++step) {
            const float gx = (ground(x + 5.0f, z) - ground(x - 5.0f, z)) / 10.0f, gz = (ground(x, z + 5.0f) - ground(x, z - 5.0f)) / 10.0f;
            const float sx = (m_coast.sample(x + 5.0f, z) - m_coast.sample(x - 5.0f, z)) / 10.0f, sz = (m_coast.sample(x, z + 5.0f) - m_coast.sample(x, z - 5.0f)) / 10.0f;
            float tx = -gx - 0.10f * sx, tz = -gz - 0.10f * sz;
            float length = std::sqrt(tx * tx + tz * tz);
            if (length < 1.0e-5f) { tx = -sx; tz = -sz; length = std::sqrt(tx * tx + tz * tz); if (length < 1.0e-5f) break; }
            tx /= length; tz /= length;
            const float meander = 0.55f * m_noise.value(x * 0.025f + seed, z * 0.025f);
            dx = 0.6f * dx + 0.4f * (tx - tz * meander);
            dz = 0.6f * dz + 0.4f * (tz + tx * meander);
            length = std::sqrt(dx * dx + dz * dz);
            if (length < 1.0e-5f) break;
            dx /= length; dz /= length;
            path.push_back({ x, z, ground(x, z), 0.0f, 0.0f, 0.0f, dx, dz, false });
            if (path.back().terrain < 0.25f && m_coast.sample(x, z) < 12.0f) break; // llegó al mar
            if (m_coast.sample(x, z) < -4.0f) break;
            x += dx * Physics::Heightfield::CELL; z += dz * Physics::Heightfield::CELL;
            if (std::fabs(x) > Physics::World::HALF_SIZE - 4.0f || std::fabs(z) > Physics::World::HALF_SIZE - 4.0f) break;
        }
        std::vector<float> smoothed(path.size());
        for (size_t i = 0; i < path.size(); ++i) { // alisa el terreno a lo largo del cauce
            float sum = 0.0f;
            int k = 0;
            for (int j = -4; j <= 4; ++j) { const int index = static_cast<int>(i) + j; if (index >= 0 && index < static_cast<int>(path.size())) { sum += path[index].terrain; ++k; } }
            smoothed[i] = sum / k;
        }
        for (size_t i = 0; i < path.size(); ++i) path[i].terrain = smoothed[i];
        return path;
    }

    struct RiverStyle { float widthMin, widthMax, depthMin, depthMax; };

    // Excava un río: el nivel del agua solo baja; con 'fall' hay una cascada (caída forzada) en el tramo más empinado.
    bool carveRiver(float startX, float startZ, float startLevel, bool fall, const RiverStyle& style) {
        std::vector<PathPoint> path = tracePath(startX, startZ);
        if (path.size() < (fall ? 45u : 24u)) return false;
        const int n = static_cast<int>(path.size());
        int cascade = -1;
        float drop = 0.0f;
        if (fall) {
            float bestDrop = 0.0f;
            for (int i = static_cast<int>(0.18f * n); i < static_cast<int>(0.65f * n) && i + 25 < n; ++i) {
                const float d = path[i].terrain - path[i + 25].terrain;
                if (path[i].terrain >= 3.0f && d > bestDrop) { bestDrop = d; cascade = i; }
            }
            if (cascade < 0) cascade = n / 3;
            drop = (std::clamp)(between(3.0f, 5.2f), 2.2f, (std::max)(2.2f, 0.8f * bestDrop));
        }
        float level = startLevel > -1.0e8f ? startLevel : path.front().terrain - 0.35f;
        constexpr int FALL_STEPS = 6;
        for (int i = 0; i < n; ++i) {
            PathPoint& p = path[i];
            if (fall && i >= cascade && i < cascade + FALL_STEPS) { level -= drop / FALL_STEPS; p.cascade = true; }
            else level = (std::min)(level, p.terrain - 0.35f);
            level = (std::max)(level, 0.12f);
            const float t = static_cast<float>(i) / n;
            p.level = level;
            p.halfWidth = style.widthMin + (style.widthMax - style.widthMin) * (0.4f * t + 0.6f * t * t);
            p.bed = level - between(style.depthMin, style.depthMax);
        }
        // reparte el cauce por los nodos cercanos: cada nodo toma el punto más próximo
        const int c = m_ground.count();
        std::vector<int> touched;
        std::vector<float> best(static_cast<size_t>(c) * c, 1.0e9f);
        std::vector<int> owner(static_cast<size_t>(c) * c, -1);
        for (int i = 0; i < n; ++i) {
            const PathPoint& p = path[i];
            const float reach = p.halfWidth + 10.0f;
            const int c0 = nearestCol(p.x - reach), c1 = nearestCol(p.x + reach), r0 = nearestCol(p.z - reach), r1 = nearestCol(p.z + reach);
            for (int row = r0; row <= r1; ++row) {
                for (int col = c0; col <= c1; ++col) {
                    const float ex = m_ground.nodeCoord(col) - p.x, ez = m_ground.nodeCoord(row) - p.z;
                    const float d = std::sqrt(ex * ex + ez * ez);
                    const size_t index = static_cast<size_t>(row) * c + col;
                    if (d < best[index]) {
                        if (owner[index] < 0) touched.push_back(static_cast<int>(index));
                        best[index] = d; owner[index] = i;
                    }
                }
            }
        }
        for (const int index : touched) {
            const int i = owner[index];
            const PathPoint& p = path[i];
            const float d = best[index], hw = p.halfWidth;
            const float bank = d < hw ? p.bed + (p.level - p.bed) * (d / hw) * (d / hw) : p.level + (d - hw) * 0.7f;
            float& g = m_ground.data()[index];
            if (bank >= g) continue;
            g = bank;
            if (d < hw + 0.1f && g < p.level) {
                const bool upper = fall && i < cascade;
                m_level[index] = p.level;
                m_kind[index] = p.cascade ? CASCADE : upper ? UPPER_RIVER : RIVER;
                const float speed = p.cascade ? 4.5f : upper ? 1.6f : 1.1f;
                m_flowX.data()[index] = p.tx * speed;
                m_flowZ.data()[index] = p.tz * speed;
            }
        }
        return true;
    }

    // Punto de salida de un lago: el lado más bajo de su orilla.
    bool outlet(const Basin& lake, float& x, float& z, float minCoast) const {
        float lowest = 1.0e9f;
        for (int k = 0; k < 16; ++k) {
            const float a = k * 0.392699f, px = lake.x + std::cos(a) * lake.radius * 1.15f, pz = lake.z + std::sin(a) * lake.radius * 1.15f;
            if (coastSigned(px, pz) < minCoast) continue;
            const float t = ground(px, pz);
            if (t < lowest) { lowest = t; x = px; z = pz; }
        }
        return lowest < 1.0e8f;
    }

    void carveRivers(const GameData& data) {
        const Habitat* river = nullptr;
        const Habitat* upper = nullptr;
        for (const Habitat& habitat : data.habitats) {
            if (habitat.placement == HabitatPlacement::RIVER) river = &habitat;
            if (habitat.placement == HabitatPlacement::UPPER_RIVER) upper = &habitat;
        }
        if (!river && !upper) return;
        const RiverStyle style{ river ? river->minRadius : 2.4f, river ? river->maxRadius : 3.4f, river ? -river->maxAltitude : 1.0f, river ? -river->minAltitude : 1.6f };
        // Ríos con cascada: nacen en un lago alto o, si no hay, en un punto alto de la montaña.
        const int falls = upper ? betweenInt((std::max)(1, upper->minZones), (std::max)(1, upper->maxZones)) : 0;
        std::vector<Basin> sources;
        for (const Basin& basin : m_basins) if (basin.kind == UPPER_LAKE) sources.push_back(basin);
        int made = 0;
        for (int i = 0; made < falls && i < 40; ++i) {
            float x = 0.0f, z = 0.0f, level = -1.0e9f;
            if (i < static_cast<int>(sources.size())) {
                if (!outlet(sources[i], x, z, 20.0f)) continue;
                level = sources[i].level;
            } else {
                float bestHeight = 0.0f;
                for (int k = 0; k < 120; ++k) {
                    const Island& island = m_islands.front();
                    const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f)) * island.r * 0.8f;
                    const float px = island.x + std::cos(a) * r, pz = island.z + std::sin(a) * r, t = ground(px, pz);
                    if (t > bestHeight && t < SNOW_LINE - 3.0f && t > 8.0f && coastSigned(px, pz) > 40.0f) { bestHeight = t; x = px; z = pz; }
                }
                if (bestHeight <= 0.0f) continue;
            }
            if (carveRiver(x, z, level, true, style)) ++made;
        }
        // Ríos normales: salen de lagos medios y bajan al mar.
        const int plain = river ? betweenInt((std::max)(1, river->minZones), (std::max)(1, river->maxZones)) : 0;
        made = 0;
        for (const Basin& lake : m_basins) {
            if (made >= plain) break;
            float x = 0.0f, z = 0.0f;
            if (lake.kind != LAKE || !outlet(lake, x, z, 10.0f)) continue;
            if (carveRiver(x, z, lake.level, false, style)) ++made;
        }
    }

    // ---- Agua final: nivel, mar, distancias ----
    void finishWater() {
        const int n = m_ground.count();
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                const size_t index = static_cast<size_t>(row) * n + col;
                const float g = m_ground.at(col, row), s = m_coast.at(col, row);
                if (m_kind[index] != DRY && g < m_level[index]) {
                    m_surface.at(col, row) = m_level[index];
                } else {
                    m_kind[index] = DRY;
                    m_flowX.at(col, row) = m_flowZ.at(col, row) = 0.0f;
                    if (g < 0.8f && s < 30.0f) { // mar (y orilla que sube y baja con la marea)
                        m_surface.at(col, row) = 0.0f;
                        m_sea.at(col, row) = 1.0f;
                        if (g < 0.0f) m_kind[index] = SEA;
                    } else {
                        m_surface.at(col, row) = g;
                    }
                }
            }
        }
        distanceField(m_riverDist, [](uint8_t k) { return k == RIVER || k == UPPER_RIVER || k == CASCADE; });
        distanceField(m_lakeDist, [](uint8_t k) { return k == LAKE || k == UPPER_LAKE; });
    }

    // Distancia (m) al agua de una familia, en dos pasadas (chamfer).
    template <typename F>
    void distanceField(Physics::Heightfield& out, F isFamily) {
        const int n = m_ground.count();
        constexpr float STEP = Physics::Heightfield::CELL, DIAGONAL = STEP * 1.4142f;
        for (int row = 0; row < n; ++row) for (int col = 0; col < n; ++col) out.at(col, row) = isFamily(m_kind[static_cast<size_t>(row) * n + col]) ? 0.0f : 999.0f;
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                float& v = out.at(col, row);
                if (col > 0) v = (std::min)(v, out.at(col - 1, row) + STEP);
                if (row > 0) {
                    v = (std::min)(v, out.at(col, row - 1) + STEP);
                    if (col > 0) v = (std::min)(v, out.at(col - 1, row - 1) + DIAGONAL);
                    if (col + 1 < n) v = (std::min)(v, out.at(col + 1, row - 1) + DIAGONAL);
                }
            }
        }
        for (int row = n - 1; row >= 0; --row) {
            for (int col = n - 1; col >= 0; --col) {
                float& v = out.at(col, row);
                if (col + 1 < n) v = (std::min)(v, out.at(col + 1, row) + STEP);
                if (row + 1 < n) {
                    v = (std::min)(v, out.at(col, row + 1) + STEP);
                    if (col + 1 < n) v = (std::min)(v, out.at(col + 1, row + 1) + DIAGONAL);
                    if (col > 0) v = (std::min)(v, out.at(col - 1, row + 1) + DIAGONAL);
                }
            }
        }
    }

    // ---- Regiones especiales ----
    static float averageRadius(const Habitat& habitat) { return 0.5f * (habitat.minRadius + habitat.maxRadius); }

    void buildRegions(const GameData& data) {
        m_under.init(m_ground);
        for (const HabitatPlacement kind : { HabitatPlacement::CAVE, HabitatPlacement::MINE }) {
            for (int h = 0; h < m_habitats; ++h) {
                if (data.habitats[h].placement != kind) continue;
                const int count = betweenInt(data.habitats[h].minZones, data.habitats[h].maxZones);
                for (int i = 0; i < count; ++i) buildSystem(data, h, kind);
            }
        }
        buildSky(data);
    }

    // Un sistema de cuevas o minas: la boca se elige en una ladera de la isla principal (mejor cuanto más empinada), lejos de la costa, del
    // inicio y de otros sistemas; las galerías (Underground) se extienden hacia dentro de la montaña.
    void buildSystem(const GameData& data, int habitat, HabitatPlacement kind) {
        const Habitat& info = data.habitats[habitat];
        Underground::Plan plan;
        plan.kind = kind == HabitatPlacement::CAVE ? Underground::Kind::CAVE : Underground::Kind::MINE;
        plan.habitat = habitat;
        plan.radius = between(info.minRadius, info.maxRadius);
        plan.water = m_deepWater;
        for (const int hall : m_hallHabitats) { // las salas de este sistema: las de su hábitat, las de su clase (cueva o mina) y las de cualquiera
            const int host = data.habitatIndex(data.habitats[hall].besideId);
            if (host < 0 || host == habitat || data.habitats[host].placement == kind) plan.halls.push_back(hall);
        }
        const Island& island = m_islands.front();
        struct Candidate { float x, z, dirX, dirZ, score; };
        std::vector<Candidate> candidates;
        for (const float tier : { 1.0f, 0.6f, 0.25f }) { // si no hay sitio ideal, se relajan las condiciones
            candidates.clear();
            for (int i = 0; i < 900; ++i) {
                const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f)) * island.r * 0.95f;
                const float x = island.x + std::cos(a) * r, z = island.z + std::sin(a) * r;
                const float h = ground(x, z);
                if (h < 5.0f || h > 38.0f || coastSigned(x, z) < 30.0f * tier || std::sqrt(x * x + z * z) < PLATEAU_FADE + 10.0f || !dryAround(x, z, 9.0f)) continue;
                bool apart = true;
                for (const Region& other : m_regions) {
                    if (!other.dark()) continue;
                    const float dx = x - other.mouthX, dz = z - other.mouthZ, need = (plan.radius + other.radius) * 0.7f * tier;
                    if (dx * dx + dz * dz < need * need) apart = false;
                }
                if (!apart) continue;
                float gx, gz;
                m_ground.gradient(x, z, gx, gz);
                const float slope = std::sqrt(gx * gx + gz * gz);
                const float dirAngle = slope > 0.15f ? std::atan2(gz, gx) : between(0.0f, 6.2831853f); // hacia lo alto de la ladera
                candidates.push_back({ x, z, std::cos(dirAngle), std::sin(dirAngle), (std::min)(slope, 0.9f) * 10.0f + between(0.0f, 4.0f) + h * 0.05f });
            }
            std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
            for (size_t i = 0; i < candidates.size() && i < 10; ++i) {
                Underground::System system;
                const Candidate& c = candidates[i];
                if (!m_under.carve(plan, { c.x, c.z, c.dirX, c.dirZ }, { m_ground, m_surface, m_kind }, m_noise, *m_random, system) || system.radius < 6.0f) continue;
                Region region;
                region.habitat = habitat;
                region.kind = kind;
                region.x = system.x; region.z = system.z; region.radius = system.radius;
                region.mouthX = system.mouthX; region.mouthZ = system.mouthZ; region.dirX = system.dirX; region.dirZ = system.dirZ;
                region.floorY = system.floorY;
                m_regions.push_back(region);
                Zone site;
                site.habitat = habitat; site.x = system.x; site.z = system.z; site.radius = system.radius * 0.7f; site.deep = true;
                m_sites.push_back(site);
                return;
            }
        }
    }

    bool dryAround(float cx, float cz, float radius) const {
        if (m_kind[nearestNode(cx, cz)] != DRY) return false;
        for (int k = 0; k < 16; ++k) {
            const float x = cx + std::cos(k * 0.392699f) * radius, z = cz + std::sin(k * 0.392699f) * radius;
            if (m_kind[nearestNode(x, z)] != DRY || ground(x, z) < 0.8f) return false;
        }
        return true;
    }

    // ---- Islas del cielo ----
    // Islas de nube de varios tamaños, más allá del archipiélago. Cada una: un suelo de nube (hábitat SKY) que termina en una orilla y un mar de
    // nubes (SKY_SEA) que la rodea; en el mar hay parcelas de tierra (SKY_LAND, de hábitats distintos) a las que se llega nadando; más allá, la
    // nube se hunde en un abismo blanco (el renderizador lo tapa con nubes). Se baja caminando o nadando hasta el borde y dejándose caer. Para
    // subir, cada isla tiene su propia Corriente Ascendente en el mar del archipiélago.
    public:
    static constexpr float SKY_SHORE = 7.0f;      // ancho (m) de la orilla del suelo de nube al mar de nubes
    static constexpr float SKY_SEA_DEPTH = 3.2f;  // profundidad del mar de nubes bajo el suelo
    static constexpr float SKY_ABYSS = 12.0f;     // ancho (m) del talud blanco más allá del mar
    static constexpr float SKY_ABYSS_DROP = 55.0f;

    // Contorno irregular de una isla: factor (≈1) por el que se divide la distancia al centro según la dirección (dx, dz: desde su centro).
    static float skyShape(const Region& region, float dx, float dz) {
        const float angle = std::atan2(dz, dx);
        float f = 1.0f;
        for (int i = 0; i < LOBES; ++i) f += 0.6f * LOBE_AMPLITUDE[i] * std::sin(LOBE_HARMONIC[i] * angle + region.lobe[i]);
        return f;
    }
    private:

    void buildSky(const GameData& data) {
        int sea = -1;
        for (int h = 0; h < m_habitats; ++h) if (data.habitats[h].placement == HabitatPlacement::SKY_SEA) { sea = h; break; }
        const float half = Physics::World::HALF_SIZE - 2.0f;
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& info = data.habitats[h];
            if (info.placement != HabitatPlacement::SKY) continue;
            const int count = betweenInt(info.minZones, info.maxZones);
            for (int i = 0; i < count; ++i) {
                Region region;
                region.habitat = h;
                region.kind = HabitatPlacement::SKY;
                region.floorRadius = between(info.minRadius, info.maxRadius);
                const float ring = sea >= 0 ? between(data.habitats[sea].minRadius, data.habitats[sea].maxRadius) : 0.0f;
                region.seaRadius = region.floorRadius + ring;
                region.radius = region.seaRadius + SKY_ABYSS;
                region.floorY = between(info.minAltitude, (std::max)(info.minAltitude, info.maxAltitude));
                for (float& phase : region.lobe) phase = between(0.0f, 6.2831853f);
                float widest = 1.0f;
                for (int k = 0; k < 90; ++k) widest = (std::max)(widest, skyShape(region, std::cos(k * 0.0698f), std::sin(k * 0.0698f)));
                bool placed = false;
                for (int attempt = 0; attempt < 500 && !placed; ++attempt) {
                    const float R = region.radius * widest; // contorno irregular incluido
                    const float limit = half - R;
                    if (limit <= 0.0f) break;
                    region.x = between(-limit, limit);
                    region.z = between(-limit, limit);
                    if (std::sqrt(region.x * region.x + region.z * region.z) < m_worldRadius + R + 6.0f) continue; // no pisa el archipiélago
                    placed = true;
                    for (const Region& other : m_regions) {
                        if (other.kind != HabitatPlacement::SKY) continue;
                        const float dx = region.x - other.x, dz = region.z - other.z, need = R + other.radius * 1.15f + 6.0f;
                        if (dx * dx + dz * dz < need * need) placed = false;
                    }
                }
                if (!placed) continue;
                m_regions.push_back(region);
                bakeSky(m_regions.back());
                Zone site;
                site.habitat = h; site.x = region.x; site.z = region.z; site.radius = region.floorRadius * 0.9f;
                m_sites.push_back(site);
            }
        }
        // Parcelas de tierra en los mares de nubes (de cada hábitat SKY_LAND, repartidas entre las islas), y una corriente ascendente por isla.
        for (int h = 0; h < m_habitats; ++h) {
            if (data.habitats[h].placement == HabitatPlacement::SKY_LAND) scatterPatches(data, h);
        }
        for (const Region& region : m_regions) if (region.kind == HabitatPlacement::SKY) placeUpdraft(region);
    }

    // Suelo, mar y abismo de una isla: el relieve, el agua de nube (nivel un poco bajo el suelo) y lo que no existe fuera de ella.
    void bakeSky(const Region& region) {
        const int n = m_ground.count();
        const float reach = region.radius * 1.35f + 6.0f;
        const int c0 = nearestCol(region.x - reach), c1 = nearestCol(region.x + reach), r0 = nearestCol(region.z - reach), r1 = nearestCol(region.z + reach);
        const float H = region.floorY, level = H - 0.35f;
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const float x = m_ground.nodeCoord(col), z = m_ground.nodeCoord(row);
                const float d = std::sqrt((x - region.x) * (x - region.x) + (z - region.z) * (z - region.z)) / skyShape(region, x - region.x, z - region.z);
                const size_t index = static_cast<size_t>(row) * n + col;
                m_kind[index] = DRY;
                m_level[index] = -1.0e9f;
                m_flowX.at(col, row) = m_flowZ.at(col, row) = 0.0f;
                m_sea.at(col, row) = 0.0f;
                m_coast.at(col, row) = 50.0f;
                float g;
                if (d > region.radius) {
                    g = SKY_VOID;
                } else {
                    const float floorG = H + 0.3f * std::fabs(m_noise.fbm(x * 0.05f + 3.0f, z * 0.05f, 3));
                    const float seaG = H - SKY_SEA_DEPTH + 0.5f * m_noise.fbm(x * 0.08f, z * 0.08f + 9.0f, 3);
                    const float shore = smooth(region.floorRadius, region.floorRadius + SKY_SHORE, d);
                    g = floorG + (seaG - floorG) * shore;
                    if (d > region.seaRadius) { // el abismo: la nube blanca se hunde
                        const float t = smooth(region.seaRadius, region.radius, d);
                        g = seaG - SKY_ABYSS_DROP * std::pow(t, 1.4f);
                    } else if (g < level - 0.05f) {
                        m_kind[index] = CLOUD;
                        m_level[index] = level;
                    }
                }
                m_ground.at(col, row) = g;
                m_surface.at(col, row) = g;
            }
        }
    }

    // Reparte las zonas de un hábitat SKY_LAND por los mares de nubes de las islas.
    void scatterPatches(const GameData& data, int habitat) {
        const Habitat& info = data.habitats[habitat];
        const int count = betweenInt((std::max)(1, info.minZones), (std::max)(1, info.maxZones));
        std::vector<size_t> islands;
        for (size_t i = 0; i < m_regions.size(); ++i) if (m_regions[i].kind == HabitatPlacement::SKY && m_regions[i].seaRadius - m_regions[i].floorRadius > 24.0f) islands.push_back(i);
        if (islands.empty()) return;
        for (int i = 0; i < count; ++i) {
            for (int attempt = 0; attempt < 80; ++attempt) {
                const Region& region = m_regions[islands[static_cast<size_t>(betweenInt(0, static_cast<int>(islands.size()) - 1))]];
                const float radius = (std::min)(between(info.minRadius, info.maxRadius), (region.seaRadius - region.floorRadius - SKY_SHORE) * 0.3f);
                const float inner = region.floorRadius + SKY_SHORE + radius * 1.3f + 2.0f, outer = region.seaRadius - radius * 1.3f - 2.0f;
                if (radius < 3.0f || outer <= inner) continue;
                const float angle = between(0.0f, 6.2831853f), dist = between(inner, outer);
                const float shape = skyShape(region, std::cos(angle), std::sin(angle));
                Zone zone;
                zone.habitat = habitat;
                zone.x = region.x + std::cos(angle) * dist * shape; zone.z = region.z + std::sin(angle) * dist * shape;
                zone.radius = radius;
                zone.stretch = between(info.minStretch, (std::max)(info.minStretch, info.maxStretch));
                zone.altitude = between(info.minAltitude, info.maxAltitude);
                zone.roughness = info.roughness;
                const float tilt = between(0.0f, 6.2831853f);
                zone.cosA = std::cos(tilt); zone.sinA = std::sin(tilt);
                for (float& phase : zone.lobe) phase = between(0.0f, 6.2831853f);
                bool apart = true;
                for (const Zone& other : m_skyZones) {
                    const float dx = zone.x - other.x, dz = zone.z - other.z, need = (zone.radius * zone.stretch + other.radius * other.stretch) * 1.1f + 3.0f;
                    if (dx * dx + dz * dz < need * need) apart = false;
                }
                if (!apart) continue;
                raiseIslet(zone, region.floorY);
                m_skyZones.push_back(zone);
                m_sites.push_back(zone);
                break;
            }
        }
    }

    // Eleva una parcela de tierra desde el fondo del mar de nubes: sube con suavidad desde su borde hasta 'altitude' sobre el suelo de nube.
    void raiseIslet(const Zone& zone, float floorY) {
        const float reach = zone.radius * zone.stretch * 1.8f + 4.0f;
        const int c0 = nearestCol(zone.x - reach), c1 = nearestCol(zone.x + reach), r0 = nearestCol(zone.z - reach), r1 = nearestCol(zone.z + reach);
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const float x = m_ground.nodeCoord(col), z = m_ground.nodeCoord(row);
                const float w = 1.0f - smooth(0.35f, 1.3f, distance2(zone, x, z));
                if (w <= 0.0f) continue;
                const float top = floorY + zone.altitude + zone.roughness * m_noise.fbm(x * 0.06f, z * 0.06f, 3);
                float& g = m_ground.at(col, row);
                g += (top - g) * w;
                m_surface.at(col, row) = g;
            }
        }
    }

    // Corriente Ascendente de una isla: una columna en el mar del archipiélago, lo más cerca posible de ella y lejos de las demás corrientes.
    void placeUpdraft(const Region& sky) {
        float bestX = 0.0f, bestZ = 0.0f, bestScore = -1.0e9f;
        for (int i = 0; i < 1200; ++i) {
            const float a = between(0.0f, 6.2831853f), r = between(m_islands.front().r * 0.6f, m_worldRadius - 8.0f);
            const float x = std::cos(a) * r, z = std::sin(a) * r;
            const float s = coastSigned(x, z);
            if (s > -25.0f || s < -110.0f || ground(x, z) > -4.0f) continue;
            bool apart = true;
            for (const Physics::World::Updraft& other : m_updrafts) {
                const float ux = x - other.x, uz = z - other.z;
                if (ux * ux + uz * uz < 30.0f * 30.0f) apart = false;
            }
            if (!apart) continue;
            const float dx = x - sky.x, dz = z - sky.z;
            const float score = -std::sqrt(dx * dx + dz * dz) + between(0.0f, 12.0f);
            if (score > bestScore) { bestScore = score; bestX = x; bestZ = z; }
        }
        if (bestScore < -1.0e8f) { // no hay mar libre: junto al borde del archipiélago, en la dirección de la isla
            const float k = (m_worldRadius - 14.0f) / (std::max)(std::sqrt(sky.x * sky.x + sky.z * sky.z), 1.0f);
            bestX = sky.x * k; bestZ = sky.z * k;
        }
        float dx = bestX - sky.x, dz = bestZ - sky.z;
        const float length = (std::max)(std::sqrt(dx * dx + dz * dz), 1.0f);
        dx /= length; dz /= length;
        const float inland = sky.floorRadius * 0.5f; // aterriza hacia el borde de la isla que mira a la corriente
        m_updrafts.push_back({ bestX, bestZ, STREAM_RADIUS, sky.floorY + STREAM_CLIMB, sky.x + dx * inland, sky.z + dz * inland });
    }

    void flattenAround(float cx, float cz, float radius) {
        const float h0 = ground(cx, cz);
        const int c0 = nearestCol(cx - radius * 2), c1 = nearestCol(cx + radius * 2), r0 = nearestCol(cz - radius * 2), r1 = nearestCol(cz + radius * 2);
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const float x = m_ground.nodeCoord(col) - cx, z = m_ground.nodeCoord(row) - cz;
                float& g = m_ground.at(col, row);
                g += (h0 - g) * (1.0f - smooth(radius, radius * 2.0f, std::sqrt(x * x + z * z)));
                m_surface.at(col, row) = (std::max)(m_surface.at(col, row), g);
            }
        }
    }

    unsigned m_seed = 0;
    int m_habitats = 0;
    WorldNoise m_noise{ 1u };
    std::mt19937* m_random = nullptr;
    std::vector<Zone> m_zones, m_sites;
    std::vector<Region> m_regions;
    std::vector<Zone> m_skyZones;
    Underground m_under;
    std::vector<int> m_hallHabitats; // hábitats de salas (HALL) de cuevas y minas
    int m_deepWater = -1;            // hábitat del agua subterránea
    std::vector<Physics::World::Updraft> m_updrafts;
    std::vector<Island> m_islands;
    std::vector<Basin> m_basins;
    std::vector<SeaBand> m_bandHabitat;
    int m_waterHabitat[WATER_KINDS] = {};
    int m_peakHabitat = -1, m_riverBank = -1, m_lakeBank = -1;
    float m_bankWidth[2] = { 7.0f, 7.0f };
    float m_worldRadius = 200.0f;

    Physics::Heightfield m_ground, m_surface, m_sea, m_coast, m_flowX, m_flowZ, m_riverDist, m_lakeDist;
    std::vector<float> m_level;
    std::vector<uint8_t> m_kind;
};
