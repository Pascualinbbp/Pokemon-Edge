#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <queue>
#include <random>
#include <utility>
#include <vector>
#include <DirectXMath.h>
#include "worldNoise.hpp"
#include "../../physics/heightfield.hpp"

// El subsuelo: cuevas y minas que se extienden BAJO el terreno, en capas propias de la malla del mundo (suelo, techo y agua de cada nodo;
// NONE donde no hay galería). Una galería nace en una boca abierta en una ladera (o en cualquier sitio: el terreno se talla y se levanta
// lo justo) y desciende poco a poco: la altura del suelo depende solo de lo que se ha recorrido desde la boca, así que nunca hay saltos.
//  - Cueva: túneles que serpentean y se ramifican, salas abiertas (grutas) con lagos subterráneos, estalactitas, estalagmitas y cristales.
//  - Mina: una galería principal casi recta con galerías transversales, salas de arranque, vetas de mineral, cuadros de madera, lámparas,
//    raíles y vagonetas.
// Todo lo calcula carve() una vez al crear el mundo; la física (capa subterránea), el render y los hábitats solo consultan el resultado.
class Underground {
    public:
    static constexpr float NONE = -1.0e4f;       // sin galería en ese nodo
    static constexpr float DEFINED = -5.0e3f;    // por encima de esto el nodo existe
    static constexpr float CLEARANCE = 2.2f;     // altura mínima libre para poder estar dentro
    static constexpr float COVER = 1.4f;         // roca mínima entre el techo y la superficie
    static constexpr float DROP = 0.42f;         // metros que baja el suelo por cada metro recorrido desde la boca
    static constexpr float DOOR = 3.0f;          // tramo de la boca al aire libre, a ras de la ladera tallada
    static constexpr float DOOR_HEIGHT = 4.0f;   // altura de la boca
    static constexpr float MOUND = 18.0f;        // hasta esta distancia de la boca el terreno se eleva para cubrir la galería
    static constexpr float MAX_DROP = 16.0f;     // lo más que baja el suelo respecto a la boca
    static constexpr float TRENCH_LENGTH = 10.0f; // explanada tallada ante la boca
    static constexpr float TRENCH_HALF = 4.5f;
    static constexpr float MIN_RADIUS = 6.0f;    // alcance mínimo de un sistema
    static constexpr float MIN_FILL = 0.3f;      // fracción del tamaño pedido que debe alcanzar como mínimo
    static constexpr float GROW_START = 8.0f;    // a partir de esta distancia a la boca las galerías empiezan a ensancharse
    static constexpr float GROW_END = 48.0f;     // y desde esta alcanzan su tamaño máximo
    static constexpr float GROW_WIDTH = 0.9f;    // cuánto más anchas (fracción) son las galerías del fondo
    static constexpr float GROW_HEIGHT = 0.85f;  // y cuánto más altas
    static constexpr float MIN_SURFACE = 2.5f;   // el terreno de encima debe estar al menos a esta altura sobre el mar

    enum class Kind : uint8_t { CAVE, MINE };

    // Elemento decorativo de una galería. 'a' y 'b' significan según el tipo (ver abajo); w y h son medidas; color indexa Style.
    struct Prop {
        enum Type : uint8_t { STALACTITE, STALAGMITE, PILLAR, BOULDER, CRYSTAL, ORE, LAMP, POST, BEAM, SLEEPER, RAIL, CART };
        Type type = BOULDER;
        DirectX::XMFLOAT3 a = {}, b = {};
        float w = 0.0f, h = 0.0f;
        uint8_t color = 0;
        // STALACTITE: a = techo, w = radio, h = largo.  STALAGMITE: a = suelo, w = radio, h = alto.  PILLAR: a = suelo, b = techo, w = radio.
        // BOULDER: a = centro, w = tamaño.  CRYSTAL: a = base, w = ancho, h = alto.  ORE: a = centro, w = tamaño.
        // LAMP: a = posición, b = hacia dónde mira (normal de la pared).  POST: a = base, w = grosor, h = alto.  BEAM: de a a b, w = grosor.
        // SLEEPER: a = centro, b = dirección de la vía, w = largo.  RAIL: de a a b, w = ancho.  CART: a = suelo, b = dirección, w = tamaño.
    };
    // Colores de minerales y cristales (Prop::color): rgb y brillo propio.
    struct Style { float r, g, b, glow; };
    static const Style& style(int color) {
        static const Style styles[] = {
            { 0.80f, 0.42f, 0.22f, 0.10f }, // cobre
            { 0.07f, 0.07f, 0.08f, 0.00f }, // carbón
            { 0.62f, 0.50f, 0.46f, 0.00f }, // hierro
            { 0.45f, 0.85f, 1.00f, 0.75f }, // diamante / cristal azul
            { 1.00f, 0.82f, 0.25f, 0.55f }, // oro
            { 0.72f, 0.45f, 1.00f, 0.75f }, // cristal violeta
            { 0.40f, 1.00f, 0.60f, 0.70f }, // cristal verde
        };
        return styles[(std::clamp)(color, 0, 6)];
    }

    struct System {
        Kind kind = Kind::CAVE;
        int habitat = 0;
        float mouthX = 0.0f, mouthZ = 0.0f, dirX = 1.0f, dirZ = 0.0f, floorY = 0.0f; // boca, hacia dentro, altura del suelo en la boca
        float x = 0.0f, z = 0.0f, radius = 0.0f;                                     // centro y alcance de las galerías
        size_t propBegin = 0, propEnd = 0;
        float minX = 0.0f, maxX = 0.0f, minZ = 0.0f, maxZ = 0.0f;
    };

