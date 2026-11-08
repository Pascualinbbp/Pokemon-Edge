#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace Audio {
    // Salida de sonido del sistema (waveOut de Windows): un hilo mantiene unos pocos búferes pequeños en cola y pide a 'fill' más
    // muestras estéreo de 16 bits cada vez que uno termina. Fuera de Windows no hace nada (pruebas).
    class AudioDevice {
        public:
        using Fill = std::function<void(int16_t* out, int frames)>;
        static constexpr int BUFFERS = 6; // ~140 ms en cola: aguanta un tropiezo del juego sin cortar el sonido
        static constexpr int FRAMES = 1024; // ~23 ms por búfer a 44 100 Hz: poca latencia sin cortes

        // Para los logs: veces que un búfer llevaba más de un periodo terminado cuando se rellenó (el hilo llegó tarde) y el mayor tiempo que estuvo así.
        uint32_t lateRefills() const { return m_late.load(std::memory_order_relaxed); }
        uint32_t maxGapMs() { return m_maxGap.exchange(0, std::memory_order_relaxed); }

#ifdef _WIN32
        bool start(int rate, Fill fill) {
            if (m_running) return true;
            m_fill = std::move(fill);
            WAVEFORMATEX format = {};
            format.wFormatTag = WAVE_FORMAT_PCM;
            format.nChannels = 2;
            format.nSamplesPerSec = static_cast<DWORD>(rate);
            format.wBitsPerSample = 16;
            format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
            format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
            m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (!m_event || waveOutOpen(&m_out, WAVE_MAPPER, &format, reinterpret_cast<DWORD_PTR>(m_event), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
                if (m_event) CloseHandle(m_event);
                m_event = nullptr;
                m_out = nullptr;
                return false;
            }
            for (int i = 0; i < BUFFERS; ++i) {
                m_data[i].assign(static_cast<size_t>(FRAMES) * 2, 0);
                m_header[i] = {};
                m_header[i].lpData = reinterpret_cast<LPSTR>(m_data[i].data());
                m_header[i].dwBufferLength = static_cast<DWORD>(m_data[i].size() * sizeof(int16_t));
                waveOutPrepareHeader(m_out, &m_header[i], sizeof(WAVEHDR));
                queue(i);
            }
            m_running = true;
            timeBeginPeriod(1); // temporizador de 1 ms: el hilo se despierta a tiempo
            m_thread = std::thread([this] {
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL); // el juego no debe dejar sin CPU al audio
                auto last = std::chrono::steady_clock::now();
                while (m_running) {
                    WaitForSingleObject(m_event, 100);
                    const auto now = std::chrono::steady_clock::now();
                    int done = 0;
                    for (int i = 0; i < BUFFERS; ++i) if (m_header[i].dwFlags & WHDR_DONE) ++done;
                    const uint32_t gap = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count());
                    last = now;
                    if (done >= BUFFERS - 1) { // casi todos terminados a la vez = el dispositivo estuvo a punto de quedarse sin audio
                        m_late.fetch_add(1, std::memory_order_relaxed);
                        uint32_t seen = m_maxGap.load(std::memory_order_relaxed);
                        while (gap > seen && !m_maxGap.compare_exchange_weak(seen, gap, std::memory_order_relaxed)) {}
                    }
                    for (int i = 0; i < BUFFERS && m_running; ++i) if (m_header[i].dwFlags & WHDR_DONE) queue(i);
                }
            });
            return true;
        }

        void stop() {
            if (!m_running) return;
            m_running = false;
            SetEvent(m_event);
            if (m_thread.joinable()) m_thread.join();
            waveOutReset(m_out);
            for (int i = 0; i < BUFFERS; ++i) waveOutUnprepareHeader(m_out, &m_header[i], sizeof(WAVEHDR));
            waveOutClose(m_out);
            CloseHandle(m_event);
            timeEndPeriod(1);
            m_out = nullptr;
            m_event = nullptr;
        }

        private:
        void queue(int i) {
            m_fill(m_data[i].data(), FRAMES);
            m_header[i].dwFlags &= ~static_cast<DWORD>(WHDR_DONE);
            waveOutWrite(m_out, &m_header[i], sizeof(WAVEHDR));
        }

        HWAVEOUT m_out = nullptr;
        HANDLE m_event = nullptr;
        WAVEHDR m_header[BUFFERS] = {};
        std::vector<int16_t> m_data[BUFFERS];
#else
        bool start(int, Fill) { return false; }
        void stop() {}
        private:
#endif
        std::atomic<uint32_t> m_late{ 0 }, m_maxGap{ 0 };
        Fill m_fill;
        std::thread m_thread;
        std::atomic<bool> m_running{ false };
    };
}
