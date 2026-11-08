#pragma once
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "radioSource.hpp"

namespace Audio {
    // Contadores del audio de la radio (se leen desde el hilo del juego para los logs; el hilo de audio solo suma, nunca escribe en disco).
    struct RadioStats {
        std::atomic<uint32_t> underruns{ 0 };      // veces que el audio pidió más de lo que había leído (se oye un corte)
        std::atomic<uint64_t> missingFrames{ 0 };  // fotogramas de silencio metidos por esos cortes
        std::atomic<uint32_t> renderMaxUs{ 0 };    // lo que más tardó el hilo de audio en preparar un búfer (desde el último informe)
        std::atomic<uint64_t> renderTotalUs{ 0 };
        std::atomic<uint32_t> renders{ 0 };
        std::atomic<uint32_t> decodeMaxUs{ 0 };    // lo que más tardó el hilo lector en descodificar un trozo
        std::atomic<uint64_t> decodeTotalUs{ 0 };
        std::atomic<uint32_t> decodes{ 0 };
        std::atomic<uint32_t> ringLowest{ 0xFFFFFFFFu }; // el mínimo de fotogramas que hubo de reserva (desde el último informe)
        std::atomic<uint32_t> loops{ 0 };          // veces que la fuente llegó al final y volvió al principio
    };

    // Lectura de la emisora en un hilo aparte. Descodificar MP3 (y abrirlo, que indexa todo el fichero) ya no ocurre en el hilo de audio ni en el del
    // juego: este hilo mantiene un anillo con ~2 s de audio ya descodificado y el hilo de audio solo copia y remuestrea desde él.
    // Todas las emisoras "emiten a la vez": al sintonizar se calcula el punto exacto según un reloj común (ver AudioEngine), sin reproducir nada
    // de las demás.
    class RadioStream {
        public:
        using Opener = std::function<RadioPtr()>;
        using Clock = std::function<double()>; // segundos de emisión

        static constexpr size_t CAPACITY = 48000 * 4; // fotogramas del anillo
        static constexpr size_t TARGET = 48000 * 2;   // se rellena hasta aquí
        static constexpr size_t CHUNK = 4096;
        static constexpr size_t PRIME = 8192;         // reserva con la que empieza a sonar

        RadioStream() = default;
        ~RadioStream() { stop(); }
        RadioStream(const RadioStream&) = delete;
        RadioStream& operator=(const RadioStream&) = delete;

        RadioStats stats;

        void start() {
            if (m_thread.joinable()) return;
            m_quit = false;
            m_ring.assign(CAPACITY * 2, 0);
            m_thread = std::thread([this] { run(); });
        }

        void stop() {
            if (!m_thread.joinable()) return;
            {
                std::lock_guard<std::mutex> lock(m_lock);
                m_quit = true;
            }
            m_wake.notify_all();
            m_thread.join();
        }

        // Sintoniza: 'open' se ejecuta en el hilo lector (carga e indexa) y 'clock' da los segundos de emisión en el momento de empezar a leer.
        void tune(int station, Opener open, Clock clock) {
            {
                std::lock_guard<std::mutex> lock(m_lock);
                m_job = { station, std::move(open), std::move(clock) };
                m_hasJob = true;
                m_source.reset();
                m_playing = false;
                m_primed = false;
                m_count = 0;
                m_head = 0;
                ++m_generation;
            }
            m_wake.notify_all();
        }

        void off() {
            {
                std::lock_guard<std::mutex> lock(m_lock);
                m_hasJob = false;
                m_source.reset();
                m_playing = false;
                m_primed = false;
                m_count = 0;
                m_head = 0;
                ++m_generation;
            }
            m_wake.notify_all();
        }

        bool playing() const { std::lock_guard<std::mutex> lock(m_lock); return m_playing; }
        int rate() const { std::lock_guard<std::mutex> lock(m_lock); return m_rate; }
        int station() const { std::lock_guard<std::mutex> lock(m_lock); return m_station; }
        size_t buffered() const { std::lock_guard<std::mutex> lock(m_lock); return m_count; }

        enum class Result { OK, NOT_READY, UNDERRUN };
        // Desde el hilo de audio: lee del anillo y avanza.
        // 'fn(ring, head, capacity)' lee del anillo sin copiar; 'need' = fotogramas necesarios; devuelve cuántos fotogramas consumió.
        template <typename Fn>
        Result consume(size_t need, Fn&& fn, size_t& consumed) {
            std::lock_guard<std::mutex> lock(m_lock); // el lector solo lo retiene lo que tarda en copiar un trozo
            if (!m_playing || !m_primed) return Result::NOT_READY; // sin emisora o todavía llenando el anillo (no es un corte)
            if (m_count < need) { m_wantMore = true; m_wake.notify_one(); return Result::UNDERRUN; }
            consumed = fn(m_ring.data(), m_head, CAPACITY);
            if (consumed > m_count) consumed = m_count;
            m_head = (m_head + consumed) % CAPACITY;
            m_count -= consumed;
            const uint32_t left = static_cast<uint32_t>(m_count);
            uint32_t low = stats.ringLowest.load(std::memory_order_relaxed);
            while (left < low && !stats.ringLowest.compare_exchange_weak(low, left, std::memory_order_relaxed)) {}
            if (m_count < TARGET / 2) m_wake.notify_one();
            return Result::OK;
        }