    // Qué se quiere generar: tipo, hábitat de las galerías, tamaño, hábitats posibles de las salas y del agua subterránea (-1 = ninguno).
    struct Plan { Kind kind = Kind::CAVE; int habitat = 0; float radius = 30.0f; std::vector<int> halls; int water = -1; float maxDrop = MAX_DROP; float minFill = MIN_FILL; };
    struct Mouth { float x = 0.0f, z = 0.0f, dirX = 1.0f, dirZ = 0.0f; };
    // El terreno del mundo: se talla y se eleva en la boca. 'notDry' != 0 donde hay agua (no se excava ahí).
    struct Terrain { Physics::Heightfield& height; Physics::Heightfield& surface; const std::vector<uint8_t>& notDry; };

    // --- Consultas ---
    bool empty() const { return m_systems.empty(); }
    const std::vector<System>& systems() const { return m_systems; }
    const std::vector<Prop>& props() const { return m_props; }
    const Physics::Heightfield& floorField() const { return m_floor; }
    const Physics::Heightfield& floor() const { return m_floor; }
    const Physics::Heightfield& ceilField() const { return m_ceil; }
    const Physics::Heightfield& waterField() const { return m_water; }
    bool nodeDefined(int col, int row) const { return m_floor.at(col, row) > DEFINED; }
    // Celda (col, row) con galería. Una celda existe si sus cuatro nodos existen.
    bool cellDefined(int col, int row) const {
        return col >= 0 && row >= 0 && col + 1 < m_floor.count() && row + 1 < m_floor.count() &&
               nodeDefined(col, row) && nodeDefined(col + 1, row) && nodeDefined(col, row + 1) && nodeDefined(col + 1, row + 1);
    }
    // Celda abierta al cielo: la boca y la explanada (aquí no se dibuja el terreno: el suelo de la galería hace de suelo).
    bool hole(int col, int row) const { return m_hole[static_cast<size_t>(row) * m_floor.count() + col] != 0; }
    int habitatAtNode(int col, int row) const { return m_habitat[static_cast<size_t>(row) * m_floor.count() + col]; }

    // ¿Está bajo tierra quien está en (x, z) con los pies a 'feet'? (en una galería y por debajo de su techo)
    bool under(float x, float z, float feet) const { return cell(x, z) && feet < m_ceil.sample(x, z) - 0.3f; }
    bool cell(float x, float z) const { return !m_floor.empty() && m_floor.covers(x, z, DEFINED); }
    float floorAt(float x, float z) const { return m_floor.sample(x, z); }
    float ceilAt(float x, float z) const { return m_ceil.sample(x, z); }
    float levelAt(float x, float z) const { return m_water.sample(x, z); }
    float depthAt(float x, float z) const { return (std::max)(0.0f, m_water.sample(x, z) - m_floor.sample(x, z)); }
    // Hábitat (índice) del nodo más cercano; -1 si no hay galería.
    int habitatAt(float x, float z) const {
        if (!cell(x, z)) return -1;
        return m_habitat[nearestIndex(x, z)];
    }
    // ¿Hay galería cubierta (no la boca al aire libre) en el punto?
    bool covered(float x, float z) const {
        if (!cell(x, z)) return false;
        const int col = (std::min)(static_cast<int>((x + m_floor.half()) / Physics::Heightfield::CELL), m_floor.count() - 2);
        const int row = (std::min)(static_cast<int>((z + m_floor.half()) / Physics::Heightfield::CELL), m_floor.count() - 2);
        return !hole(col, row);
    }
    // ¿Cabe un cuerpo de estas medidas en (x, y, z) (y = pies)? (cámara y proyectiles)
    bool clear(float x, float y, float z, float height = 0.0f) const {
        if (!cell(x, z)) return false;
        return y >= m_floor.sample(x, z) - 0.05f && y + height <= m_ceil.sample(x, z) + 0.05f;
    }

    // Distancia (m, hasta 'limit') desde (x, z) a la pared más cercana de la galería, mirando en 8 direcciones.
    float nearestWall(float x, float z, float limit) const {
        float best = limit;
        for (int k = 0; k < 8; ++k) best = (std::min)(best, wallDistance(x, z, std::cos(k * 0.785398f), std::sin(k * 0.785398f), best));
        return best;
    }

    // Une todo el subsuelo en un conjunto de mallas del tamaño del mundo (hay que llamarlo antes de carve).
    void init(const Physics::Heightfield& like) {
        m_systems.clear();
        m_props.clear();
        m_floor.init(like.half(), NONE);
        m_ceil.init(like.half(), NONE);
        m_water.init(like.half(), NONE);
        const size_t nodes = static_cast<size_t>(like.count()) * like.count();
        m_habitat.assign(nodes, -1);
        m_hole.assign(nodes, 0);
    }

    // Abre una cueva o una mina. false si no cabe (la boca no tiene salida o no hay suelo seco suficiente).
    bool carve(const Plan& plan, const Mouth& mouth, Terrain terrain, const WorldNoise& noise, std::mt19937& random, System& out);

    private:
    struct Cap { float ax, az, bx, bz, r; };
    struct Hall { float x, z, r, seed; int habitat; bool pool; float dist, level, poolR, depth; };
    using Lane = std::vector<DirectX::XMFLOAT2>;

