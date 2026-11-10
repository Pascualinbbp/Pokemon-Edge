#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "../../physics/physicsWorld.hpp"

// Fruto caído de un árbol (coco de palmera, plátano de árbol de jungla, cereza de cerezo). Cae, rueda por el suelo con su inercia y, si nadie lo coge,
// se pudre pasado un buen rato. Lo recogen el jugador (tiene prioridad), su acompañante o un pokémon salvaje (que se lo come).
class Fruit {
    public:
    enum class Kind { COCONUT, BANANA, CHERRY };
    // El coco partido son dos mitades (nodos mitad_a y mitad_b del modelo), cada una con su propio cuerpo físico. El racimo de plátanos (WHOLE) lleva
    // un plátano por bit de 'bananas' (nodos platano_1..5 de banana_bunch.glb); un plátano suelto es PIECE y recuerda cuál era (variant).
    enum class Part { WHOLE, HALF_A, HALF_B, PIECE };
    static constexpr int   BUNCH_SIZE     = 5;
    static constexpr float HALF_RADIUS    = 0.10f;  // medidas de los modelos (ya a su tamaño)
    static constexpr float COCONUT_RADIUS = 0.155f;
    static constexpr float BUNCH_RADIUS   = 0.10f;
    static constexpr float BUNCH_HEIGHT   = 0.054f; // del suelo al centro del racimo
    static constexpr float PIECE_RADIUS   = 0.06f;
    static constexpr float PIECE_HEIGHT   = 0.052f;
    static constexpr float HALF_LIFETIME  = 90.0f;  // una mitad suelta dura menos en el suelo
    static constexpr float RADIUS         = 0.13f;
    static constexpr float INTERACT_RANGE = 1.8f;
    static constexpr float LIFETIME       = 420.0f; // segundos en el suelo antes de pudrirse
    static constexpr float FADE_TIME      = 0.35f;
    static constexpr float ROT_FADE       = 6.0f;   // últimos segundos de vida: se encoge

    Physics::Body body;

    Fruit(Kind kind, int id, const DirectX::XMFLOAT3& from, float vx, float vz, Part part = Part::WHOLE, float vy = 0.0f, int variant = 0) : m_kind(kind), m_id(id), m_part(part), m_variant(variant) {
        body.position = from;
        body.velocity = { vx, vy, vz };
        const bool bunch = kind == Kind::BANANA && part == Part::WHOLE, piece = part == Part::PIECE;
        const float r = piece ? PIECE_RADIUS : bunch ? BUNCH_RADIUS : half() ? HALF_RADIUS : kind == Kind::COCONUT ? COCONUT_RADIUS : RADIUS;
        body.groundOffset = piece ? PIECE_HEIGHT : bunch ? BUNCH_HEIGHT : r;
        body.collisionRadius = r;
        body.collisionHeight = body.groundOffset * 2.0f;
        body.restitution = loose() ? 0.25f : kind == Kind::COCONUT ? 0.35f : kind == Kind::CHERRY ? 0.2f : 0.12f;
        body.groundDrag = loose() ? 2.4f : kind == Kind::COCONUT ? 0.55f : kind == Kind::CHERRY ? 1.2f : 1.6f; // el coco rueda lejos; plátano y cereza se quedan cerca; lo suelto se frena
        body.rolls = part == Part::WHOLE && kind == Kind::COCONUT;
    }

    Kind kind() const { return m_kind; }
    int id() const { return m_id; }
    float reach() const { return INTERACT_RANGE; }
    Part part() const { return m_part; }
    bool half() const { return m_part == Part::HALF_A || m_part == Part::HALF_B; }
    bool piece() const { return m_part == Part::PIECE; }
    bool loose() const { return m_part != Part::WHOLE; } // mitad de coco o plátano suelto
    bool bunch() const { return m_kind == Kind::BANANA && m_part == Part::WHOLE; }
    int variant() const { return m_variant; } // qué plátano del racimo era (0..4)
    // Plátanos que quedan en el racimo (bits 0..4).
    unsigned bananas() const { return m_bananas; }
    int bananaCount() const { int n = 0; for (int i = 0; i < BUNCH_SIZE; ++i) n += (m_bananas >> i) & 1u; return n; }
    bool hasBanana(int i) const { return (m_bananas >> i) & 1u; }
    // Quita el plátano 'which'-ésimo de los que quedan (0 = el primero); devuelve su variante o -1 si no queda ninguno.
    int removeBanana(int which) {
        for (int i = 0, n = 0; i < BUNCH_SIZE; ++i) {
            if (!hasBanana(i)) continue;
            if (n++ == which) { m_bananas &= ~(1u << i); return i; }
        }
        return -1;
    }
    // Racimo que se está cayendo de un árbol: al tocar el suelo pierde plátanos al azar.
    void setShedsOnLanding(bool value) { m_sheds = value; }
    bool shedsOnLanding() const { return m_sheds && bunch(); }
    void landed() { m_sheds = false; }
    float lifetime() const { return loose() ? HALF_LIFETIME : LIFETIME; }
    bool available() const { return !m_taken && m_age < lifetime(); }
    bool finished() const { return (m_taken && m_fade >= m_fadeTime) || m_age >= lifetime(); }
    // Desaparece encogiéndose en 'duration' segundos, tras 'delay' segundos sin cambios (suave, nada brusco).
    void take(float duration = FADE_TIME, float delay = 0.0f) { m_taken = true; m_fadeTime = duration; m_fade = -delay; }
    // Se sustituye por otra cosa (el coco entero que se parte): deja de existir al instante.
    void consume() { m_taken = true; m_fadeTime = 0.0f; m_fade = 0.0f; }
    // Coco que caerá de un árbol y se romperá al tocar el suelo.
    void setBreaksOnLanding(bool value) { m_breaks = value; }
    bool breaksOnLanding() const { return m_breaks && !half(); }
    float yaw() const { return static_cast<float>(m_id) * 1.7f; }
    float spin() const { return m_roll; }
    // Volumen del golpe al tocar el suelo (0 = no acaba de tocarlo): el primero suena entero y cada bote siguiente, más flojo.
    float takeLanding() {
        const bool landed = body.onGround && !m_wasGround;
        m_wasGround = body.onGround;
        return landed ? std::pow(0.5f, static_cast<float>(m_landings++)) : 0.0f;
    }
    bool settled() const { return body.onGround; }

    void update(float dt) {
        if (body.onGround) {
            m_age += dt;
            m_roll += std::hypot(body.velocity.x, body.velocity.z) * dt / body.collisionRadius; // gira según avanza
        } else m_age += dt * 0.25f;
        if (m_taken) m_fade += dt;
    }

    float scale() const {
        if (m_taken) return m_fadeTime <= 0.0f ? 0.0f : (std::max)(0.0f, 1.0f - (std::max)(0.0f, m_fade) / m_fadeTime);
        const float left = lifetime() - m_age;
        return left < ROT_FADE ? (std::max)(0.0f, left / ROT_FADE) : 1.0f;
    }

    private:
    Kind m_kind;
    int m_id;
    Part m_part = Part::WHOLE;
    int m_variant = 0;
    unsigned m_bananas = (1u << BUNCH_SIZE) - 1u;
    bool m_sheds = false;
    bool m_breaks = false;
    float m_fadeTime = FADE_TIME;
    bool m_taken = false;
    float m_age = 0.0f;
    float m_fade = 0.0f;
    float m_roll = 0.0f;
    bool m_wasGround = false;
    int m_landings = 0;
};
