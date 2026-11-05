#pragma once
#include <array>
#include <functional>
#include <string>
#include <vector>
#include "audioDevice.hpp"
#include "mixer.hpp"
#include "radio.hpp"
#include "sounds.hpp"

namespace Audio {
    // Motor de audio del juego: traduce los eventos de Sound (efectos con posición) y la emisora elegida en voces del mezclador.
    //  - Solo se carga lo que suena: un efecto fuera del alcance de su 'maxDistance' ni se pide ni se lee; los ficheros se cargan al
    //    primer uso, se guardan mientras se usan y se descargan tras un rato sin sonar. De la radio solo está en memoria la emisora actual.
    //  - 'Loader' devuelve el clip de un fichero de assets/ (nullptr si no existe): así el motor no sabe de dónde salen los datos.
    class AudioEngine {
        public:
        using Loader = std::function<ClipPtr(const std::string& path)>;
        static constexpr float RADIO_VOLUME = 0.42f;
        static constexpr float UNLOAD_AFTER = 90.0f; // segundos sin sonar para soltar un efecto
        static constexpr float SWEEP_EVERY = 20.0f;

        bool init(Loader loader) {
            m_loader = std::move(loader);
            m_clock = m_lastSweep = 0.0f;
            return m_device.start(Mixer::RATE, [this](int16_t* out, int frames) { m_mixer.render(out, frames); });
        }

        void shutdown() {
            m_device.stop();
            m_mixer.setRadio(nullptr, 0.0f);
            m_station = 0;
            m_resume.fill(0.0f);
            for (Slot& slot : m_slots) slot = {};
        }

        // Cada fotograma de juego: oyente, efectos pedidos y emisora (0 = apagada).
        void update(float dt, const Mixer::Listener& listener, int station) {
            m_clock += dt;
            m_mixer.setListener(listener);
            m_events.clear();
            Sound::drain(m_events);
            for (const Sound::Event& event : m_events) playEffect(event, listener);
            setStation(station);
            if (m_clock - m_lastSweep >= SWEEP_EVERY) sweep();
        }

        // Fuera de la partida: sin radio.
        void silence() { setStation(0); }

        Mixer& mixer() { return m_mixer; }
        int loadedEffects() const { int n = 0; for (const Slot& s : m_slots) n += s.clip ? 1 : 0; return n; }

        private:
        struct Slot { ClipPtr clip; float lastUsed = 0.0f; bool failed = false; };

        ClipPtr effectClip(Sound::Sfx sfx) {
            Slot& slot = m_slots[static_cast<size_t>(sfx)];
            slot.lastUsed = m_clock;
            if (!slot.clip && !slot.failed && m_loader) {
                slot.clip = m_loader(Sound::info(sfx).path);
                slot.failed = !slot.clip; // no se reintenta cada vez que debería sonar
            }
            return slot.clip;
        }

        void playEffect(const Sound::Event& event, const Mixer::Listener& listener) {
            const Sound::Info& info = Sound::info(event.sfx);
            if (!event.global && !Mixer::audible(listener, event.at.x, event.at.y, event.at.z, info.maxDistance)) return; // demasiado lejos: ni se carga
            Mixer::Play play;
            play.clip = effectClip(event.sfx);
            if (!play.clip) return;
            play.spatial = !event.global;
            play.x = event.at.x; play.y = event.at.y; play.z = event.at.z;
            play.volume = info.volume * event.volume;
            play.maxDistance = info.maxDistance;
            play.pitch = event.pitch * (1.0f + info.pitchJitter * jitter());
            m_mixer.play(play);
        }

        void setStation(int station) {
            if (station == m_station) return;
            if (m_station > 0) m_resume[static_cast<size_t>(m_station)] = m_mixer.radioPosition(); // la emisora que se deja guarda por dónde iba
            m_station = station;
            ClipPtr clip;
            if (station > 0 && m_loader) clip = m_loader(Radio::station(station).path); // solo la emisora actual está en memoria
            m_mixer.setRadio(clip, RADIO_VOLUME, station > 0 ? m_resume[static_cast<size_t>(station)] : 0.0f); // y al volver a ella sigue desde ahí
        }

        void sweep() {
            m_lastSweep = m_clock;
            for (Slot& slot : m_slots) if (slot.clip && m_clock - slot.lastUsed > UNLOAD_AFTER) slot.clip.reset();
        }

        float jitter() { m_seed = m_seed * 1664525u + 1013904223u; return static_cast<float>((m_seed >> 8) & 0xFFFF) / 32767.5f - 1.0f; } // -1..1

        Loader m_loader;
        AudioDevice m_device;
        Mixer m_mixer;
        std::array<Slot, static_cast<size_t>(Sound::Sfx::COUNT)> m_slots;
        std::vector<Sound::Event> m_events;
        int m_station = 0;
        std::array<float, Radio::COUNT + 1> m_resume{}; // por dónde iba cada emisora (0..1 de su fichero)
        float m_clock = 0.0f, m_lastSweep = 0.0f;
        unsigned m_seed = 2463534242u;
    };
}
