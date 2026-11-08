#pragma once
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include "audioDevice.hpp"
#include "mixer.hpp"
#include "radio.hpp"
#include "sounds.hpp"
#include "../../utils/core/loggerUtil.hpp"

namespace Audio {
    // Motor de audio del juego: traduce los eventos de Sound (efectos con posición) y la emisora elegida en voces del mezclador.
    //  - Solo se carga lo que suena: un efecto fuera del alcance de su 'maxDistance' ni se pide ni se lee; los ficheros se cargan al
    //    primer uso, se guardan mientras se usan y se descargan tras un rato sin sonar. De la radio solo está abierta la emisora actual.
    //  - Todas las emisoras "emiten a la vez": hay un reloj de emisión común y, al sintonizar una, empieza en el punto que le toca según ese reloj
    //    (módulo su duración, así que las más cortas vuelven a empezar). Las demás no se abren, no se leen ni gastan nada.
    //  - 'Loader' devuelve el clip de un fichero de assets/ (nullptr si no existe): así el motor no sabe de dónde salen los datos.
    class AudioEngine {
        public:
        using Loader = std::function<ClipPtr(const std::string& path)>;           // efectos: un WAV de assets/
        using RadioLoader = std::function<RadioPtr(const std::string& path)>;   // emisoras: su fichero (MP3 o WAV) sin extensión
        static constexpr float RADIO_VOLUME = 0.42f;
        static constexpr float UNLOAD_AFTER = 90.0f; // segundos sin sonar para soltar un efecto
        static constexpr float SWEEP_EVERY = 20.0f;
        static constexpr float REPORT_EVERY = 30.0f; // segundos entre resúmenes del estado de la radio en el log

        bool init(Loader loader, RadioLoader radioLoader) {
            m_loader = std::move(loader);
            m_radioLoader = std::move(radioLoader);
            m_clock = m_lastSweep = 0.0f;
            m_start = std::chrono::steady_clock::now();
            m_epoch = std::fmod(static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()), 86400.0); // dónde va la emisión al arrancar
            Logger::logInfo("RADIO", "Audio iniciado. Reloj de emisión en " + std::to_string(static_cast<int>(m_epoch)) + " s. Emisoras: " + std::to_string(Radio::count()));
            return m_device.start(Mixer::RATE, [this](int16_t* out, int frames) { m_mixer.render(out, frames); });
        }