        private:
        struct Job { int station = 0; Opener open; Clock clock; };

        static uint32_t micros(std::chrono::steady_clock::time_point from) {
            return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - from).count());
        }
        static void raise(std::atomic<uint32_t>& value, uint32_t candidate) {
            uint32_t seen = value.load(std::memory_order_relaxed);
            while (candidate > seen && !value.compare_exchange_weak(seen, candidate, std::memory_order_relaxed)) {}
        }

        void run() {
            std::vector<int16_t> chunk(CHUNK * 2);
            while (true) {
                Job job;
                uint64_t generation = 0;
                bool open = false;
                {
                    std::unique_lock<std::mutex> lock(m_lock);
                    m_wake.wait_for(lock, std::chrono::milliseconds(200), [this] { return m_quit || m_hasJob || (m_source && m_count < TARGET); });
                    if (m_quit) return;
                    if (m_hasJob) { job = std::move(m_job); m_hasJob = false; open = true; generation = m_generation; }
                }
                if (open) {
                    RadioPtr source = job.open ? job.open() : nullptr;
                    if (source) {
                        const size_t length = source->length();
                        const double seconds = job.clock ? job.clock() : 0.0;
                        const size_t start = length ? static_cast<size_t>(std::fmod(seconds, static_cast<double>(length) / source->rate()) * source->rate()) : 0;
                        source->seek(start);
                        std::lock_guard<std::mutex> lock(m_lock);
                        if (generation == m_generation) { // no se ha cambiado de emisora mientras se abría
                            m_source = std::move(source);
                            m_rate = m_source->rate();
                            m_station = job.station;
                            m_count = 0;
                            m_head = 0;
                            m_playing = true;
                            m_primed = false;
                            m_openedAt = start;
                            m_openedLength = length;
                            m_event = true;
                        }
                    } else {
                        std::lock_guard<std::mutex> lock(m_lock);
                        if (generation == m_generation) { m_station = job.station; m_failed = true; m_event = true; }
                    }
                    continue;
                }
                // Rellena el anillo.
                while (true) {
                    RadioPtr source;
                    uint64_t gen;
                    {
                        std::lock_guard<std::mutex> lock(m_lock);
                        if (m_quit) return;
                        if (!m_source || m_count >= TARGET || m_hasJob) break;
                        source = m_source;
                        gen = m_generation;
                    }
                    const auto t0 = std::chrono::steady_clock::now();
                    const size_t before = source->position();
                    const size_t got = source->read(chunk.data(), CHUNK);
                    const uint32_t us = micros(t0);
                    stats.decodes.fetch_add(1, std::memory_order_relaxed);
                    stats.decodeTotalUs.fetch_add(us, std::memory_order_relaxed);
                    raise(stats.decodeMaxUs, us);
                    if (got > 0 && source->position() < before) stats.loops.fetch_add(1, std::memory_order_relaxed);
                    std::lock_guard<std::mutex> lock(m_lock);
                    if (gen != m_generation) break;
                    if (got == 0) { m_source.reset(); m_playing = false; m_failed = true; m_event = true; break; }
                    for (size_t i = 0; i < got; ++i) {
                        const size_t at = (m_head + m_count + i) % CAPACITY;
                        m_ring[at * 2] = chunk[i * 2];
                        m_ring[at * 2 + 1] = chunk[i * 2 + 1];
                    }
                    m_count += got;
                    if (!m_primed && m_count >= PRIME) m_primed = true;
                }
            }
        }

        public:
        // Para los logs (hilo del juego): cambios de estado ocurridos en el hilo lector.
        struct Event { bool any = false; bool failed = false; int station = 0; size_t startFrame = 0; size_t length = 0; int rate = 0; };
        Event takeEvent() {
            std::lock_guard<std::mutex> lock(m_lock);
            Event e;
            if (!m_event) return e;
            e = { true, m_failed, m_station, m_openedAt, m_openedLength, m_rate };
            m_event = false;
            m_failed = false;
            return e;
        }

        private:
        mutable std::mutex m_lock;
        std::condition_variable m_wake;
        std::thread m_thread;
        std::vector<int16_t> m_ring;
        size_t m_head = 0, m_count = 0;
        RadioPtr m_source;
        Job m_job;
        bool m_hasJob = false, m_playing = false, m_primed = false, m_quit = false, m_wantMore = false, m_event = false, m_failed = false;
        int m_rate = 44100, m_station = 0;
        size_t m_openedAt = 0, m_openedLength = 0;
        uint64_t m_generation = 0;
    };
}
