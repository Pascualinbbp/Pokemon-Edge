#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

// Ciclo de día y noche. Para las pruebas dura pocos minutos: cambia las dos constantes de duración.
// El sol recorre un arco de este (+X) a oeste (-X); la luna va siempre en el punto opuesto del cielo.
class DayCycle {
    public:
    static constexpr float DAY_SECONDS   = 180.0f; // sol sobre el horizonte
    static constexpr float NIGHT_SECONDS = 120.0f; // sol bajo el horizonte
    static constexpr float CYCLE_SECONDS = DAY_SECONDS + NIGHT_SECONDS;
    static constexpr float START_TIME    = 25.0f;  // se empieza por la mañana
    static constexpr float MIN_LIGHT_ELEVATION = 0.22f; // la luz nunca llega más rasante: así las sombras siguen existiendo al amanecer y al atardecer

    // Todo lo que el renderer necesita para dibujar el cielo, la luz y las sombras de este instante.
    struct Lighting {
        DirectX::XMFLOAT3 lightDir;       // hacia la luz activa (sol de día, luna de noche)
        DirectX::XMFLOAT3 lightColor;     // color e intensidad de la luz activa (0 al cruzar el horizonte)
        DirectX::XMFLOAT3 ambientSky;     // luz ambiente desde arriba
        DirectX::XMFLOAT3 ambientGround;  // luz ambiente rebotada desde abajo
        DirectX::XMFLOAT3 skyZenith;
        DirectX::XMFLOAT3 skyHorizon;
        DirectX::XMFLOAT3 sunDir;
        DirectX::XMFLOAT3 moonDir;
        float starVisibility;             // 0 = de día, 1 = noche cerrada
        float warm;                       // tinte anaranjado de amanecer y atardecer
        float angle;                      // posición del sol en su arco (radianes)
        float time;                       // segundos dentro del ciclo
    };

    void update(float dt) {
        m_time += dt;
        if (m_time >= CYCLE_SECONDS) {
            m_time = std::fmod(m_time, CYCLE_SECONDS);
            ++m_day;
        }
    }

    float time() const { return m_time; }
    int day() const { return m_day; } // días completos transcurridos desde que se creó la partida (no se guarda)

    // Restaura la hora de una partida guardada (un valor no válido deja la hora de inicio).
    void setTime(float seconds) {
        if (!std::isfinite(seconds) || seconds < 0.0f) return;
        m_time = std::fmod(seconds, CYCLE_SECONDS);
    }

    Lighting lighting() const {
        using DirectX::XMFLOAT3;
        const float a = angle();
        XMFLOAT3 sun = normalize({ std::cos(a), std::sin(a), 0.30f });
        const XMFLOAT3 moon = { -sun.x, -sun.y, -sun.z };
        const float sy = sun.y;

        const float day = smooth(-0.15f, 0.25f, sy);     // mezcla de ambiente y cielo
        const float sunI = smooth(0.0f, 0.25f, sy);      // la intensidad de cada luz pasa por 0 en el horizonte,
        const float moonI = smooth(0.0f, 0.25f, -sy);    // así el cambio de sol a luna no se nota
        const float warm = (std::max)(0.0f, 1.0f - std::fabs(sy) / 0.30f);

        Lighting l;
        l.sunDir = sun;
        l.moonDir = moon;
        l.angle = a;
        l.time = m_time;
        l.warm = warm;
        l.starVisibility = 1.0f - smooth(-0.20f, 0.05f, sy);

        if (sy > 0.0f) {
            l.lightDir = raised(sun);
            const XMFLOAT3 noon = { 0.80f, 0.76f, 0.68f };
            const XMFLOAT3 dusk = { 0.85f, 0.45f, 0.24f };
            l.lightColor = scale(mix(noon, dusk, warm), sunI);
        } else {
            l.lightDir = raised(moon);
            l.lightColor = scale({ 0.26f, 0.31f, 0.48f }, moonI);
        }

        l.ambientSky = mix({ 0.19f, 0.23f, 0.37f }, { 0.44f, 0.49f, 0.58f }, day);
        l.ambientGround = mix({ 0.11f, 0.13f, 0.21f }, { 0.30f, 0.30f, 0.27f }, day);
        l.skyZenith = mix({ 0.015f, 0.03f, 0.10f }, { 0.20f, 0.45f, 0.86f }, day);
        l.skyHorizon = mix({ 0.05f, 0.08f, 0.18f }, { 0.66f, 0.80f, 0.95f }, day);
        return l;
    }

    private:
    // 0..π durante el día y π..2π durante la noche (cada tramo con su propia duración).
    float angle() const {
        constexpr float PI = 3.14159265f;
        if (m_time < DAY_SECONDS) return PI * m_time / DAY_SECONDS;
        return PI + PI * (m_time - DAY_SECONDS) / NIGHT_SECONDS;
    }

    static float smooth(float e0, float e1, float x) {
        const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    static DirectX::XMFLOAT3 raised(DirectX::XMFLOAT3 v) {
        v.y = (std::max)(v.y, MIN_LIGHT_ELEVATION);
        return normalize(v);
    }
    static DirectX::XMFLOAT3 normalize(DirectX::XMFLOAT3 v) {
        const float n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return { v.x / n, v.y / n, v.z / n };
    }
    static DirectX::XMFLOAT3 mix(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
    }
    static DirectX::XMFLOAT3 scale(const DirectX::XMFLOAT3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }

    float m_time = START_TIME;
    int m_day = 0;
};