        void shutdown() {
            m_device.stop();
            m_mixer.shutdownRadio();
            m_station = 0;
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
            reportRadio();
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

        // Segundos de emisión: el reloj común de todas las emisoras (avanza aunque el juego esté en pausa o la radio apagada).
        double broadcastSeconds() const {
            return m_epoch + std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
        }

        void setStation(int station) {
            if (station == m_station) return;
            Logger::logInfo("RADIO", "Cambio de emisora " + std::to_string(m_station) + " -> " + std::to_string(station) + " (reloj de emisión " + std::to_string(static_cast<long long>(broadcastSeconds())) + " s).");
            m_station = station;
            m_lastReport = m_clock;
            if (station <= 0 || !m_radioLoader) { m_mixer.radioOff(); return; }
            const std::string path = Radio::music(station);
            RadioLoader loader = m_radioLoader;
            m_mixer.tuneRadio(station, [loader, path, station]() -> RadioPtr {
                const auto t0 = std::chrono::steady_clock::now();
                RadioPtr source = loader(path); // en el hilo lector: abrir un MP3 largo indexa todo el fichero
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                Logger::logInfo("RADIO", "Emisora " + std::to_string(station) + " (" + path + ") " + (source ? "abierta" : "NO SE PUDO ABRIR") + " en " + std::to_string(static_cast<int>(ms)) + " ms.");
                return source;
            }, [this] { return broadcastSeconds(); }, RADIO_VOLUME);
        }

        // Informes de la radio: lo que pasa en el hilo lector (apertura, fallos) al momento y un resumen cada REPORT_EVERY segundos mientras suena;
        // los cortes (underruns) se avisan en cuanto se detectan, con límite para no llenar el log.
        void reportRadio() {
            RadioStream& stream = m_mixer.radio();
            const RadioStream::Event event = stream.takeEvent();
            if (event.any) {
                if (event.failed) Logger::logError("RADIO", "La emisora " + std::to_string(event.station) + " dejó de tener audio (fichero ausente, corrupto o vacío).");
                else Logger::logInfo("RADIO", "Emisora " + std::to_string(event.station) + " sintonizada: dura " + fixed(static_cast<double>(event.length) / (std::max)(1, event.rate), 1) + " s, empieza en " +
                    fixed(static_cast<double>(event.startFrame) / (std::max)(1, event.rate), 1) + " s (" + std::to_string(event.rate) + " Hz).");
            }
            RadioStats& st = stream.stats;
            const uint32_t underruns = st.underruns.load(std::memory_order_relaxed);
            if (underruns != m_seenUnderruns && m_clock - m_lastUnderrunLog >= 1.0f) {
                m_lastUnderrunLog = m_clock;
                Logger::logError("RADIO", "CORTE de audio: +" + std::to_string(underruns - m_seenUnderruns) + " (total " + std::to_string(underruns) + "), emisora " + std::to_string(m_station) +
                    ", reserva del anillo " + std::to_string(stream.buffered()) + " fotogramas, decodificación máx. " + std::to_string(st.decodeMaxUs.load()) + " us, preparar búfer máx. " + std::to_string(st.renderMaxUs.load()) + " us.");
                m_seenUnderruns = underruns;
            }
            if (m_station <= 0 || m_clock - m_lastReport < REPORT_EVERY) return;
            m_lastReport = m_clock;
            const uint32_t renders = st.renders.exchange(0), decodes = st.decodes.exchange(0);
            const uint64_t renderTotal = st.renderTotalUs.exchange(0), decodeTotal = st.decodeTotalUs.exchange(0);
            const uint32_t low = st.ringLowest.exchange(0xFFFFFFFFu);
            Logger::logInfo("RADIO", "Estado emisora " + std::to_string(m_station) + " | emisión " + std::to_string(static_cast<long long>(broadcastSeconds())) + " s | cortes " + std::to_string(underruns) + " (" +
                std::to_string(st.missingFrames.load()) + " fotogramas) | vueltas al inicio " + std::to_string(st.loops.load()) + " | reserva mín. " + (low == 0xFFFFFFFFu ? std::string("-") : std::to_string(low)) +
                " ahora " + std::to_string(stream.buffered()) + " | búfer audio: media " + std::to_string(renders ? renderTotal / renders : 0) + " us, máx. " + std::to_string(st.renderMaxUs.exchange(0)) +
                " us (" + std::to_string(renders) + ") | descodificar: media " + std::to_string(decodes ? decodeTotal / decodes : 0) + " us, máx. " + std::to_string(st.decodeMaxUs.exchange(0)) + " us (" + std::to_string(decodes) +
                ") | huecos del dispositivo " + std::to_string(m_device.lateRefills()) + ", máx. sin servir " + std::to_string(m_device.maxGapMs()) + " ms | voces " + std::to_string(m_mixer.activeVoices()));
        }

        static std::string fixed(double value, int decimals) {
            char text[32];
            std::snprintf(text, sizeof(text), "%.*f", decimals, value);
            return text;
        }

        void sweep() {
            m_lastSweep = m_clock;
            for (Slot& slot : m_slots) if (slot.clip && m_clock - slot.lastUsed > UNLOAD_AFTER) slot.clip.reset();
        }

        float jitter() { m_seed = m_seed * 1664525u + 1013904223u; return static_cast<float>((m_seed >> 8) & 0xFFFF) / 32767.5f - 1.0f; } // -1..1

        Loader m_loader;
        RadioLoader m_radioLoader;
        AudioDevice m_device;
        Mixer m_mixer;
        std::array<Slot, static_cast<size_t>(Sound::Sfx::COUNT)> m_slots;
        std::vector<Sound::Event> m_events;
        int m_station = 0;
        std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
        double m_epoch = 0.0;                 // segundos de emisión en el instante de arrancar el audio
        float m_lastReport = 0.0f, m_lastUnderrunLog = -10.0f;
        uint32_t m_seenUnderruns = 0;
        float m_clock = 0.0f, m_lastSweep = 0.0f;
        unsigned m_seed = 2463534242u;
    };
}
