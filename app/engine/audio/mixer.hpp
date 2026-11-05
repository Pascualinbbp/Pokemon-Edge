#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>
#include "wav.hpp"

namespace Audio {
    // Mezclador por software (sin dependencias del sistema: lo alimenta un dispositivo de salida). Estéreo de 16 bits.
    // Sonido posicional: cuanto más cerca esté el oyente, más alto suena; pasada la distancia máxima no suena nada. El balance izquierda /
    // derecha sale de hacia dónde mira el oyente. Las ganancias cambian de forma suave dentro de cada bloque (sin chasquidos).
    class Mixer {
        public:
        static constexpr int   RATE = 22050;     // frecuencia de salida; cada clip se remuestrea con 'step' si tiene otra
        static constexpr int   MAX_VOICES = 32;
        static constexpr float REFERENCE = 1.5f; // a menos de esto suena al máximo
        static constexpr float MAX_PAN = 0.65f;  // cuánto se reparte hacia un lado como mucho

        struct Listener { float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f; };
        struct Play {
            ClipPtr clip;
            bool spatial = true;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            float volume = 1.0f;
            float pitch = 1.0f;
            float maxDistance = 40.0f;
            bool loop = false;
        };

        // Ganancia por distancia: 1 de cerca, cae suavemente (cuadrática) hasta 0 en 'maxDistance'.
        static float distanceGain(float distance, float maxDistance) {
            if (maxDistance <= 0.0f || distance >= maxDistance) return 0.0f;
            if (distance <= REFERENCE) return 1.0f;
            const float t = 1.0f - (distance - REFERENCE) / (maxDistance - REFERENCE);
            return t * t;
        }

        static bool audible(const Listener& at, float x, float y, float z, float maxDistance) {
            const float dx = x - at.x, dy = y - at.y, dz = z - at.z;
            return dx * dx + dy * dy + dz * dz < maxDistance * maxDistance;
        }

        // Reparto izquierda / derecha (potencia constante) de lo que suena en (dx, dz) respecto al oyente que mira a 'yaw'.
        static void pan(float dx, float dz, float yaw, float& left, float& right) {
            const float length = std::sqrt(dx * dx + dz * dz);
            float side = 0.0f; // -1 a la izquierda, +1 a la derecha
            if (length > 0.25f) side = (dx * std::cos(yaw) - dz * std::sin(yaw)) / length; // la derecha del oyente es (cos, -sin)
            const float angle = (std::clamp(side, -1.0f, 1.0f) * MAX_PAN + 1.0f) * 0.78539816f;
            left = std::cos(angle);
            right = std::sin(angle);
        }

        void setListener(const Listener& listener) {
            std::lock_guard<std::mutex> guard(m_lock);
            m_listener = listener;
        }

        void play(const Play& request) {
            if (!request.clip || request.clip->samples.empty()) return;
            std::lock_guard<std::mutex> guard(m_lock);
            if (static_cast<int>(m_voices.size()) >= MAX_VOICES) { // lleno: se descarta el que más ha avanzado
                size_t victim = 0;
                for (size_t i = 1; i < m_voices.size(); ++i) if (m_voices[i].progress() > m_voices[victim].progress()) victim = i;
                m_voices.erase(m_voices.begin() + static_cast<long>(victim));
            }
            m_voices.push_back(make(request));
        }

        // Emisora de radio: suena sobre el oyente en bucle. nullptr = apagada. Con el mismo clip no reinicia; 'start' (0..1) es el punto del clip
        // por el que empieza (para retomar la emisora donde se dejó).
        void setRadio(ClipPtr clip, float volume, float start = 0.0f) {
            std::lock_guard<std::mutex> guard(m_lock);
            m_radioVolume = volume;
            if (clip == m_radioClip) return;
            m_radioClip = clip;
            m_radioOn = clip != nullptr;
            if (!clip) { m_radio = Voice(); return; }
            Play request;
            request.clip = clip;
            request.spatial = false;
            request.volume = volume;
            request.loop = true;
            m_radio = make(request);
            m_radio.pos = static_cast<double>(std::clamp(start, 0.0f, 0.999f)) * static_cast<double>(clip->samples.size());
        }