    static float smooth(float edge0, float edge1, float x) {
        const float t = (std::clamp)((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    size_t nearestIndex(float x, float z) const {
        const int n = m_floor.count();
        const auto node = [&](float v) { return (std::clamp)(static_cast<int>(std::lround((v + m_floor.half()) / Physics::Heightfield::CELL)), 0, n - 1); };
        return static_cast<size_t>(node(z)) * n + node(x);
    }
    // Distancia horizontal que hay desde (x, z) hacia (dx, dz) hasta la pared (máx. 'limit').
    float wallDistance(float x, float z, float dx, float dz, float limit) const {
        for (float d = 0.25f; d <= limit; d += 0.25f) if (!cell(x + dx * d, z + dz * d)) return d - 0.25f;
        return limit;
    }

    Physics::Heightfield m_floor, m_ceil, m_water;
    std::vector<int16_t> m_habitat;
    std::vector<uint8_t> m_hole;
    std::vector<System> m_systems;
    std::vector<Prop> m_props;
};

inline bool Underground::carve(const Plan& plan, const Mouth& mouth, Terrain terrain, const WorldNoise& noise, std::mt19937& random, System& out) {
    using DirectX::XMFLOAT2;
    using DirectX::XMFLOAT3;
    constexpr float TAU = 6.2831853f;
    Physics::Heightfield& height = terrain.height;
    const int n = height.count();
    const float R = plan.radius;
    const bool cave = plan.kind == Kind::CAVE;
    const auto uni = [&](float low, float high) { return low + (high - low) * std::uniform_real_distribution<float>(0.0f, 1.0f)(random); };
    const auto chance = [&](float p) { return uni(0.0f, 1.0f) < p; };
    // La cueva crece hacia dentro: la boca es estrecha y las galerías se ensanchan y suben poco a poco con la distancia a ella.
    const auto progress = [&](float x, float z) { return smooth(GROW_START, GROW_END, std::hypot(x - mouth.x, z - mouth.z)); };
    const auto wrap = [](float a) { return std::remainder(a, 6.2831853f); };
    const auto nodeOf = [&](float v) { return (std::clamp)(static_cast<int>(std::lround((v + height.half()) / Physics::Heightfield::CELL)), 0, n - 1); };

    const float S0 = height.sample(mouth.x, mouth.z);
    const float heading0 = std::atan2(mouth.dirZ, mouth.dirX);

    // ---- 1. Plano: tubos (cápsulas), salas y líneas por las que irán los raíles ----
    std::vector<Cap> caps;
    std::vector<Hall> halls;
    std::vector<Lane> lanes;
    const float centerX = mouth.x + mouth.dirX * R * 0.65f, centerZ = mouth.z + mouth.dirZ * R * 0.65f;
    const auto addHall = [&](float x, float z, float r, float poolChance) {
        Hall hall{ x, z, r, uni(0.0f, 100.0f), plan.halls.empty() ? -1 : plan.halls[random() % plan.halls.size()], false, 0.0f, 0.0f, 0.0f, 0.0f };
        if (r >= 8.0f && chance(poolChance)) {
            hall.pool = true;
            hall.poolR = r * uni(0.42f, 0.55f);
            hall.depth = uni(2.0f, 3.2f);
        }
        halls.push_back(hall);
    };

    if (cave) {
        std::function<void(float, float, float, float, float, int)> walk = [&](float x, float z, float heading, float length, float r, int depth) {
            const float seed = uni(0.0f, 100.0f);
            float s = 0.0f;
            while (s < length) {
                constexpr float STEP = 4.0f;
                float turn = s < 8.0f && depth == 0 ? 0.0f : 0.5f * noise.value(seed + s * 0.07f, seed * 0.7f) + uni(-0.12f, 0.12f);
                heading += turn;
                if (std::hypot(x - centerX, z - centerZ) > R * 0.8f) heading += wrap(std::atan2(centerZ - z, centerX - x) - heading) * 0.3f;
                const float nx = x + std::cos(heading) * STEP, nz = z + std::sin(heading) * STEP;
                caps.push_back({ x, z, nx, nz, r * (1.0f + 0.18f * noise.value(seed + s * 0.11f, 3.0f)) * (1.0f + GROW_WIDTH * progress(x, z)) });
                x = nx; z = nz; s += STEP;
                if (depth < 2 && s > 8.0f && s < length - 8.0f && chance(0.16f)) {
                    walk(x, z, heading + (chance(0.5f) ? -1.0f : 1.0f) * uni(0.6f, 1.4f), length * uni(0.35f, 0.65f), r * 0.85f, depth + 1);
                }
                if (s > 12.0f && chance(0.06f)) addHall(x, z, uni(7.0f, (std::max)(8.0f, (std::min)(0.4f * R, 34.0f))), 0.6f);
            }
            if (depth == 0 || chance(0.6f)) addHall(x, z, uni(9.0f, (std::max)(10.0f, (std::min)(0.45f * R, 38.0f))), 0.65f);
        };
        walk(mouth.x + mouth.dirX * 2.0f, mouth.z + mouth.dirZ * 2.0f, heading0, R * 2.1f, uni(4.2f, 5.4f), 0);
    } else {
        // galería principal: tramos rectos con pequeñas desviaciones; transversales a ambos lados
        float x = mouth.x + mouth.dirX * 2.0f, z = mouth.z + mouth.dirZ * 2.0f, heading = heading0, s = 0.0f, nextCross = uni(14.0f, 20.0f);
        const float length = (std::max)(R * 1.9f, 26.0f);
        Lane main{ { x, z } };
        bool side = chance(0.5f);
        float segment = uni(22.0f, 34.0f);
        while (s < length) {
            constexpr float STEP = 4.0f;
            const float nx = x + std::cos(heading) * STEP, nz = z + std::sin(heading) * STEP;
            caps.push_back({ x, z, nx, nz, 4.3f * (1.0f + 0.6f * GROW_WIDTH * progress(x, z)) });
            x = nx; z = nz; s += STEP;
            main.push_back({ x, z });
            if (s >= nextCross && s < length - 10.0f) {
                nextCross += uni(18.0f, 26.0f);
                const float a = heading + (side ? 1.5708f : -1.5708f) + uni(-0.1f, 0.1f);
                side = !side;
                const float len = (std::max)(10.0f, R * uni(0.4f, 0.8f));
                float cx = x, cz = z;
                Lane lane{ { cx, cz } };
                for (float t = 0.0f; t < len; t += STEP) {
                    const float mx = cx + std::cos(a) * STEP, mz = cz + std::sin(a) * STEP;
                    caps.push_back({ cx, cz, mx, mz, 3.9f * (1.0f + 0.6f * GROW_WIDTH * progress(cx, cz)) });
                    cx = mx; cz = mz;
                    lane.push_back({ cx, cz });
                }
                if (chance(0.55f)) lanes.push_back(lane);
                if (chance(0.7f)) addHall(cx + std::cos(a) * 3.0f, cz + std::sin(a) * 3.0f, uni(6.0f, 11.0f), 0.2f);
            }
            if (s >= segment) { heading += uni(-0.3f, 0.3f); segment += uni(22.0f, 34.0f); }
        }
        lanes.insert(lanes.begin(), main);
        if (R >= 20.0f) addHall(x + std::cos(heading) * 4.0f, z + std::sin(heading) * 4.0f, uni(9.0f, 14.0f), 0.3f);
    }

    // ---- 2. Campo de apertura: dónde hay hueco ----
    float minX = mouth.x - TRENCH_LENGTH - 10.0f, maxX = mouth.x + TRENCH_LENGTH + 10.0f, minZ = minX - mouth.x + mouth.z, maxZ = maxX - mouth.x + mouth.z;
    const auto grow = [&](float x, float z, float r) { minX = (std::min)(minX, x - r); maxX = (std::max)(maxX, x + r); minZ = (std::min)(minZ, z - r); maxZ = (std::max)(maxZ, z + r); };
    for (const Cap& c : caps) { grow(c.ax, c.az, c.r + 5.0f); grow(c.bx, c.bz, c.r + 5.0f); }
    for (const Hall& h : halls) grow(h.x, h.z, h.r * 1.4f + 5.0f);
    const int c0 = (std::max)(nodeOf(minX), 2), c1 = (std::min)(nodeOf(maxX), n - 3), r0 = (std::max)(nodeOf(minZ), 2), r1 = (std::min)(nodeOf(maxZ), n - 3);
    const int W = c1 - c0 + 1, Hh = r1 - r0 + 1;
    if (W < 4 || Hh < 4) return false;
    const auto at = [&](int c, int r) { return static_cast<size_t>(r - r0) * W + (c - c0); };
    const auto inBox = [&](int c, int r) { return c >= c0 && c <= c1 && r >= r0 && r <= r1; };
    const float INF = 1.0e9f;
    std::vector<float> field(static_cast<size_t>(W) * Hh, -INF), original(field.size()), dist(field.size(), INF), wallHall(field.size(), 0.0f), mouthDist(field.size(), 0.0f);
    std::vector<uint8_t> ok(field.size(), 0), reach(field.size(), 0), inSet(field.size(), 0);
    std::vector<float> floorNode(field.size(), 0.0f), ceilNode(field.size(), 0.0f), levelNode(field.size(), 0.0f);
    std::vector<int16_t> habitatNode(field.size(), -1);

    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const float x = height.nodeCoord(c), z = height.nodeCoord(r);
            const size_t i = at(c, r);
            original[i] = height.at(c, r);
            float f = -INF;
            for (const Cap& cap : caps) {
                const float abx = cap.bx - cap.ax, abz = cap.bz - cap.az;
                const float t = (std::clamp)(((x - cap.ax) * abx + (z - cap.az) * abz) / (abx * abx + abz * abz + 1.0e-6f), 0.0f, 1.0f);
                f = (std::max)(f, cap.r - std::hypot(x - (cap.ax + abx * t), z - (cap.az + abz * t)));
            }
            for (const Hall& h : halls) {
                const float dx = x - h.x, dz = z - h.z, d = std::hypot(dx, dz);
                const float a = std::atan2(dz, dx);
                const float rEff = h.r * (1.0f + 0.22f * noise.value(h.seed + std::cos(a) * 1.3f, h.seed + std::sin(a) * 1.3f));
                f = (std::max)(f, rEff - d);
            }
            f += (cave ? 0.7f : 0.15f) * noise.fbm(x * 0.11f, z * 0.11f, 2);
            field[i] = f;
            mouthDist[i] = std::hypot(x - mouth.x, z - mouth.z);
            for (const Hall& h : halls) wallHall[i] = (std::max)(wallHall[i], 1.0f - smooth(0.55f * h.r, 1.05f * h.r, std::hypot(x - h.x, z - h.z)));
            bool fits = f > 0.0f && terrain.notDry[static_cast<size_t>(r) * n + c] == 0 && original[i] >= MIN_SURFACE;
            for (int dr = -1; dr <= 1 && fits; ++dr) for (int dc = -1; dc <= 1; ++dc) {
                if (m_floor.at(c + dc, r + dr) > DEFINED || terrain.notDry[static_cast<size_t>(r + dr) * n + c + dc] != 0) { fits = false; break; }
            }
            ok[i] = fits ? 1 : 0;
        }
    }

    // ---- 3. Conexión con la boca, suelo, techo y agua ----
    const auto cellOk = [&](int c, int r) { return inBox(c, r) && inBox(c + 1, r + 1) && ok[at(c, r)] && ok[at(c + 1, r)] && ok[at(c, r + 1)] && ok[at(c + 1, r + 1)]; };
    int startC = -1, startR = -1;
    float best = 1.0e9f;
    for (int r = r0; r < r1; ++r) for (int c = c0; c < c1; ++c) {
        if (!cellOk(c, r)) continue;
        const float d = std::hypot(height.nodeCoord(c) + 1.0f - (mouth.x + mouth.dirX * 1.5f), height.nodeCoord(r) + 1.0f - (mouth.z + mouth.dirZ * 1.5f));
        if (d < best) { best = d; startC = c; startR = r; }
    }
    if (startC < 0 || best > 6.0f) return false;

    const auto settle = [&]() {
        // Celdas alcanzables desde la boca (por aristas)
        std::fill(reach.begin(), reach.end(), 0);
        std::vector<std::pair<int, int>> stack{ { startC, startR } };
        reach[at(startC, startR)] = 1;
        while (!stack.empty()) {
            const auto [c, r] = stack.back();
            stack.pop_back();
            for (const auto& d : { std::pair<int, int>{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }) {
                const int cc = c + d.first, rr = r + d.second;
                if (cellOk(cc, rr) && !reach[at(cc, rr)]) { reach[at(cc, rr)] = 1; stack.push_back({ cc, rr }); }
            }
        }
        std::fill(inSet.begin(), inSet.end(), 0);
        for (int r = r0; r < r1; ++r) for (int c = c0; c < c1; ++c) {
            if (!reach[at(c, r)]) continue;
            inSet[at(c, r)] = inSet[at(c + 1, r)] = inSet[at(c, r + 1)] = inSet[at(c + 1, r + 1)] = 1;
        }
        const auto reachCell = [&](int c, int r) { return inBox(c, r) && inBox(c + 1, r + 1) && reach[at(c, r)]; };
        // Distancia recorrida desde la boca por los nodos (Dijkstra por las aristas de las celdas alcanzables)
        std::fill(dist.begin(), dist.end(), INF);
        using Entry = std::pair<float, std::pair<int, int>>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
        for (const auto& d : { std::pair<int, int>{ 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } }) {
            const int c = startC + d.first, r = startR + d.second;
            const float v = std::hypot(height.nodeCoord(c) - mouth.x, height.nodeCoord(r) - mouth.z);
            dist[at(c, r)] = v;
            queue.push({ v, { c, r } });
        }
        while (!queue.empty()) {
            const auto [v, node] = queue.top();
            queue.pop();
            const auto [c, r] = node;
            if (v > dist[at(c, r)]) continue;
            for (int dr = -1; dr <= 1; ++dr) for (int dc = -1; dc <= 1; ++dc) {
                if (!dc && !dr) continue;
                const int cc = c + dc, rr = r + dr;
                if (!inBox(cc, rr) || !inSet[at(cc, rr)]) continue;
                bool along = false; // la arista pertenece a alguna celda alcanzable
                if (dc && dr) along = reachCell((std::min)(c, cc), (std::min)(r, rr));
                else if (dc) along = reachCell((std::min)(c, cc), r) || reachCell((std::min)(c, cc), r - 1);
                else along = reachCell(c, (std::min)(r, rr)) || reachCell(c - 1, (std::min)(r, rr));
                if (!along) continue;
                const float openness = 0.5f * (wallHall[at(c, r)] + wallHall[at(cc, rr)]); // dentro de una sala casi no se baja: el suelo es llano
                const float step = ((dc && dr) ? 2.8284f : 2.0f) * (1.0f - 0.97f * openness), nv = v + step;
                if (nv < dist[at(cc, rr)]) { dist[at(cc, rr)] = nv; queue.push({ nv, { cc, rr } }); }
            }
        }
        for (Hall& h : halls) { // distancia en el centro de cada sala
            const int c = (std::clamp)(nodeOf(h.x), c0, c1), r = (std::clamp)(nodeOf(h.z), r0, r1);
            float d = INF;
            for (int dr = -3; dr <= 3; ++dr) for (int dc = -3; dc <= 3; ++dc) if (inBox(c + dc, r + dr)) d = (std::min)(d, dist[at(c + dc, r + dr)]);
            h.dist = d;
        }

        bool changed = false;
        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                const size_t i = at(c, r);
                if (!inSet[i] || dist[i] >= INF) { if (ok[i]) { ok[i] = 0; changed = true; } continue; }
                const float x = height.nodeCoord(c), z = height.nodeCoord(r), d = dist[i], away = mouthDist[i]; // d: recorrido; away: en línea recta desde la boca
                const float open = smooth(DOOR, DOOR + 6.0f, away);
                float floor = S0 - (std::min)(DROP * (std::max)(0.0f, d - DOOR), plan.maxDrop) + 0.35f * noise.fbm(x * 0.13f + 3.0f, z * 0.13f, 2) * open;
                float tall = cave ? 3.4f + 1.3f * (0.5f + 0.5f * noise.value(x * 0.09f, z * 0.09f)) : 3.0f + 0.15f * noise.value(x * 0.2f, z * 0.2f);
                float level = -INF, strength = 0.0f, bowlDrop = 0.0f;
                int hallHabitat = -1;
                for (const Hall& h : halls) {
                    if (h.dist >= INF) continue;
                    const float dc = std::hypot(x - h.x, z - h.z);
                    const float w = 1.0f - smooth(0.55f * h.r, 1.05f * h.r, dc);
                    if (w <= 0.0f) continue;
                    const float L = S0 - (std::min)(DROP * (std::max)(0.0f, h.dist - DOOR), plan.maxDrop); // nivel del suelo en el centro de la sala
                    const float roof = cave ? (std::min)(14.0f, 4.2f + 0.5f * h.r) * (0.9f + 0.1f * noise.value(x * 0.07f, z * 0.07f)) : 4.4f + 0.15f * h.r;
                    const float dome = w * (1.0f - 0.3f * smooth(0.3f * h.r, h.r, dc));
                    tall += (roof - tall) * dome;
                    if (w > strength) { strength = w; hallHabitat = h.habitat; }
                    if (h.pool && dc < h.poolR * 1.05f) {
                        const float bowl = (1.0f - smooth(0.0f, h.poolR, dc)) * w;
                        bowlDrop = (std::max)(bowlDrop, h.depth * bowl);
                        level = (std::max)(level, L - 0.4f);
                    }
                }
                const float flat = floor; // el techo no sigue la cubeta del lago
                floor -= bowlDrop;
                tall *= 1.0f + GROW_HEIGHT * progress(x, z);
                tall = (away < DOOR + 6.0f) ? DOOR_HEIGHT + (tall - DOOR_HEIGHT) * open : tall;
                if (away > MOUND) { // bajo la ladera: techo limitado por la roca que hay encima
                    const float cap = original[i] - COVER - flat;
                    if (cap < CLEARANCE) { ok[i] = 0; changed = true; continue; }
                    tall = (std::min)(tall, cap);
                }
                floorNode[i] = floor;
                ceilNode[i] = flat + tall;
                levelNode[i] = level > -INF * 0.5f ? (std::max)(floor, level) : floor;
                habitatNode[i] = static_cast<int16_t>(strength > 0.5f && hallHabitat >= 0 ? hallHabitat : plan.habitat);
                if (levelNode[i] - floor > 0.35f && plan.water >= 0) habitatNode[i] = static_cast<int16_t>(plan.water);
            }
        }
        return changed;
    };
    for (int pass = 0; pass < 8; ++pass) if (!settle()) break;
    // Tras la última poda hay que recalcular una vez más con el conjunto estable
    settle();
    bool any = false;
    for (size_t i = 0; i < ok.size() && !any; ++i) any = ok[i] && inSet[i];
    if (!any) return false;
    { // un sistema que se queda muy corto (la roca de encima no deja sitio) se descarta antes de tocar el terreno
        float lx = INF, hx = -INF, lz = INF, hz = -INF;
        for (int r = r0; r <= r1; ++r) for (int c = c0; c <= c1; ++c) {
            if (!ok[at(c, r)] || !inSet[at(c, r)]) continue;
            lx = (std::min)(lx, height.nodeCoord(c)); hx = (std::max)(hx, height.nodeCoord(c));
            lz = (std::min)(lz, height.nodeCoord(r)); hz = (std::max)(hz, height.nodeCoord(r));
        }
        if (0.5f * (std::max)(hx - lx, hz - lz) < (std::max)(MIN_RADIUS, plan.minFill * plan.radius)) return false;
    }

