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
    enum Water : uint8_t { DRY = 0, SEA, RIVER, UPPER_RIVER, LAKE, UPPER_LAKE, POND, CASCADE, CLOUD, LAVA, HOT, WATER_KINDS };
    // Capa del mapa: lo que hay bajo tierra (cuevas y minas), la superficie y la isla del cielo.
    enum Layer : int { UNDERGROUND = 0, SURFACE, SKY, LAYERS };

    static constexpr float SNOW_LINE = 24.0f;      // altura (m) desde la que hay nieve y hielo
    static constexpr float SEA_FLOOR = -44.0f;     // fondo del mar abierto: muy hondo, y poco a poco (junto a la orilla es bajo)
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
    static constexpr float MAX_WORLD_RADIUS = 760.0f; // radio máximo del archipiélago (el resto de la malla es mar y las islas del cielo)
    static constexpr float SKY_VOID = -100.0f;     // altura de lo que no existe alrededor de la isla del cielo (no se dibuja)

    struct Zone {
        int habitat = 0; // índice en GameData::habitats
        float x = 0.0f, z = 0.0f, radius = 10.0f;
        float stretch = 1.0f, cosA = 1.0f, sinA = 0.0f;
        float altitude = 3.0f;
        float peak = 0.0f; // volcanes: altura de la cima (altitude es la de la base)
        float roughness = 0.5f;
        float lobe[LOBES] = {};
        bool deep = false; // zona subterránea (sistema de cueva o mina)
        bool feature = false; // accidente propio (volcán, glaciar…): los ríos y lagos lo evitan
    };

    // Región especial: sistema de cueva o mina (bajo tierra, con su boca en (mouthX, mouthZ)) o isla del cielo.
    struct Region {
        int habitat = 0;
        HabitatPlacement kind = HabitatPlacement::CAVE;
        float x = 0.0f, z = 0.0f, radius = 60.0f;
        float mouthX = 0.0f, mouthZ = 0.0f, dirX = 1.0f, dirZ = 0.0f; // boca y dirección hacia dentro (solo cueva y mina)
        float floorY = 0.0f;                                           // altura del suelo (en la boca, en cueva y mina; en el cielo, la del suelo de nube)
        float floorRadius = 0.0f, seaRadius = 0.0f;                    // isla del cielo: radio del suelo de nube y del mar de nubes (radius = el borde de la isla, donde acaba la malla de nube)
        float thickness = 0.0f;                                        // isla del cielo: grosor de la malla de nube en el centro (m)
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
        m_peakHabitat = m_riverBank = m_lakeBank = m_ventHabitat = -1;
        m_overlays.clear();
        m_geysers.clear();
        m_springs.clear();
        std::mt19937 random(seed);
        m_random = &random;

        bindHabitats(data);
        planWorld(data);
        placeZones(data);
        placeSeabed(data);
        bakeGround();
        shapeSeabed();
        raiseVolcanoes();
        carveLakes(data, false);
        carveRivers(data);
        carveLavaFlows();
        placeGeysers(data);
        carveLakes(data, true); // charcas
        buildRegions(data);     // antes de cerrar el agua: la isla del cielo tiene su propio mar de nubes
        finishWater();
        bakeGrip(data);
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
    const std::vector<Physics::World::Geyser>& geysers() const { return m_geysers; }

    float height(float x, float z) const { return m_ground.sample(x, z); }
    // Distancia a la costa (m): positiva en tierra, negativa en el mar.
    float coastDistance(float x, float z) const { return m_coast.sample(x, z); }

    // Temperatura (°C) a una altura dada: baja con la altitud.
    static float temperature(float altitude, float cooling = 1.0f) { return 26.0f - 0.95f * cooling * (std::max)(altitude, 0.0f); }

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

    // ¿Está el punto en una isla del cielo? (allí no se recibe daño por caída)
    bool onSky(float x, float z) const { const Region* r = regionAt(x, z); return r && r->kind == HabitatPlacement::SKY; }
    // ¿Es suelo de nube (blando, rebota) y no una parcela de tierra elevada?
    bool cloudFloor(float x, float z) const {
        const Region* r = regionAt(x, z);
        return r && r->kind == HabitatPlacement::SKY && ground(x, z) < r->floorY + CLOUD_FLOOR_HEIGHT;
    }
    static constexpr float CLOUD_FLOOR_HEIGHT = 1.0f;

    // Capa del mapa en la que está el punto de la superficie: el cielo (islas de nube) o la superficie.
    Layer surfaceLayerAt(float x, float z) const { return regionAt(x, z) ? SKY : SURFACE; }

    // Capa del mapa de quien está en (x, z) a la altura 'feet': bajo tierra (galería), el cielo o la superficie.
    Layer layerAt(float x, float z, float feet) const { return m_under.under(x, z, feet) ? UNDERGROUND : surfaceLayerAt(x, z); }
    // Oscuridad (0..1) de quien está en (x, z) a la altura 'feet': crece con la roca que tiene encima, así entrar es gradual.
    float darkness(float x, float z, float feet) const {
        if (!m_under.under(x, z, feet)) return 0.0f;
        const float dark = smooth(0.5f, DARK_DEPTH, m_ground.sample(x, z) - feet);
        return x * x + z * z > m_worldRadius * m_worldRadius ? dark * CLOUD_CAVE_DARKNESS : dark; // las cuevas de nube son claras
    }
    static constexpr float CLOUD_CAVE_DARKNESS = 0.3f;
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
    // Cuánto se ha enfriado la lava en un nodo (0 = recién salida del cráter, 1 = ya casi roca).
    float lavaCoolAt(int col, int row) const { return m_lavaCool.at(col, row); }
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
            if (kind == SEA) { seaWeights(g, out); applyOverlays(x, z, out, true); }
            else if (m_waterHabitat[kind] >= 0) out[m_waterHabitat[kind]] = 1.0f;
            else if (m_waterHabitat[RIVER] >= 0) out[m_waterHabitat[RIVER]] = 1.0f;
            return;
        }
        float w[MAX_ZONES];
        shape(x, z, w);
        float scale = 1.0f;
        const float peak = m_peakHabitat >= 0 ? smooth(SNOW_LINE - 4.0f, SNOW_LINE + 1.0f, g) * (1.0f - featureWeight(x, z)) : 0.0f;
        if (peak > 0.0f) { out[m_peakHabitat] = peak; scale -= peak; }
        float bank[2] = { 0.0f, 0.0f };
        if (m_riverBank >= 0) bank[0] = 1.0f - smooth(1.5f, m_bankWidth[0], m_riverDist.sample(x, z));
        if (m_lakeBank >= 0) bank[1] = 1.0f - smooth(1.5f, m_bankWidth[1], m_lakeDist.sample(x, z));
        if (bank[0] > 0.0f) out[m_riverBank] = bank[0] * scale;
        if (bank[1] > 0.0f) out[m_lakeBank] += bank[1] * scale * (1.0f - bank[0]);
        const float rest = scale * (1.0f - bank[0]) * (1.0f - bank[1]);
        for (size_t i = 0; i < m_zones.size(); ++i) out[m_zones[i].habitat] += w[i] * rest;
        applyOverlays(x, z, out, false);
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
        world.lavaMask = m_lava;
        world.hotMask = m_hot;
        world.springs = m_springs;
        world.grip = m_grip;
        world.caveGrip = m_caveGrip;
        world.geysers = m_geysers;
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

    // Cuánto pesa un volcán o un glaciar en el punto (0..1): ahí no se pone la nieve de las cumbres.
    float featureWeight(float x, float z) const {
        float best = 0.0f;
        for (const Zone& zone : m_zones) {
            if (!zone.feature) continue;
            const float reach = zone.radius * (std::max)(zone.stretch, 1.0f / zone.stretch) * 1.4f;
            if ((x - zone.x) * (x - zone.x) + (z - zone.z) * (z - zone.z) > reach * reach) continue;
            best = (std::max)(best, 1.0f - smooth(0.7f, 1.1f, distance2(zone, x, z)));
        }
        return best;
    }

    private:
    static constexpr int MAX_ZONES = 160;
    static constexpr float SKY_TRIGGER = 14.0f; // a menos de esto fuera del borde de la isla del cielo, el jugador queda dentro de ella
    static constexpr float SKY_EXIT = 6.0f;     // margen más allá del talud: el jugador puede salir del suelo de nube y caer
    static constexpr float SKY_FALL = 70.0f;    // por debajo de esta altura bajo el suelo de nube deja de pertenecer a la isla
    static constexpr float WET = 0.25f; // con menos agua que esto cuenta como orilla/tierra

    struct Island { float x, z, r, ox, oz, phase1, phase2; };
    struct SeaBand { int habitat; float mid, width; };
    struct PathPoint { float x, z, terrain, level, bed, halfWidth, tx, tz; bool cascade; bool gorge; };
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
                case HabitatPlacement::LAVA: m_waterHabitat[LAVA] = h; break;
                case HabitatPlacement::HOT_SPRING: m_waterHabitat[HOT] = h; break;
                case HabitatPlacement::VENT: m_ventHabitat = h; break;
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
        const float mainRadius = (std::clamp)(std::sqrt(area * 0.8f / 3.14159f) * 1.05f, 260.0f, 360.0f);
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
            zone.feature = habitat.placement == HabitatPlacement::VOLCANO || habitat.placement == HabitatPlacement::GLACIER;
            if (habitat.placement == HabitatPlacement::VOLCANO) zone.peak = zone.radius * between(0.5f, 0.7f);
            for (float& phase : zone.lobe) phase = between(0.0f, 6.2831853f);
            const bool coast = habitat.placement == HabitatPlacement::COAST;
            float bestCost = 1.0e9f, bestX = 0.0f, bestZ = 0.0f, bestAngle = 0.0f;
            for (int attempt = 0; attempt < 160; ++attempt) {
                const Island& island = m_islands[std::uniform_int_distribution<size_t>(0, m_islands.size() - 1)(*m_random)];
                const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f)) * island.r * 1.05f;
                const float x = island.x + std::cos(a) * r, z = island.z + std::sin(a) * r;
                const float s = coastSigned(x, z);
                if (coast ? (s < 8.0f || s > 26.0f) : s < (std::min)((zone.feature ? 0.8f : 0.45f) * zone.radius, zone.feature ? 90.0f : 55.0f)) continue;
                if (zone.feature && x * x + z * z < (PLATEAU_FADE + zone.radius) * (PLATEAU_FADE + zone.radius)) continue; // el inicio queda libre de volcanes y hielo
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
        constexpr float SEA_SCALE = 160.0f;
        if (s < 0.0f) { // mar: el fondo baja con la distancia a la costa, suave junto a la orilla y cada vez más hondo
            const float t = -s / SEA_SCALE;
            const float bed = SEA_FLOOR * (t * t / (1.0f + t * t));
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
        m_lavaCool.init(half, 1.0f);
        m_flowZ.init(half, 0.0f);
        m_riverDist.init(half, 999.0f);
        m_lakeDist.init(half, 999.0f);
        m_level.assign(static_cast<size_t>(n) * n, -1.0e9f);
        m_kind.assign(static_cast<size_t>(n) * n, DRY);
    }

    // ---- Lagos y charcas ----
    // Perfil de una cuenca de agua quieta según u = distancia / radio: fondo llano en el centro, talud suave y una orilla que apenas asoma sobre el
    // nivel del agua (nunca un pozo): el agua llega hasta u = 1.
    static float basinFloor(float u, float level, float depth) {
        const float shelf = 1.0f - smooth(0.45f, 1.0f, u);
        return level + 0.25f - (depth + 0.25f) * shelf;
    }
    // Dique que contiene el agua en las laderas: queda sobre el nivel hasta u = 1.3 y se hunde después, así que el lago nunca se vacía hacia abajo.
    static float basinDam(float u, float level) {
        return level + 0.05f + 0.25f * (1.0f - smooth(1.0f, 1.5f, u)) - 8.0f * smooth(1.3f, 2.4f, u);
    }

    // Aplana la zona, da forma a la cuenca y fija el nivel del agua (que queda siempre por debajo del borde).
    void carveBasin(float cx, float cz, float radius, float depth, float level, Water kind, float h0, float irregularity = 0.22f, float frequency = 0.07f) {
        depth = (std::min)(depth, 0.32f * radius + 1.0f);
        const float reach = radius * 2.6f;
        const int c0 = nearestCol(cx - reach), c1 = nearestCol(cx + reach), r0 = nearestCol(cz - reach), r1 = nearestCol(cz + reach);
        const float seed = between(0.0f, 100.0f);
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const float x = m_ground.nodeCoord(col), z = m_ground.nodeCoord(row);
                const float wobble = 1.0f + irregularity * m_noise.fbm((x - cx) * frequency + seed, (z - cz) * frequency - seed, 3); // orilla irregular
                const float u = std::sqrt((x - cx) * (x - cx) + (z - cz) * (z - cz)) / wobble / radius;
                if (u > 2.6f || m_coast.at(col, row) < 0.0f) continue; // el mar no se rellena: nada de tierra sin isla
                float& g = m_ground.at(col, row);
                const float flat = g + (h0 - g) * (1.0f - smooth(1.0f, 2.4f, u));
                if (u < 1.0f) {
                    g = basinFloor(u, level, depth);
                } else {
                    const float bank = level + 0.25f + (h0 - level) * smooth(1.0f, 1.5f, u);
                    g = (std::max)(bank + (flat - bank) * smooth(1.0f, 1.8f, u), basinDam(u, level));
                    if (u > 1.8f) g = (std::max)(flat, basinDam(u, level));
                }
                if (u < 1.02f && g < level) {
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

    // ¿Hay un volcán o un glaciar en este punto (más un margen)? Los lagos y los ríos de montaña no nacen ahí.
    bool onFeature(float x, float z, float margin) const {
        for (const Zone& zone : m_zones) {
            if (!zone.feature) continue;
            const float dx = x - zone.x, dz = z - zone.z, reach = zone.radius * 1.25f + margin;
            if (dx * dx + dz * dz < reach * reach) return true;
        }
        return false;
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
                    if (onFeature(cx, cz, radius * 1.5f)) continue;
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
    static bool riverKind(uint8_t k) { return k == RIVER || k == UPPER_RIVER; }
    static bool lakeKind(uint8_t k) { return k == LAKE || k == UPPER_LAKE || k == POND; }

    struct Trace { std::vector<PathPoint> path; bool sea = false, joined = false; float joinLevel = 0.0f; };

    // Camino de un río: baja por la máxima pendiente (con meandros) hasta el mar o hasta un río mayor al que se une (un afluente). Si se atasca
    // en una hondonada, se encamina hacia la costa (el cauce la atraviesa excavando) y, si aun así no llega, sigue recto hacia el mar: ningún
    // río se queda a medias.
    Trace tracePath(float x, float z, bool mayJoin) {
        Trace trace;
        std::vector<PathPoint>& path = trace.path;
        float dx = 0.0f, dz = 0.0f, lowest = 1.0e9f;
        int stalled = 0;
        const float seed = between(0.0f, 100.0f);
        for (int step = 0; step < 1400; ++step) {
            const float gx = (ground(x + 5.0f, z) - ground(x - 5.0f, z)) / 10.0f, gz = (ground(x, z + 5.0f) - ground(x, z - 5.0f)) / 10.0f;
            const float sx = (m_coast.sample(x + 5.0f, z) - m_coast.sample(x - 5.0f, z)) / 10.0f, sz = (m_coast.sample(x, z + 5.0f) - m_coast.sample(x, z - 5.0f)) / 10.0f;
            const float here = ground(x, z);
            if (here < lowest - 0.15f) { lowest = here; stalled = 0; } else ++stalled;
            const float toSea = stalled > 20 ? 1.0f : 0.10f; // atascado: manda la dirección al mar
            float tx = -gx * (stalled > 20 ? 0.3f : 1.0f) - toSea * sx, tz = -gz * (stalled > 20 ? 0.3f : 1.0f) - toSea * sz;
            float length = std::sqrt(tx * tx + tz * tz);
            if (length < 1.0e-5f) { tx = -sx; tz = -sz; length = std::sqrt(tx * tx + tz * tz); if (length < 1.0e-5f) break; }
            tx /= length; tz /= length;
            const float meander = 0.55f * m_noise.value(x * 0.025f + seed, z * 0.025f);
            dx = 0.6f * dx + 0.4f * (tx - tz * meander);
            dz = 0.6f * dz + 0.4f * (tz + tx * meander);
            length = std::sqrt(dx * dx + dz * dz);
            if (length < 1.0e-5f) break;
            dx /= length; dz /= length;
            path.push_back({ x, z, here, 0.0f, 0.0f, 0.0f, dx, dz, false, false });
            if (path.back().terrain < 0.25f && m_coast.sample(x, z) < 12.0f) { trace.sea = true; break; } // llegó al mar
            if (m_coast.sample(x, z) < -4.0f) { trace.sea = true; break; }
            if (mayJoin && step > 12) { // llegó a otro río (no a su cascada): se une a él
                const size_t node = nearestNode(x, z);
                if (riverKind(m_kind[node]) && m_level[node] > 0.8f) { trace.joined = true; trace.joinLevel = m_level[node]; break; }
            }
            x += dx * Physics::Heightfield::CELL; z += dz * Physics::Heightfield::CELL;
            if (std::fabs(x) > Physics::World::HALF_SIZE - 4.0f || std::fabs(z) > Physics::World::HALF_SIZE - 4.0f) break;
        }
        if (!trace.sea && !trace.joined && !path.empty()) { // sin salida natural: recto hacia el mar
            x = path.back().x; z = path.back().z;
            for (int k = 0; k < 600 && m_coast.sample(x, z) > -4.0f; ++k) {
                const float sx = (m_coast.sample(x + 5.0f, z) - m_coast.sample(x - 5.0f, z)) / 10.0f, sz = (m_coast.sample(x, z + 5.0f) - m_coast.sample(x, z - 5.0f)) / 10.0f;
                const float length = std::sqrt(sx * sx + sz * sz);
                if (length < 1.0e-5f) break;
                dx = -sx / length; dz = -sz / length;
                x += dx * Physics::Heightfield::CELL; z += dz * Physics::Heightfield::CELL;
                if (std::fabs(x) > Physics::World::HALF_SIZE - 4.0f || std::fabs(z) > Physics::World::HALF_SIZE - 4.0f) break;
                path.push_back({ x, z, ground(x, z), 0.0f, 0.0f, 0.0f, dx, dz, false, false });
            }
            trace.sea = m_coast.sample(x, z) <= -4.0f;
        }
        std::vector<float> smoothed(path.size());
        for (size_t i = 0; i < path.size(); ++i) { // alisa el terreno a lo largo del cauce
            float sum = 0.0f;
            int k = 0;
            for (int j = -2; j <= 2; ++j) { const int index = static_cast<int>(i) + j; if (index >= 0 && index < static_cast<int>(path.size())) { sum += path[index].terrain; ++k; } }
            smoothed[i] = sum / k;
        }
        for (size_t i = 0; i < path.size(); ++i) path[i].terrain = smoothed[i];
        return trace;
    }

    struct RiverStyle { float widthMin, widthMax, depthMin, depthMax; };

    static constexpr int FALL_WINDOW = 3;         // pasos del cauce con los que se mide si el terreno cae en picado
    static constexpr float FALL_MIN_DROP = 2.2f;  // desnivel (m) en esa ventana desde el que hay cascada
    static constexpr int FALL_STEPS = 3;          // pasos del cauce que dura la caída del agua

    // Lago quieto de montaña: la cuenca se hunde bajo 'level' y un reborde lo contiene (el cauce de un río lo atraviesa al excavarse después).
    void carveHighLake(float cx, float cz, float radius, float level, float depth) {
        depth = (std::min)(depth, 0.32f * radius + 1.0f);
        const float reach = radius * 1.8f;
        const int c0 = nearestCol(cx - reach), c1 = nearestCol(cx + reach), r0 = nearestCol(cz - reach), r1 = nearestCol(cz + reach);
        const float seed = between(0.0f, 100.0f);
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const float x = m_ground.nodeCoord(col), z = m_ground.nodeCoord(row);
                const float wobble = 1.0f + 0.15f * m_noise.fbm((x - cx) * 0.08f + seed, (z - cz) * 0.08f - seed, 3);
                const float u = std::sqrt((x - cx) * (x - cx) + (z - cz) * (z - cz)) / wobble / radius;
                if (m_coast.at(col, row) < 0.0f) continue;
                float& g = m_ground.at(col, row);
                const size_t index = static_cast<size_t>(row) * m_ground.count() + col;
                if (u < 1.0f) {
                    g = basinFloor(u, level, depth);
                    if (g < level) { m_level[index] = level; m_kind[index] = UPPER_LAKE; }
                } else if (u < 1.8f) { // reborde: el agua no se escapa por los lados y la ladera se funde con él
                    const float rim = level + 0.25f + 0.35f * (1.0f - smooth(1.0f, 1.6f, u));
                    g = (std::max)(g + (rim - g) * (1.0f - smooth(1.2f, 1.8f, u)), (std::min)(rim, level + 0.3f));
                }
            }
        }
        m_basins.push_back({ cx, cz, radius, level, UPPER_LAKE });
    }

    static constexpr float SPRING_RADIUS = 4.5f;  // charca de la que nace un río de montaña
    static constexpr int POOL_STEPS = 6;          // pasos de cauce que ocupa la poza al pie de una cascada
    static constexpr int MOUTH_STEPS = 7;         // pasos con los que el río se abre al mar
    static constexpr int MOUTH_EXTRA = 6;         // pasos que el cauce se adentra en el mar
    static constexpr int JOIN_STEPS = 6;          // pasos finales de un afluente: su nivel se iguala al del río al que se une
    static constexpr float BANK_FIX = 4.0f;       // ancho (m) de la orilla llana junto al agua: el suelo la rellena y la cubre siempre
    static constexpr float VALLEY = 18.0f;        // hasta dónde (m, desde la orilla) el cauce modela el valle
    static constexpr float VALLEY_SLOPE = 0.5f;   // pendiente de las paredes del valle
    static constexpr float GORGE_SLOPE = 1.0f;    // y de las de la garganta que la erosión abre en cascadas y pozas

    // Excava un río que baja hasta el mar: nace en una charca (manantial), el nivel del agua solo baja, el cauce abre un valle con su orilla y donde
    // el terreno cae en picado (o, con 'forceFall', en el tramo más empinado si no hay ninguno) el agua se despeña en cascada, con una garganta
    // erosionada y una poza al pie. Antes de la primera cascada el río se ensancha en un lago quieto. Hasta 'maxFalls' cascadas. Termina en el mar
    // o se une como afluente a otro río (que ya llega al mar); false si no puede (entonces no toca el terreno).
    bool carveRiver(float startX, float startZ, float startLevel, int maxFalls, bool forceFall, const RiverStyle& style, bool spring) {
        Trace trace = tracePath(startX, startZ, true);
        std::vector<PathPoint>& path = trace.path;
        int n = static_cast<int>(path.size());
        if (n < 16 || (!trace.sea && !trace.joined)) return false;
        if (!trace.joined) {
            for (int k = 1; k <= MOUTH_EXTRA; ++k) { // la desembocadura se adentra en el mar
                PathPoint p = path[n - 1];
                p.x += p.tx * Physics::Heightfield::CELL * k; p.z += p.tz * Physics::Heightfield::CELL * k;
                p.terrain = (std::min)(path[n - 1].terrain, 0.0f) - 0.5f * k;
                path.push_back(p);
            }
        }
        n = static_cast<int>(path.size());
        const int tail = trace.joined ? JOIN_STEPS : MOUTH_EXTRA + MOUTH_STEPS;
        struct Fall { int lip; int end; float forced; };
        std::vector<Fall> falls;
        for (int i = 3; i + FALL_WINDOW < n - tail && static_cast<int>(falls.size()) < maxFalls; ++i) {
            if (path[i].terrain < 2.5f || path[i].terrain - path[i + FALL_WINDOW].terrain < FALL_MIN_DROP) continue;
            int end = i + FALL_WINDOW;
            while (end + 1 < n - tail && end - i < 9 && path[end].terrain - path[end + 1].terrain > 0.8f) ++end;
            falls.push_back({ i, end, 0.0f });
            i = end + 14; // cascadas separadas
        }
        for (int wanted = forceFall ? 2 : 0; static_cast<int>(falls.size()) < wanted;) { // sin cortados naturales: se talla la caída en el tramo más empinado libre
            int best = -1;
            float bestDrop = 0.0f;
            for (int i = static_cast<int>(0.15f * n); i < static_cast<int>(0.75f * n) && i + 8 < n - tail; ++i) {
                bool free = true;
                for (const Fall& f : falls) if (i > f.lip - 16 && i < f.end + 16) free = false;
                const float d = path[i].terrain - path[i + 8].terrain;
                if (free && path[i].terrain >= 4.0f && d > bestDrop) { bestDrop = d; best = i; }
            }
            if (best < 0) break;
            falls.push_back({ best, best + FALL_STEPS, (std::clamp)(between(6.0f, 11.0f), 4.0f, (std::max)(4.0f, 0.95f * bestDrop)) });
        }
        std::sort(falls.begin(), falls.end(), [](const Fall& a, const Fall& b) { return a.lip < b.lip; });

        float level = startLevel > -1.0e8f ? startLevel : path.front().terrain - 0.35f;
        // Lago de montaña antes de la primera cascada: el nivel se mantiene plano a lo largo del lago.
        const bool lake = !falls.empty() && falls.front().lip >= 14;
        const float lakeRadius = between(10.0f, 15.0f);
        const int lakeSteps = static_cast<int>(std::ceil(lakeRadius / Physics::Heightfield::CELL));
        const int lakeFrom = lake ? (std::max)(2, falls.front().lip - 2 * lakeSteps) : -1;
        float lakeLevel = 0.0f;
        if (lake) {
            lakeLevel = 1.0e9f;
            for (int i = lakeFrom; i <= falls.front().lip; ++i) lakeLevel = (std::min)(lakeLevel, path[i].terrain - 0.35f);
            lakeLevel = (std::max)(lakeLevel, 1.0f);
        }
        size_t fall = 0;
        float lipLevel = level, fallTo = level;
        int poolEnd = -1;
        for (int i = 0; i < n; ++i) {
            PathPoint& p = path[i];
            const float target = p.terrain - 0.35f;
            if (lake && i >= lakeFrom && i <= falls.front().lip) {
                level = (std::min)(level, lakeLevel); // plano a lo largo del lago
            } else if (fall < falls.size() && i == falls[fall].lip) { // borde de la cascada: el agua sigue a este nivel y cae a partir de aquí
                level = (std::min)(level, target);
                lipLevel = level;
                const Fall& f = falls[fall];
                fallTo = f.forced > 0.0f ? level - f.forced : (std::min)(level - 3.5f, path[f.end].terrain - 0.35f);
                fallTo = (std::max)(fallTo, 0.0f);
            } else if (fall < falls.size() && i > falls[fall].lip && i <= falls[fall].lip + FALL_STEPS) {
                const float t = static_cast<float>(i - falls[fall].lip) / FALL_STEPS;
                level = (std::min)(level, lipLevel + (fallTo - lipLevel) * t);
                p.cascade = true;
                if (i == falls[fall].lip + FALL_STEPS) { poolEnd = i + POOL_STEPS; ++fall; }
            } else {
                level = (std::min)(level, target);
            }
            if (!trace.joined && i >= n - MOUTH_EXTRA - MOUTH_STEPS) { // el nivel baja hasta el del mar
                const float t = static_cast<float>(i - (n - MOUTH_EXTRA - MOUTH_STEPS)) / (MOUTH_EXTRA + MOUTH_STEPS - 1);
                level = (std::min)(level, level * (1.0f - t));
            }
            if (trace.joined && i >= n - JOIN_STEPS) level = (std::max)(level, trace.joinLevel); // desemboca a ras del río mayor
            level = (std::max)(level, 0.0f);
            const float t = static_cast<float>(i) / n;
            p.level = level;
            p.halfWidth = style.widthMin + (style.widthMax - style.widthMin) * (0.4f * t + 0.6f * t * t);
            p.bed = level - between(style.depthMin, style.depthMax);
            if (i > 0 && poolEnd >= 0 && i <= poolEnd && !p.cascade) { // poza al pie de la cascada: más ancha y honda
                p.halfWidth *= 1.9f;
                p.bed = level - style.depthMax * 1.7f;
                p.gorge = true;
            }
            if (p.cascade) p.gorge = true;
            if (!trace.joined && i >= n - MOUTH_EXTRA - MOUTH_STEPS) { // desembocadura abierta
                const float m = static_cast<float>(i - (n - MOUTH_EXTRA - MOUTH_STEPS)) / (MOUTH_EXTRA + MOUTH_STEPS - 1);
                p.halfWidth *= 1.0f + 1.4f * m;
                p.bed = (std::min)(p.bed, -1.2f - 1.5f * m);
            }
        }
        for (const Fall& f : falls) { // la garganta empieza un poco antes del borde
            for (int i = (std::max)(0, f.lip - 2); i < f.lip; ++i) path[i].gorge = true;
        }
        if (spring) carveHighLake(path.front().x, path.front().z, SPRING_RADIUS, path.front().level + 0.35f, 2.6f); // el manantial
        if (lake) {
            const PathPoint& c = path[falls.front().lip - lakeSteps];
            carveHighLake(c.x, c.z, lakeRadius, lakeLevel, between(4.0f, 6.0f));
        }
        // reparte el cauce por los nodos cercanos: cada nodo toma el punto más próximo
        const int c = m_ground.count();
        std::vector<int> touched;
        std::vector<float> best(static_cast<size_t>(c) * c, 1.0e9f);
        std::vector<int> owner(static_cast<size_t>(c) * c, -1);
        for (int i = 0; i < n; ++i) {
            const PathPoint& p = path[i];
            const float reach = p.halfWidth + VALLEY * (p.gorge ? 0.7f : 1.0f) + BANK_FIX;
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
        const int firstLip = falls.empty() ? -1 : falls.front().lip;
        std::vector<int> banks; // nodos de la orilla y el valle, que se suavizan al final
        std::vector<float> bankFloor;
        for (const int index : touched) {
            const int i = owner[index];
            const PathPoint& p = path[i];
            const float d = best[index], hw = p.halfWidth;
            const uint8_t before = m_kind[index];
            if (lakeKind(before) || before == LAVA || riverKind(before) || before == CASCADE) continue; // los lagos, la lava y los ríos ya excavados no se tocan
            const bool fill = p.level > 0.5f && m_coast.data()[index] > 0.0f; // en la desembocadura el cauce solo corta (no levanta una barrera en el mar)
            const float slope = p.gorge ? GORGE_SLOPE : VALLEY_SLOPE;
            const float top = p.level + 0.15f + BANK_FIX * 0.12f;
            float bank;
            if (d < hw) bank = p.bed + (p.level - p.bed) * (d / hw) * (d / hw);          // el cauce
            else if (d < hw + BANK_FIX) bank = p.level + 0.15f + (d - hw) * 0.12f;      // orilla llana: el suelo rodea siempre el agua
            else bank = top + (d - hw - BANK_FIX) * slope;                               // paredes del valle
            float& g = m_ground.data()[index];
            if (d < hw + BANK_FIX) { if (fill || bank < g) g = bank; }
            else if (bank < g) g = bank;
            if (d < hw + 0.1f && g < p.level) {
                const bool upper = firstLip >= 0 && i <= firstLip;
                m_level[index] = p.level;
                m_kind[index] = p.cascade ? CASCADE : upper ? UPPER_RIVER : RIVER;
                const float speed = p.cascade ? 5.0f : upper ? 1.6f : 1.1f;
                m_flowX.data()[index] = p.tx * speed;
                m_flowZ.data()[index] = p.tz * speed;
            } else if (d >= hw + 0.1f && d < hw + BANK_FIX + 10.0f) {
                banks.push_back(index);
                bankFloor.push_back(fill ? p.level + 0.08f : -1.0e9f);
            }
        }
        for (int pass = 0; pass < 2; ++pass) { // suaviza la orilla y el valle sin hundirlos bajo el agua
            std::vector<float> next(banks.size());
            for (size_t k = 0; k < banks.size(); ++k) {
                const int index = banks[k], col = index % c, row = index / c;
                float sum = 0.0f;
                int count = 0;
                for (const auto& o : { std::pair<int, int>{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }) {
                    const int cc = col + o.first, rr = row + o.second;
                    if (cc < 0 || rr < 0 || cc >= c || rr >= c) continue;
                    sum += m_ground.at(cc, rr);
                    ++count;
                }
                const float g = m_ground.data()[index];
                next[k] = (std::max)(count > 0 ? 0.5f * g + 0.5f * sum / count : g, bankFloor[k]);
            }
            for (size_t k = 0; k < banks.size(); ++k) m_ground.data()[banks[k]] = next[k];
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

    // Ríos de montaña: nacen en las cumbres (o en un lago alto), bajan abriendo valles, saltan en cascadas y acaban en el mar (o se unen a otro río
    // que llega a él). Ríos de llanura: salen de los lagos y bajan al mar. Los lagos no tienen corriente.
    void carveRivers(const GameData& data) {
        const Habitat* river = nullptr;
        const Habitat* upper = nullptr;
        for (const Habitat& habitat : data.habitats) {
            if (habitat.placement == HabitatPlacement::RIVER) river = &habitat;
            if (habitat.placement == HabitatPlacement::UPPER_RIVER) upper = &habitat;
        }
        if (!river && !upper) return;
        const RiverStyle style{ river ? river->minRadius : 2.4f, river ? river->maxRadius : 3.4f, river ? -river->maxAltitude : 2.2f, river ? -river->minAltitude : 3.2f };
        const int mountain = upper ? (std::max)(2, betweenInt((std::max)(1, upper->minZones), (std::max)(1, upper->maxZones))) : 0;
        struct Source { float x, z, level; };
        std::vector<Source> sources;
        for (const Basin& basin : m_basins) {
            float x = 0.0f, z = 0.0f;
            if (basin.kind == UPPER_LAKE && outlet(basin, x, z, 20.0f)) sources.push_back({ x, z, basin.level });
        }
        // cumbres: los puntos más altos de la isla principal (sin entrar en la nieve perpetua), separados entre sí
        struct Peak { float x, z, h; };
        std::vector<Peak> peaks;
        const Island& island = m_islands.front();
        for (int k = 0; k < 1200; ++k) {
            const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f)) * island.r * 0.85f;
            const float px = island.x + std::cos(a) * r, pz = island.z + std::sin(a) * r, t = ground(px, pz);
            if (t > 9.0f && t < SNOW_LINE + 6.0f && coastSigned(px, pz) > 40.0f && std::sqrt(px * px + pz * pz) > PLATEAU_FADE + 10.0f && m_kind[nearestNode(px, pz)] == DRY && !onFeature(px, pz, 0.0f)) peaks.push_back({ px, pz, t });
        }
        std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& b) { return a.h > b.h; });
        for (const Peak& peak : peaks) {
            bool apart = true;
            for (const Source& other : sources) if ((peak.x - other.x) * (peak.x - other.x) + (peak.z - other.z) * (peak.z - other.z) < 70.0f * 70.0f) apart = false;
            if (apart) sources.push_back({ peak.x, peak.z, -1.0e9f });
        }
        int made = 0;
        for (size_t i = 0; i < sources.size() && made < mountain; ++i) {
            if (carveRiver(sources[i].x, sources[i].z, sources[i].level, 3, true, style, sources[i].level < -1.0e8f)) ++made;
        }
        const int plain = river ? betweenInt((std::max)(1, river->minZones), (std::max)(1, river->maxZones)) : 0;
        made = 0;
        for (const Basin& lake : m_basins) {
            if (made >= plain) break;
            float x = 0.0f, z = 0.0f;
            if (lake.kind != LAKE || !outlet(lake, x, z, 10.0f)) continue;
            if (carveRiver(x, z, lake.level, 1, false, style, false)) ++made;
        }
    }


    // ---- Accidentes: volcanes, lava, géiseres y fondo marino ----
    // Un volcán es una zona de tierra con 'peak': un cono de laderas de roca negra con un cráter lleno de lava (agua de tipo LAVA) y, de vez en
    // cuando, ríos de lava que bajan por la ladera. Los géiseres son parcelas pequeñas de un hábitat anfitrión con una charca hirviente y un chorro
    // que erupciona a intervalos. El fondo marino tiene arrecifes de coral, campos de algas y volcanes submarinos (con fumarolas en el cráter):
    // son 'capas' que se superponen al mar y a la tierra con un peso que baja hacia el borde.
    static constexpr float CRATER = 0.2f;         // radio del cráter (fracción del radio del volcán)
    static constexpr float CRATER_DEPTH = 0.3f;   // hondura del cráter (fracción de la altura del cono)
    static constexpr float LAVA_FILL = 0.45f;     // fracción de la hondura del cráter que llena la lava
    static constexpr float VENT_SIZE = 0.28f;     // tamaño de las fumarolas respecto al volcán submarino
    static constexpr int LAVA_STEPS = 60;         // pasos máximos de un río de lava
    static constexpr float GEYSER_PAD = 1.8f;     // la parcela del géiser se aplana hasta este múltiplo de su radio

    struct Overlay {
        Zone zone;
        HabitatPlacement kind = HabitatPlacement::GEYSER;
        float reach2 = 0.0f; // cuadrado del radio fuera del cual no pesa
        bool sea() const { return kind == HabitatPlacement::SEA_VOLCANO || kind == HabitatPlacement::REEF || kind == HabitatPlacement::KELP || kind == HabitatPlacement::VENT; }
    };

    static float coneProfile(float u) { return std::pow((std::max)(1.0f - u, 0.0f), 1.6f); }
    // Perfil del volcán: cono con cráter (1 = la altura de 'peak').
    static float volcanoProfile(float u) {
        if (u >= CRATER) return coneProfile(u);
        const float k = u / CRATER;
        return coneProfile(CRATER) - CRATER_DEPTH * (1.0f - k * k);
    }

    void addOverlay(const Zone& zone, HabitatPlacement kind) {
        Overlay overlay;
        overlay.zone = zone;
        overlay.kind = kind;
        const float reach = zone.radius * (std::max)(zone.stretch, 1.0f / zone.stretch) * 1.4f;
        overlay.reach2 = reach * reach;
        m_overlays.push_back(overlay);
    }

    // Peso (0..1) de una capa en (x, z).
    float overlayWeight(const Overlay& overlay, float x, float z) const {
        const float dx = x - overlay.zone.x, dz = z - overlay.zone.z;
        if (dx * dx + dz * dz > overlay.reach2) return 0.0f;
        return 1.0f - smooth(0.55f, 1.15f, distance2(overlay.zone, x, z));
    }

    // Pesos de las capas que se superponen a 'out' (sea: las del mar; si no, las de tierra).
    void applyOverlays(float x, float z, std::vector<float>& out, bool sea) const {
        for (const Overlay& overlay : m_overlays) {
            if (overlay.sea() != sea) continue;
            const float w = overlayWeight(overlay, x, z);
            if (w <= 0.0f) continue;
            for (float& value : out) value *= 1.0f - w;
            out[overlay.zone.habitat] += w;
        }
    }

    // Recorre los nodos de la malla alrededor de una zona (caja que la envuelve).
    template <typename F>
    void forNodesAround(const Zone& zone, float scale, F visit) {
        const float reach = zone.radius * (std::max)(zone.stretch, 1.0f / zone.stretch) * scale;
        const int c0 = nearestCol(zone.x - reach), c1 = nearestCol(zone.x + reach), r0 = nearestCol(zone.z - reach), r1 = nearestCol(zone.z + reach);
        for (int row = r0; row <= r1; ++row) for (int col = c0; col <= c1; ++col) visit(col, row, m_ground.nodeCoord(col), m_ground.nodeCoord(row));
    }

    // Levanta los volcanes de tierra y llena sus cráteres de lava.
    void raiseVolcanoes() {
        for (const Zone& zone : m_zones) {
            if (zone.peak <= 0.0f) continue;
            const float base = averageHeight(zone.x, zone.z, zone.radius * 0.5f);
            const float lava = base + zone.peak * (volcanoProfile(0.0f) + CRATER_DEPTH * LAVA_FILL);
            const float seed = between(0.0f, 100.0f);
            forNodesAround(zone, 1.2f, [&](int col, int row, float x, float z) {
                const float u = std::sqrt(distance2(zone, x, z));
                if (u >= 1.0f) return;
                const float rough = 1.0f + 0.12f * m_noise.fbm(x * 0.05f + seed, z * 0.05f - seed, 3); // barrancos y coladas en la ladera
                float& g = m_ground.at(col, row);
                g += zone.peak * volcanoProfile(u) * rough;
                if (u < CRATER && g < lava) {
                    const size_t index = static_cast<size_t>(row) * m_ground.count() + col;
                    m_level[index] = lava;
                    m_kind[index] = LAVA;
                    m_lavaCool.at(col, row) = 0.0f; // la lava del cráter está al rojo
                }
            });
        }
    }

    // Ríos de lava: bajan por la máxima pendiente desde el borde del cráter, más lentos que el agua. Unos son largos y otros cortos; al final
    // se estrechan, se hacen someros y pierden fuerza hasta enfriarse en roca volcánica (o donde topen con el agua).
    void carveLavaFlows() {
        for (const Zone& zone : m_zones) {
            if (zone.peak <= 0.0f) continue;
            const int flows = betweenInt(2, 4);
            for (int f = 0; f < flows; ++f) {
                const float angle = between(0.0f, 6.2831853f), ca = std::cos(angle), sa = std::sin(angle);
                float x = zone.x, z = zone.z, best = -1.0e9f;
                for (float r = 0.0f; r < zone.radius * 0.5f; r += 1.5f) { // el punto más alto del radio: el borde del cráter
                    const float h = ground(zone.x + ca * r, zone.z + sa * r);
                    if (h > best) { best = h; x = zone.x + ca * r; z = zone.z + sa * r; }
                }
                const int length = betweenInt(LAVA_MIN_STEPS, LAVA_STEPS); // río corto o largo
                float level = 1.0e9f, dx = ca, dz = sa;
                for (int step = 0; step < length; ++step) {
                    float gx, gz;
                    m_ground.gradient(x, z, gx, gz);
                    const float norm0 = std::sqrt(gx * gx + gz * gz);
                    if (norm0 > 1.0e-3f) { dx = 0.6f * dx - 0.4f * gx / norm0; dz = 0.6f * dz - 0.4f * gz / norm0; }
                    const float norm = std::sqrt(dx * dx + dz * dz);
                    dx /= norm; dz /= norm;
                    const float terrain = ground(x, z);
                    if (terrain < 1.6f || (step > 4 && m_kind[nearestNode(x, z)] != DRY && m_kind[nearestNode(x, z)] != LAVA)) break;
                    const float life = 1.0f - static_cast<float>(step) / length;              // 1 en el cráter, 0 en la punta
                    const float fade = smooth(0.0f, 0.45f, life);                                // el último tramo se apaga
                    level = (std::min)(level, terrain - 0.4f * fade);
                    const float half = (1.7f + 1.2f * (1.0f - life)) * (0.25f + 0.75f * fade);  // se ensancha al bajar y se afina al acabar
                    if (half < 0.7f) break;
                    carveLavaSection(x, z, dx, dz, half, level, fade);
                    x += dx * Physics::Heightfield::CELL; z += dz * Physics::Heightfield::CELL;
                }
            }
        }
    }

    // 'strength' (0..1): la hondura de la colada y su velocidad; cerca de la punta casi no hay lava y la corriente es mínima.
    void carveLavaSection(float x, float z, float tx, float tz, float half, float level, float strength) {
        const int c0 = nearestCol(x - half - 3.0f), c1 = nearestCol(x + half + 3.0f), r0 = nearestCol(z - half - 3.0f), r1 = nearestCol(z + half + 3.0f);
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const float nx = m_ground.nodeCoord(col), nz = m_ground.nodeCoord(row);
                const float d = std::sqrt((nx - x) * (nx - x) + (nz - z) * (nz - z));
                if (d > half + 3.0f) continue;
                float& g = m_ground.at(col, row);
                const size_t index = static_cast<size_t>(row) * m_ground.count() + col;
                const float bed = level - (0.25f + 0.65f * strength);
                const float wall = d < half ? bed + (level + 0.3f - bed) * (d / half) * (d / half) : level + 0.3f + (d - half) * 0.6f;
                g = (std::min)(g, wall);
                if (d < half && g < level && (m_kind[index] == DRY || m_kind[index] == LAVA)) {
                    if (m_kind[index] != LAVA || level < m_level[index]) m_level[index] = level;
                    m_kind[index] = LAVA;
                    const float speed = LAVA_SPEED * (0.25f + 0.75f * strength);
                    m_flowX.at(col, row) = tx * speed;
                    m_flowZ.at(col, row) = tz * speed;
                    m_lavaCool.at(col, row) = (std::min)(m_lavaCool.at(col, row), 1.0f - strength);
                }
            }
        }
    }
    static constexpr float LAVA_SPEED = 0.55f; // m/s de la lava que baja por la ladera (el agua corre bastante más rápido)
    static constexpr int LAVA_MIN_STEPS = 14;  // pasos del río de lava más corto

    // Géiseres: parcelas dentro de las zonas del hábitat anfitrión (besideId) con varias charcas hirvientes de distinto tamaño y forma irregular, cada
    // una con su chorro intermitente, y alrededor las aguas termales (hábitat HOT_SPRING con ese mismo anfitrión).
    void placeGeysers(const GameData& data) {
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& habitat = data.habitats[h];
            if (habitat.placement != HabitatPlacement::GEYSER) continue;
            int spring = -1;
            for (int k = 0; k < m_habitats; ++k) if (data.habitats[k].placement == HabitatPlacement::HOT_SPRING && data.habitats[k].besideId == habitat.id) spring = k;
            std::vector<const Zone*> hosts;
            for (const Zone& zone : m_zones) if (data.habitats[zone.habitat].id == habitat.besideId || (habitat.besideId < 0 && !zone.feature)) hosts.push_back(&zone);
            if (hosts.empty()) continue;
            const int count = betweenInt(habitat.minZones, habitat.maxZones);
            for (int i = 0; i < count; ++i) {
                const float radius = between(habitat.minRadius, habitat.maxRadius);
                for (int attempt = 0; attempt < 200; ++attempt) {
                    const Zone& host = *hosts[std::uniform_int_distribution<size_t>(0, hosts.size() - 1)(*m_random)];
                    const float a = between(0.0f, 6.2831853f);
                    const float r = host.peak > 0.0f ? between(0.9f, 1.4f) * host.radius : std::sqrt(between(0.0f, 1.0f)) * host.radius * 0.75f; // junto a un volcán: en la llanura que lo rodea
                    const float x = host.x + std::cos(a) * r, z = host.z + std::sin(a) * r;
                    if (coastSigned(x, z) < 25.0f || x * x + z * z < PLATEAU_FADE * PLATEAU_FADE) continue;
                    if (!dryAround(x, z, radius * GEYSER_PAD) || m_ground.slope(x, z) > 0.45f) continue;
                    bool apart = true;
                    for (const Overlay& other : m_overlays) {
                        const float dx = x - other.zone.x, dz = z - other.zone.z, need = radius + other.zone.radius + 14.0f;
                        if (dx * dx + dz * dz < need * need) apart = false;
                    }
                    if (!apart) continue;
                    const float h0 = averageHeight(x, z, radius);
                    if (coastSigned(x, z) < radius * GEYSER_PAD * 1.8f) continue;
                    flatten(x, z, radius * GEYSER_PAD, h0);
                    Zone zone;
                    zone.habitat = h; zone.x = x; zone.z = z; zone.radius = radius; zone.altitude = h0;
                    for (float& phase : zone.lobe) phase = between(0.0f, 6.2831853f);
                    addOverlay(zone, HabitatPlacement::GEYSER);
                    struct Spot { float x, z, r; };
                    std::vector<Spot> spots;
                    const auto free = [&](float px, float pz, float pr) {
                        for (const Spot& s : spots) { const float d = std::sqrt((px - s.x) * (px - s.x) + (pz - s.z) * (pz - s.z)); if (d < pr + s.r + 1.5f) return false; }
                        return true;
                    };
                    const int basins = betweenInt(GEYSER_MIN_BASINS, GEYSER_MAX_BASINS);
                    for (int b = 0; b < basins; ++b) {
                        for (int t = 0; t < 30; ++t) {
                            const float ba = between(0.0f, 6.2831853f), br = std::sqrt(between(0.0f, 1.0f)) * radius * 0.55f, pr = between(GEYSER_MIN_BASIN, GEYSER_MAX_BASIN);
                            const float bx = x + std::cos(ba) * br, bz = z + std::sin(ba) * br;
                            if (!free(bx, bz, pr)) continue;
                            spots.push_back({ bx, bz, pr });
                            carveBasin(bx, bz, pr, 1.6f, h0 - 0.35f, POND, h0, 0.4f, 0.28f);
                            Physics::World::Geyser geyser;
                            const float size = (pr - GEYSER_MIN_BASIN) / (GEYSER_MAX_BASIN - GEYSER_MIN_BASIN); // las charcas grandes lanzan chorros más altos
                            geyser.x = bx; geyser.z = bz; geyser.radius = (std::max)(1.2f, pr * 0.55f);
                            geyser.height = between(habitat.minAltitude, habitat.maxAltitude) * (0.55f + 0.45f * size);
                            geyser.period = between(habitat.minWeatherWait, habitat.maxWeatherWait); // segundos entre erupciones
                            geyser.phase = between(0.0f, geyser.period);
                            geyser.burst = between(3.5f, 6.0f);
                            m_geysers.push_back(geyser);
                            break;
                        }
                    }
                    if (spring >= 0) {
                        const Habitat& hot = data.habitats[spring];
                        const int pools = betweenInt(hot.minZones, hot.maxZones);
                        for (int p = 0; p < pools; ++p) {
                            for (int t = 0; t < 40; ++t) {
                                const float pa = between(0.0f, 6.2831853f), pd = between(0.55f, 0.95f) * radius, pr = between(hot.minRadius, hot.maxRadius);
                                const float px = x + std::cos(pa) * pd, pz = z + std::sin(pa) * pd;
                                if (!free(px, pz, pr)) continue;
                                spots.push_back({ px, pz, pr });
                                carveBasin(px, pz, pr, 1.3f, h0 - 0.25f, HOT, h0, 0.34f, 0.24f);
                                m_springs.push_back({ px, pz, pr * 1.5f });
                                Zone pool;
                                pool.habitat = spring; pool.x = px; pool.z = pz; pool.radius = pr * 1.8f; pool.altitude = h0;
                                for (float& phase : pool.lobe) phase = between(0.0f, 6.2831853f);
                                addOverlay(pool, HabitatPlacement::HOT_SPRING);
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    static constexpr int GEYSER_MIN_BASINS = 3, GEYSER_MAX_BASINS = 6; // charcas con géiser por parcela
    static constexpr float GEYSER_MIN_BASIN = 1.8f, GEYSER_MAX_BASIN = 4.4f; // radio de cada una (m)

    // Lleva el terreno de un disco a la altura 'target' (con un talud suave hacia fuera).
    void flatten(float cx, float cz, float radius, float target) {
        const float reach = radius * 1.8f;
        const int c0 = nearestCol(cx - reach), c1 = nearestCol(cx + reach), r0 = nearestCol(cz - reach), r1 = nearestCol(cz + reach);
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                const float x = m_ground.nodeCoord(col), z = m_ground.nodeCoord(row);
                const float u = std::sqrt((x - cx) * (x - cx) + (z - cz) * (z - cz)) / radius;
                if (u >= 1.8f) continue;
                float& g = m_ground.at(col, row);
                g += (target - g) * (1.0f - smooth(1.0f, 1.8f, u));
            }
        }
    }

    // Arrecifes, algas y volcanes submarinos: capas bajo el agua, a distintas distancias de la costa.
    void placeSeabed(const GameData& data) {
        for (int h = 0; h < m_habitats; ++h) {
            const Habitat& habitat = data.habitats[h];
            if (!habitat.seabed()) continue;
            const bool volcano = habitat.placement == HabitatPlacement::SEA_VOLCANO, reef = habitat.placement == HabitatPlacement::REEF;
            const float nearest = reef ? 10.0f : volcano ? 110.0f : 30.0f, farthest = reef ? 80.0f : volcano ? 330.0f : 190.0f;
            const int count = betweenInt(habitat.minZones, habitat.maxZones);
            for (int i = 0; i < count; ++i) {
                Zone zone;
                zone.habitat = h;
                zone.radius = between(habitat.minRadius, habitat.maxRadius);
                zone.stretch = between(habitat.minStretch, (std::max)(habitat.minStretch, habitat.maxStretch));
                zone.altitude = between(habitat.minAltitude, habitat.maxAltitude);
                zone.roughness = habitat.roughness;
                const float angle = between(0.0f, 6.2831853f);
                zone.cosA = std::cos(angle); zone.sinA = std::sin(angle);
                for (float& phase : zone.lobe) phase = between(0.0f, 6.2831853f);
                bool placed = false;
                for (int attempt = 0; attempt < 400 && !placed; ++attempt) {
                    const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f)) * (m_worldRadius - zone.radius * 1.3f - 20.0f);
                    const float x = std::cos(a) * r, z = std::sin(a) * r, s = -coastSigned(x, z);
                    if (s < nearest + zone.radius * 0.6f || s > farthest + zone.radius) continue;
                    bool apart = true;
                    for (const Overlay& other : m_overlays) {
                        const float dx = x - other.zone.x, dz = z - other.zone.z, need = (zone.radius + other.zone.radius) * 0.9f;
                        if (dx * dx + dz * dz < need * need) apart = false;
                    }
                    if (!apart) continue;
                    zone.x = x; zone.z = z;
                    placed = true;
                }
                if (!placed) continue;
                addOverlay(zone, habitat.placement);
                if (volcano && m_ventHabitat >= 0) { // fumarolas en el cráter
                    Zone vent = zone;
                    vent.habitat = m_ventHabitat;
                    vent.radius = zone.radius * VENT_SIZE;
                    vent.stretch = 1.0f; vent.cosA = 1.0f; vent.sinA = 0.0f;
                    addOverlay(vent, HabitatPlacement::VENT);
                }
            }
        }
    }

    // Da forma al fondo del mar bajo las capas: el arrecife y el campo de algas suben o bajan el fondo hasta su altura; el volcán submarino es un
    // cono con cráter cuya cima queda a la profundidad indicada.
    void shapeSeabed() {
        for (const Overlay& overlay : m_overlays) {
            if (!overlay.sea() || overlay.kind == HabitatPlacement::VENT) continue;
            const Zone& zone = overlay.zone;
            const float seed = between(0.0f, 100.0f);
            forNodesAround(zone, 1.4f, [&](int col, int row, float x, float z) {
                if (m_coast.at(col, row) > -2.0f) return;
                const float u = std::sqrt(distance2(zone, x, z));
                if (u >= 1.4f) return;
                float& g = m_ground.at(col, row);
                const float bumps = zone.roughness * m_noise.fbm(x * 0.08f + seed, z * 0.08f - seed, 3);
                if (overlay.kind == HabitatPlacement::SEA_VOLCANO) {
                    if (u >= 1.0f) return;
                    const float summit = zone.altitude;
                    const float t = u >= CRATER ? coneProfile(u) / coneProfile(CRATER) : 1.0f - 0.5f * CRATER_DEPTH * (1.0f - (u / CRATER) * (u / CRATER)) / coneProfile(CRATER);
                    g = (std::max)(g, g + (summit - g) * (std::max)(t, 0.0f) * (1.0f - smooth(0.85f, 1.0f, u)) + bumps * 0.3f);
                } else {
                    const float w = 1.0f - smooth(0.55f, 1.4f, u);
                    g += (zone.altitude + bumps - g) * w;
                }
            });
        }
    }

    // Agarre del suelo de cada nodo según el hábitat que domina (el hielo resbala): solo se calcula donde hay un hábitat resbaladizo.
    void bakeGrip(const GameData& data) {
        m_grip.init(m_ground.half(), 1.0f);
        m_caveGrip.init(m_ground.half(), 1.0f);
        for (const Underground::System& system : m_under.systems()) { // galerías: el suelo de cada nodo es el de su sala
            const int c0 = nearestCol(system.minX), c1 = nearestCol(system.maxX), r0 = nearestCol(system.minZ), r1 = nearestCol(system.maxZ);
            for (int row = r0; row <= r1; ++row) {
                for (int col = c0; col <= c1; ++col) {
                    const int h = m_under.nodeDefined(col, row) ? m_under.habitatAtNode(col, row) : -1;
                    if (h >= 0 && h < m_habitats) m_caveGrip.at(col, row) = data.habitats[h].friction;
                }
            }
        }
        std::vector<float> w;
        std::vector<char> done(static_cast<size_t>(m_ground.count()) * m_ground.count(), 0);
        for (const Zone& zone : m_zones) {
            if (data.habitats[zone.habitat].friction >= 0.99f) continue;
            forNodesAround(zone, 1.3f, [&](int col, int row, float x, float z) {
                const size_t index = static_cast<size_t>(row) * m_ground.count() + col;
                if (done[index]) return;
                done[index] = 1;
                if (m_kind[index] != DRY) return;
                weights(x, z, w);
                float grip = 0.0f;
                for (size_t h = 0; h < w.size(); ++h) grip += w[h] * data.habitats[h].friction;
                m_grip.at(col, row) = grip;
            });
        }
    }

    // ---- Agua final: nivel, mar, distancias ----
    void finishWater() {
        const int n = m_ground.count();
        // Red de seguridad: ninguna capa (ríos, lagos, parcelas) deja tierra emergida lejos de una isla; el mar sigue siendo mar.
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                if (m_coast.at(col, row) >= -2.0f) continue;
                float& g = m_ground.at(col, row);
                if (g > -0.3f) g = -0.3f - 0.1f * smooth(0.0f, 30.0f, -m_coast.at(col, row));
            }
        }
        m_lava.init(m_ground.half(), 0.0f);
        m_hot.init(m_ground.half(), 0.0f);
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                const size_t index = static_cast<size_t>(row) * n + col;
                const float g = m_ground.at(col, row), s = m_coast.at(col, row);
                if (m_kind[index] != DRY && g < m_level[index]) {
                    m_surface.at(col, row) = m_level[index];
                    m_lava.at(col, row) = m_kind[index] == LAVA ? 1.0f : 0.0f;
                    m_hot.at(col, row) = m_kind[index] == HOT ? 1.0f : 0.0f;
                    if (m_kind[index] == LAKE || m_kind[index] == UPPER_LAKE || m_kind[index] == POND || m_kind[index] == HOT) m_flowX.at(col, row) = m_flowZ.at(col, row) = 0.0f; // agua quieta: solo la mueve el viento
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
        const int host = data.habitatIndex(info.besideId); // grutas volcánicas y de hielo: la boca se abre en las laderas del hábitat anfitrión
        std::vector<const Zone*> hostZones;
        if (host >= 0) for (const Zone& zone : m_zones) if (zone.habitat == host) hostZones.push_back(&zone);
        if (host >= 0 && hostZones.empty()) return;
        struct Candidate { float x, z, dirX, dirZ, score; };
        std::vector<Candidate> candidates;
        for (const float tier : { 1.0f, 0.6f, 0.25f }) { // si no hay sitio ideal, se relajan las condiciones
            candidates.clear();
            for (int i = 0; i < 900; ++i) {
                const float a = between(0.0f, 6.2831853f), r = std::sqrt(between(0.0f, 1.0f));
                float x, z;
                if (host >= 0) {
                    const Zone& zone = *hostZones[std::uniform_int_distribution<size_t>(0, hostZones.size() - 1)(*m_random)];
                    x = zone.x + std::cos(a) * r * zone.radius * 0.9f; z = zone.z + std::sin(a) * r * zone.radius * 0.9f;
                } else {
                    x = island.x + std::cos(a) * r * island.r * 0.95f; z = island.z + std::sin(a) * r * island.r * 0.95f;
                    if (onFeature(x, z, 6.0f)) continue; // las grutas corrientes no se abren en volcanes ni glaciares
                }
                const float h = ground(x, z);
                if (h < 5.0f || h > (host >= 0 ? 70.0f : 38.0f) || coastSigned(x, z) < 30.0f * tier || std::sqrt(x * x + z * z) < PLATEAU_FADE + 10.0f || !dryAround(x, z, 9.0f)) continue;
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
            for (size_t i = 0; i < candidates.size() && i < 24; ++i) {
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
    static constexpr float SKY_SHORE = 4.0f;      // ancho (m) de la orilla del suelo de nube al mar de nubes
    static constexpr float SKY_SEA_DEPTH = 3.2f;  // profundidad del mar de nubes bajo el suelo
    static constexpr float SKY_ABYSS = 4.0f;      // ancho (m) del talud blanco más allá del mar
    static constexpr float SKY_ABYSS_DROP = 55.0f;
    static constexpr float SKY_SEA_RATIO = 0.3f;  // ancho mínimo del mar de nubes respecto al radio del suelo: el mar es el hábitat mayor
    static constexpr float SKY_LAND_SHARE = 0.2f; // parte de la isla que ocupan los demás hábitats (el resto es nube)
    static constexpr float SKY_RIM = 1.5f;        // altura (m) del borde de nube que contiene el mar

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
                region.floorY = between(info.minAltitude, (std::max)(info.minAltitude, info.maxAltitude));
                for (float& phase : region.lobe) phase = between(0.0f, 6.2831853f);
                float widest = 1.0f;
                for (int k = 0; k < 90; ++k) widest = (std::max)(widest, skyShape(region, std::cos(k * 0.0698f), std::sin(k * 0.0698f)));
                bool placed = false;
                for (int attempt = 0; attempt < 1500 && !placed; ++attempt) {
                    if (attempt % 250 == 0) { // el mar de nubes es el mayor hábitat de la isla: su ancho crece con el tamaño del suelo; si no cabe, la isla se encoge
                        region.floorRadius = (std::max)(24.0f, region.floorRadius * (attempt == 0 ? 1.0f : 0.85f));
                        const float ring = sea >= 0 ? (std::max)(between(data.habitats[sea].minRadius, data.habitats[sea].maxRadius), region.floorRadius * SKY_SEA_RATIO) : 0.0f;
                        region.seaRadius = region.floorRadius + ring;
                        region.radius = region.seaRadius + SKY_ABYSS;
                        region.thickness = (std::clamp)(region.seaRadius * between(0.10f, 0.16f), 14.0f, 40.0f); // fina: la isla es mucho más ancha que alta
                    }
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
        m_hills.clear();
        for (int h = 0; h < m_habitats; ++h) {
            if (data.habitats[h].placement != HabitatPlacement::SKY_CAVE) continue;
            const int count = betweenInt(data.habitats[h].minZones, data.habitats[h].maxZones);
            for (int i = 0; i < count; ++i) buildCloudCave(data, h);
        }
        buildSkyLand(data);
        for (const Region& region : m_regions) if (region.kind == HabitatPlacement::SKY) placeUpdraft(region);
    }

    // Los hábitats de la superficie (los de colocación libre) también aparecen sobre el suelo de nube: cada isla recibe parcelas de unos
    // pocos hábitats distintos, así que no hay dos islas iguales.
    void buildSkyLand(const GameData& data) {
        std::vector<int> land;
        for (int h = 0; h < m_habitats; ++h) if (data.habitats[h].placement == HabitatPlacement::ANY) land.push_back(h);
        if (land.empty()) return;
        for (size_t r = 0; r < m_regions.size(); ++r) {
            const Region region = m_regions[r];
            if (region.kind != HabitatPlacement::SKY || region.floorRadius < 18.0f) continue;
            const int count = betweenInt(2, region.floorRadius > 30.0f ? 5 : 3);
            for (int i = 0; i < count; ++i) {
                for (int attempt = 0; attempt < 60; ++attempt) {
                    const int h = land[static_cast<size_t>(betweenInt(0, static_cast<int>(land.size()) - 1))];
                    const Habitat& info = data.habitats[h];
                    const float radius = (std::min)(between(info.minRadius, info.maxRadius) * 0.45f, region.floorRadius * std::sqrt(SKY_LAND_SHARE / (static_cast<float>(count) * 1.3f))); // todos juntos, no más del 20 % de la isla
                    if (radius < 6.0f) continue;
                    const float angle = between(0.0f, 6.2831853f), dist = between(0.0f, (std::max)(0.0f, region.floorRadius * 0.72f - radius * 0.5f));
                    const float shape = skyShape(region, std::cos(angle), std::sin(angle));
                    Zone zone;
                    zone.habitat = h;
                    zone.x = region.x + std::cos(angle) * dist * shape; zone.z = region.z + std::sin(angle) * dist * shape;
                    zone.radius = radius;
                    zone.stretch = between(info.minStretch, (std::max)(info.minStretch, info.maxStretch));
                    zone.altitude = (std::max)(1.2f, between(info.minAltitude, info.maxAltitude) * 0.45f);
                    zone.roughness = info.roughness * 0.5f;
                    const float tilt = between(0.0f, 6.2831853f);
                    zone.cosA = std::cos(tilt); zone.sinA = std::sin(tilt);
                    for (float& phase : zone.lobe) phase = between(0.0f, 6.2831853f);
                    bool apart = true;
                    for (const Zone& other : m_skyZones) {
                        const float dx = zone.x - other.x, dz = zone.z - other.z, need = (zone.radius * zone.stretch + other.radius * other.stretch) * 0.85f;
                        if (dx * dx + dz * dz < need * need) apart = false;
                    }
                    for (const Zone& other : m_hills) { // sin pisar las colinas de las cuevas de nube
                        const float dx = zone.x - other.x, dz = zone.z - other.z, need = (zone.radius * zone.stretch + other.radius) * 1.15f;
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
    }

    // Cueva de nube: una colina en el suelo de una isla grande con una boca en su ladera y galerías por dentro.
    void buildCloudCave(const GameData& data, int habitat) {
        const Habitat& info = data.habitats[habitat];
        Underground::Plan plan;
        plan.kind = Underground::Kind::CAVE;
        plan.habitat = habitat;
        plan.radius = between(info.minRadius, info.maxRadius);
        plan.minFill = 0.2f;
        plan.maxDrop = 4.0f; // bajo el suelo de nube no hay roca: las galerías casi no bajan
        std::vector<size_t> islands;
        for (size_t i = 0; i < m_regions.size(); ++i) if (m_regions[i].kind == HabitatPlacement::SKY && m_regions[i].floorRadius >= 24.0f) islands.push_back(i);
        if (islands.empty()) return;
        for (int attempt = 0; attempt < 30; ++attempt) {
            const Region region = m_regions[islands[static_cast<size_t>(betweenInt(0, static_cast<int>(islands.size()) - 1))]];
            Zone hill;
            hill.habitat = region.habitat;
            hill.radius = (std::min)(plan.radius * 1.1f, region.floorRadius * 0.5f);
            const float angle = between(0.0f, 6.2831853f), dist = between(0.0f, (std::max)(0.0f, region.floorRadius * 0.5f));
            hill.x = region.x + std::cos(angle) * dist; hill.z = region.z + std::sin(angle) * dist;
            hill.altitude = between(12.0f, 16.0f);
            hill.roughness = 0.8f;
            for (const Zone& other : m_hills) {
                const float dx = hill.x - other.x, dz = hill.z - other.z, need = (hill.radius + other.radius) * 1.1f;
                if (dx * dx + dz * dz < need * need) { hill.radius = 0.0f; break; }
            }
            if (hill.radius < 8.0f) continue;
            raiseIslet(hill, region.floorY);
            m_hills.push_back(hill);
            float bestX = hill.x, bestZ = hill.z, bestSlope = -1.0f;
            for (int k = 0; k < 48; ++k) { // la boca, donde la ladera es más empinada
                const float a = k * 0.1308997f, x = hill.x + std::cos(a) * hill.radius * 0.85f, z = hill.z + std::sin(a) * hill.radius * 0.85f;
                float gx, gz;
                m_ground.gradient(x, z, gx, gz);
                const float slope = std::sqrt(gx * gx + gz * gz);
                if (slope > bestSlope && ground(x, z) > region.floorY + 3.0f) { bestSlope = slope; bestX = x; bestZ = z; }
            }
            float gx, gz;
            m_ground.gradient(bestX, bestZ, gx, gz);
            const float length = (std::max)(std::sqrt(gx * gx + gz * gz), 1.0e-4f);
            Underground::System system;
            if (!m_under.carve(plan, { bestX, bestZ, gx / length, gz / length }, { m_ground, m_surface, m_kind }, m_noise, *m_random, system) || system.radius < 6.0f) continue;
            Zone site;
            site.habitat = habitat; site.x = system.x; site.z = system.z; site.radius = system.radius * 0.7f; site.deep = true;
            m_sites.push_back(site);
            return;
        }
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
                    const float rim = SKY_RIM * smooth(region.seaRadius - 5.0f, region.seaRadius - 1.0f, d) * (1.0f - smooth(region.seaRadius + 1.0f, region.seaRadius + 7.0f, d)); // borde de nube que contiene el mar
                    if (d > region.seaRadius) { // el abismo: la nube blanca se hunde
                        const float t = smooth(region.seaRadius, region.radius, d);
                        g = (std::max)(seaG - SKY_ABYSS_DROP * std::pow(t, 1.4f), level + 0.2f + rim - 0.6f * smooth(region.seaRadius, region.seaRadius + 7.0f, d));
                    } else if (rim > 0.0f) {
                        g = (std::max)(g, level + 0.2f + rim);
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
    std::vector<Zone> m_hills;       // colinas de las cuevas de nube (la tierra del cielo no las pisa)
    Underground m_under;
    std::vector<int> m_hallHabitats; // hábitats de salas (HALL) de cuevas y minas
    int m_deepWater = -1;            // hábitat del agua subterránea
    std::vector<Physics::World::Updraft> m_updrafts;
    std::vector<Island> m_islands;
    std::vector<Basin> m_basins;
    std::vector<SeaBand> m_bandHabitat;
    int m_waterHabitat[WATER_KINDS] = {};
    int m_peakHabitat = -1, m_riverBank = -1, m_lakeBank = -1, m_ventHabitat = -1;
    std::vector<Overlay> m_overlays;
    std::vector<Physics::World::Geyser> m_geysers;
    Physics::Heightfield m_grip, m_caveGrip, m_lava, m_hot, m_lavaCool;
    std::vector<Physics::World::Spring> m_springs;
    float m_bankWidth[2] = { 7.0f, 7.0f };
    float m_worldRadius = 200.0f;

    Physics::Heightfield m_ground, m_surface, m_sea, m_coast, m_flowX, m_flowZ, m_riverDist, m_lakeDist;
    std::vector<float> m_level;
    std::vector<uint8_t> m_kind;
};