        // Punto actual de la emisora (0..1 del clip); 0 si está apagada.
        float radioPosition() const {
            std::lock_guard<std::mutex> guard(m_lock);
            return m_radioOn ? std::clamp(m_radio.progress(), 0.0f, 1.0f) : 0.0f;
        }

        int activeVoices() const {
            std::lock_guard<std::mutex> guard(m_lock);
            return static_cast<int>(m_voices.size()) + (m_radioOn ? 1 : 0);
        }

        // Rellena 'frames' fotogramas estéreo entrelazados.
        void render(int16_t* out, int frames) {
            std::lock_guard<std::mutex> guard(m_lock);
            m_mix.assign(static_cast<size_t>(frames) * 2, 0.0f);
            for (Voice& voice : m_voices) mixVoice(voice, frames);
            if (m_radioOn) {
                m_radio.volume = m_radioVolume;
                mixVoice(m_radio, frames);
            }
            m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(), [](const Voice& v) { return v.done; }), m_voices.end());
            for (int i = 0; i < frames * 2; ++i) out[i] = static_cast<int16_t>(std::tanh(m_mix[static_cast<size_t>(i)]) * 32000.0f); // limitador suave
        }

        private:
        struct Voice {
            ClipPtr clip;
            double pos = 0.0;
            double step = 1.0;
            float volume = 1.0f;
            bool spatial = true;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            float maxDistance = 40.0f;
            bool loop = false;
            bool done = false;
            float lastL = -1.0f, lastR = -1.0f; // ganancia del bloque anterior (-1 = primer bloque)
            float progress() const { return clip && !clip->samples.empty() ? static_cast<float>(pos / static_cast<double>(clip->samples.size())) : 1.0f; }
        };

        static Voice make(const Play& request) {
            Voice v;
            v.clip = request.clip;
            v.step = static_cast<double>(request.clip->rate) / RATE * request.pitch;
            v.volume = request.volume;
            v.spatial = request.spatial;
            v.x = request.x; v.y = request.y; v.z = request.z;
            v.maxDistance = request.maxDistance;
            v.loop = request.loop;
            return v;
        }

        void gains(const Voice& v, float& left, float& right) const {
            if (!v.spatial) { left = right = v.volume * 0.7071f; return; }
            const float dx = v.x - m_listener.x, dy = v.y - m_listener.y, dz = v.z - m_listener.z;
            const float g = v.volume * distanceGain(std::sqrt(dx * dx + dy * dy + dz * dz), v.maxDistance);
            float l, r;
            pan(dx, dz, m_listener.yaw, l, r);
            left = g * l;
            right = g * r;
        }

        void mixVoice(Voice& v, int frames) {
            if (v.done) return;
            const std::vector<int16_t>& s = v.clip->samples;
            const double size = static_cast<double>(s.size());
            float tl, tr;
            gains(v, tl, tr);
            float l = v.lastL < 0.0f ? tl : v.lastL, r = v.lastR < 0.0f ? tr : v.lastR;
            const float dl = (tl - l) / static_cast<float>(frames), dr = (tr - r) / static_cast<float>(frames);
            const bool silent = tl <= 0.0005f && l <= 0.0005f;
            for (int i = 0; i < frames; ++i) {
                if (v.pos >= size) {
                    if (!v.loop) { v.done = true; break; }
                    v.pos = std::fmod(v.pos, size);
                }
                if (!silent) {
                    const size_t a = static_cast<size_t>(v.pos);
                    const size_t b = a + 1 < s.size() ? a + 1 : (v.loop ? 0 : a);
                    const float f = static_cast<float>(v.pos - static_cast<double>(a));
                    const float sample = (static_cast<float>(s[a]) * (1.0f - f) + static_cast<float>(s[b]) * f) * (1.0f / 32768.0f);
                    m_mix[static_cast<size_t>(i) * 2] += sample * l;
                    m_mix[static_cast<size_t>(i) * 2 + 1] += sample * r;
                }
                l += dl;
                r += dr;
                v.pos += v.step;
            }
            v.lastL = tl;
            v.lastR = tr;
        }

        mutable std::mutex m_lock;
        Listener m_listener;
        std::vector<Voice> m_voices;
        std::vector<float> m_mix;
        Voice m_radio;
        ClipPtr m_radioClip;
        bool m_radioOn = false;
        float m_radioVolume = 0.5f;
    };
}