    // ---- 4. Terreno: boca tallada, explanada y montículo que cubre el principio de la galería ----
    std::vector<float> raised(field.size(), -INF);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const size_t i = at(c, r);
            if (!ok[i] || !inSet[i]) continue;
            raised[i] = mouthDist[i] < DOOR ? floorNode[i] : (std::max)(original[i], ceilNode[i] + COVER);
        }
    }
    for (int pass = 0; pass < 3; ++pass) { // la roca elevada se funde con la ladera (pendiente máxima 1.3)
        std::vector<float> next = raised;
        for (int r = r0; r <= r1; ++r) for (int c = c0; c <= c1; ++c) {
            const size_t i = at(c, r);
            if (ok[i] && inSet[i]) continue;
            for (int dr = -1; dr <= 1; ++dr) for (int dc = -1; dc <= 1; ++dc) {
                if (!inBox(c + dc, r + dr) || (!dc && !dr)) continue;
                const float v = raised[at(c + dc, r + dr)];
                if (v > -INF * 0.5f) next[i] = (std::max)(next[i], v - 1.3f * ((dc && dr) ? 2.83f : 2.0f));
            }
        }
        raised = next;
    }
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const size_t i = at(c, r);
            if (terrain.notDry[static_cast<size_t>(r) * n + c] != 0) continue;
            float g = original[i];
            const bool inside = ok[i] && inSet[i];
            if (raised[i] > -INF * 0.5f) g = inside ? raised[i] : (std::max)(g, raised[i]);
            if (!inside) { // explanada ante la boca
                const float x = height.nodeCoord(c) - mouth.x, z = height.nodeCoord(r) - mouth.z;
                const float u = x * mouth.dirX + z * mouth.dirZ, v = -x * mouth.dirZ + z * mouth.dirX;
                const float w = (1.0f - smooth(TRENCH_HALF, TRENCH_HALF + 4.5f, std::fabs(v))) * (1.0f - smooth(-TRENCH_LENGTH, -TRENCH_LENGTH - 7.0f, u)) * (1.0f - smooth(2.0f, 6.0f, u));
                g += (S0 - g) * w;
            }
            if (std::fabs(terrain.surface.at(c, r) - original[i]) < 1.0e-3f) terrain.surface.at(c, r) = g; // solo donde la superficie era el terreno
            height.at(c, r) = g;
        }
    }

    // ---- 5. Escribir las capas ----
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const size_t i = at(c, r);
            if (!ok[i] || !inSet[i]) continue;
            m_floor.at(c, r) = floorNode[i];
            m_ceil.at(c, r) = ceilNode[i];
            m_water.at(c, r) = levelNode[i];
            m_habitat[static_cast<size_t>(r) * n + c] = habitatNode[i];
        }
    }
    for (int r = r0; r < r1; ++r) {
        for (int c = c0; c < c1; ++c) {
            if (!cellDefined(c, r) || !reach[at(c, r)]) continue;
            bool open = false;
            for (const auto& d : { std::pair<int, int>{ 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } }) open = open || height.at(c + d.first, r + d.second) < m_ceil.at(c + d.first, r + d.second) + 0.2f;
            m_hole[static_cast<size_t>(r) * n + c] = open ? 1 : 0;
        }
    }

    // ---- 6. Sistema y decoración ----
    out = System{};
    out.kind = plan.kind;
    out.habitat = plan.habitat;
    out.mouthX = mouth.x; out.mouthZ = mouth.z; out.dirX = mouth.dirX; out.dirZ = mouth.dirZ; out.floorY = S0;
    float sx = 0.0f, sz = 0.0f;
    int count = 0;
    out.minX = out.minZ = INF; out.maxX = out.maxZ = -INF;
    for (int r = r0; r <= r1; ++r) for (int c = c0; c <= c1; ++c) {
        if (!ok[at(c, r)] || !inSet[at(c, r)]) continue;
        const float x = height.nodeCoord(c), z = height.nodeCoord(r);
        sx += x; sz += z; ++count;
        out.minX = (std::min)(out.minX, x); out.maxX = (std::max)(out.maxX, x);
        out.minZ = (std::min)(out.minZ, z); out.maxZ = (std::max)(out.maxZ, z);
    }
    out.x = sx / count; out.z = sz / count;
    out.radius = 0.5f * (std::max)(out.maxX - out.minX, out.maxZ - out.minZ);
    out.propBegin = m_props.size();

    const auto pushProp = [&](Prop::Type type, const XMFLOAT3& a, const XMFLOAT3& b, float w, float h, int color = 0) {
        Prop p;
        p.type = type; p.a = a; p.b = b; p.w = w; p.h = h; p.color = static_cast<uint8_t>(color);
        m_props.push_back(p);
    };
    const auto usable = [&](float x, float z) { return covered(x, z) && cell(x, z); };

    // Roca suelta en cuevas y minas; estalactitas, estalagmitas y columnas en las cuevas
    for (int r = r0; r < r1; ++r) {
        for (int c = c0; c < c1; ++c) {
            const size_t i = at(c, r);
            if (!ok[i] || !inSet[i] || mouthDist[i] < DOOR + 3.0f) continue;
            const float x = height.nodeCoord(c) + uni(-0.9f, 0.9f), z = height.nodeCoord(r) + uni(-0.9f, 0.9f);
            if (!usable(x, z)) continue;
            const float fl = floorAt(x, z), ce = ceilAt(x, z), tall = ce - fl;
            const bool hallNode = wallHall[i] > 0.4f, edge = field[i] < 1.6f;
            const float roll = uni(0.0f, 1.0f);
            if (cave) {
                if (roll < 0.11f) pushProp(Prop::STALACTITE, { x, ce, z }, {}, uni(0.18f, 0.5f), uni(0.6f, (std::max)(0.8f, (std::min)(3.2f, 0.7f * tall))));
                else if (roll < 0.17f && (edge || hallNode)) pushProp(Prop::STALAGMITE, { x, fl, z }, {}, uni(0.2f, 0.55f), uni(0.5f, 2.0f));
                else if (roll < 0.185f && hallNode && tall < 10.0f && depthAt(x, z) < 0.05f) pushProp(Prop::PILLAR, { x, fl, z }, { x, ce, z }, uni(0.5f, 1.0f), 0.0f);
            }
            if (edge && chance(cave ? 0.07f : 0.05f) && depthAt(x, z) < 0.05f) pushProp(Prop::BOULDER, { x, fl + 0.15f, z }, {}, uni(0.4f, 1.1f), 0.0f);
        }
    }
    // Cristales en algunas salas de las cuevas
    if (cave) {
        for (const Hall& h : halls) {
            if (h.dist >= INF || !chance(0.45f)) continue;
            for (int cluster = 0; cluster < 3; ++cluster) {
                const float a = uni(0.0f, TAU), rr = h.r * uni(0.6f, 0.85f);
                const float cx = h.x + std::cos(a) * rr, cz = h.z + std::sin(a) * rr;
                const int color = 3 + static_cast<int>(random() % 3);
                for (int k = 0, crystals = 4 + static_cast<int>(random() % 5); k < crystals; ++k) {
                    const float x = cx + uni(-0.9f, 0.9f), z = cz + uni(-0.9f, 0.9f);
                    if (usable(x, z) && depthAt(x, z) < 0.05f) pushProp(Prop::CRYSTAL, { x, floorAt(x, z), z }, { uni(-0.4f, 0.4f), 0.0f, uni(-0.4f, 0.4f) }, uni(0.15f, 0.4f), uni(0.5f, 1.6f), color);
                }
            }
        }
    }

    // Minas: raíles, durmientes, cuadros de madera, lámparas, vetas y vagonetas a lo largo de las galerías
    if (!cave) {
        const auto pickOre = [&]() { const float v = uni(0.0f, 1.0f); return v < 0.3f ? 0 : v < 0.6f ? 1 : v < 0.85f ? 2 : v < 0.93f ? 4 : 3; };
        for (size_t laneIndex = 0; laneIndex < lanes.size(); ++laneIndex) {
            const Lane& lane = lanes[laneIndex];
            std::vector<XMFLOAT2> samples; // puntos cada ~1.6 m
            for (size_t k = 0; k + 1 < lane.size(); ++k) {
                const float dx = lane[k + 1].x - lane[k].x, dz = lane[k + 1].y - lane[k].y, len = std::hypot(dx, dz);
                for (float t = 0.0f; t < len; t += 1.6f) samples.push_back({ lane[k].x + dx * t / len, lane[k].y + dz * t / len });
            }
            for (size_t k = 0; k + 1 < samples.size(); ++k) {
                const XMFLOAT2& p = samples[k]; const XMFLOAT2& q = samples[k + 1];
                const float dx = q.x - p.x, dz = q.y - p.y, len = (std::max)(std::hypot(dx, dz), 1.0e-3f);
                const float tx = dx / len, tz = dz / len, nx = -tz, nz = tx;
                if (!usable(p.x, p.y) || !usable(q.x, q.y) || !usable(p.x + nx * 0.7f, p.y + nz * 0.7f) || !usable(p.x - nx * 0.7f, p.y - nz * 0.7f)) continue;
                const float py = floorAt(p.x, p.y), qy = floorAt(q.x, q.y);
                pushProp(Prop::SLEEPER, { p.x, py + 0.05f, p.y }, { tx, 0.0f, tz }, 1.7f, 0.0f);
                for (const float side : { -1.0f, 1.0f }) {
                    pushProp(Prop::RAIL, { p.x + nx * 0.55f * side, py + 0.14f, p.y + nz * 0.55f * side }, { q.x + nx * 0.55f * side, qy + 0.14f, q.y + nz * 0.55f * side }, 0.09f, 0.0f);
                }
                const bool frame = k % 4 == 0, lamp = k % 6 == 3;
                if (!frame && !lamp) continue;
                const float left = wallDistance(p.x, p.y, nx, nz, 7.0f), right = wallDistance(p.x, p.y, -nx, -nz, 7.0f);
                if (left < 1.4f || right < 1.4f) continue;
                const float ceilY = ceilAt(p.x, p.y);
                if (frame) {
                    const XMFLOAT3 l = { p.x + nx * (left - 0.3f), py, p.y + nz * (left - 0.3f) }, rgt = { p.x - nx * (right - 0.3f), py, p.y - nz * (right - 0.3f) };
                    pushProp(Prop::POST, l, {}, 0.3f, ceilY - py - 0.1f);
                    pushProp(Prop::POST, rgt, {}, 0.3f, ceilY - py - 0.1f);
                    pushProp(Prop::BEAM, { l.x, ceilY - 0.25f, l.z }, { rgt.x, ceilY - 0.25f, rgt.z }, 0.32f, 0.0f);
                }
                if (lamp) {
                    const float s = (k / 6) % 2 == 0 ? 1.0f : -1.0f, wall = s > 0.0f ? left : right;
                    pushProp(Prop::LAMP, { p.x + nx * s * (wall - 0.2f), py + 2.1f, p.y + nz * s * (wall - 0.2f) }, { -nx * s, 0.0f, -nz * s }, 0.0f, 0.0f);
                }
                if (frame && chance(0.35f)) { // veta de mineral en la pared
                    const float s = chance(0.5f) ? 1.0f : -1.0f, wall = s > 0.0f ? left : right;
                    const int color = pickOre();
                    for (int v = 0, pieces = 3 + static_cast<int>(random() % 4); v < pieces; ++v) {
                        const float along = uni(-1.2f, 1.2f);
                        pushProp(Prop::ORE, { p.x + tx * along + nx * s * (wall - 0.05f), py + uni(0.5f, 2.6f), p.y + tz * along + nz * s * (wall - 0.05f) }, {}, uni(0.2f, 0.5f), 0.0f, color);
                    }
                }
            }
            if (laneIndex == 0 && samples.size() > 8) { // vagonetas sobre la vía principal
                const int carts = 1 + static_cast<int>(random() % 3);
                for (int k = 0; k < carts; ++k) {
                    const size_t at2 = 4 + random() % (samples.size() - 6);
                    const XMFLOAT2& p = samples[at2]; const XMFLOAT2& q = samples[at2 + 1];
                    const float len = (std::max)(std::hypot(q.x - p.x, q.y - p.y), 1.0e-3f);
                    if (usable(p.x, p.y)) pushProp(Prop::CART, { p.x, floorAt(p.x, p.y) + 0.14f, p.y }, { (q.x - p.x) / len, 0.0f, (q.y - p.y) / len }, 1.0f, 0.0f);
                }
            }
        }
        for (const Hall& h : halls) { // salas de arranque: lámparas y mineral en las paredes
            if (h.dist >= INF) continue;
            for (int k = 0; k < 4; ++k) {
                const float a = uni(0.0f, TAU);
                const float wall = wallDistance(h.x, h.z, std::cos(a), std::sin(a), h.r * 1.5f);
                if (wall < 2.0f) continue;
                const float px = h.x + std::cos(a) * (wall - 0.2f), pz = h.z + std::sin(a) * (wall - 0.2f);
                if (k < 2) {
                    pushProp(Prop::LAMP, { px, floorAt(px, pz) + 2.2f, pz }, { -std::cos(a), 0.0f, -std::sin(a) }, 0.0f, 0.0f);
                } else {
                    const int color = pickOre();
                    for (int v = 0; v < 5; ++v) pushProp(Prop::ORE, { px + uni(-1.0f, 1.0f), floorAt(px, pz) + uni(0.5f, 2.6f), pz + uni(-1.0f, 1.0f) }, {}, uni(0.2f, 0.55f), 0.0f, color);
                }
            }
        }
    }
    out.propEnd = m_props.size();
    m_systems.push_back(out);
    return true;
}
