#pragma once
#include <atomic>
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
        static constexpr int BUFFERS = 4;
        static constexpr int FRAMES = 512; // ~23 ms por búfer a 22 050 Hz: poca latencia sin cortes

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
            m_thread = std::thread([this] {
                while (m_running) {
                    WaitForSingleObject(m_event, 100);
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
        Fill m_fill;
        std::thread m_thread;
        std::atomic<bool> m_running{ false };
    };
}
