#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "wav.hpp"

namespace Audio {
    // Lo que suena en una emisora: audio estéreo de 16 bits que se va leyendo poco a poco (streaming), así una emisora puede durar horas sin
    // ocupar memoria descomprimida. Al llegar al final vuelve al principio. Las posiciones son en fotogramas (un fotograma = izquierda + derecha).
    class RadioSource {
        public:
        virtual ~RadioSource() = default;
        virtual int rate() const = 0;
        virtual size_t length() const = 0;                         // fotogramas totales
        virtual size_t position() const = 0;                       // fotograma que se leerá a continuación
        virtual void seek(size_t frame) = 0;
        virtual size_t read(int16_t* stereo, size_t frames) = 0;   // rellena 'frames' fotogramas (en bucle); devuelve cuántos
    };
    using RadioPtr = std::shared_ptr<RadioSource>;

    // Un clip en memoria (WAV) como emisora.
    class ClipSource : public RadioSource {
        public:
        explicit ClipSource(ClipPtr clip) : m_clip(std::move(clip)) {}
        int rate() const override { return m_clip->rate; }
        size_t length() const override { return m_clip->samples.size(); }
        size_t position() const override { return m_pos; }
        void seek(size_t frame) override { m_pos = length() ? frame % length() : 0; }
        size_t read(int16_t* stereo, size_t frames) override {
            if (m_clip->samples.empty()) return 0;
            for (size_t i = 0; i < frames; ++i) {
                const int16_t s = m_clip->samples[m_pos];
                stereo[i * 2] = stereo[i * 2 + 1] = s;
                if (++m_pos >= m_clip->samples.size()) m_pos = 0;
            }
            return frames;
        }
        private:
        ClipPtr m_clip;
        size_t m_pos = 0;
    };
}
