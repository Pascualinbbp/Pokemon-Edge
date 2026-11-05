#pragma once
#include <memory>
#include <vector>
#include "radioSource.hpp"
#define MINIMP3_NO_STDIO
#include "../../utils/minimp3/minimp3_ex.h"

namespace Audio {
    // Emisora en MP3: se descodifica sobre la marcha (minimp3), sin pasar todo a PCM. El MP3 puede durar horas; el índice para saltar a un
    // punto se construye al abrirlo. 'data' debe seguir vivo mientras exista la fuente (o se entrega en 'keep').
    class Mp3Source : public RadioSource {
        public:
        static std::shared_ptr<Mp3Source> open(const uint8_t* data, size_t size, std::shared_ptr<const void> keep = nullptr) {
            auto source = std::shared_ptr<Mp3Source>(new Mp3Source());
            source->m_keep = std::move(keep);
            if (!data || mp3dec_ex_open_buf(&source->m_dec, data, size, MP3D_SEEK_TO_SAMPLE) != 0 || source->m_dec.info.channels < 1 || source->m_dec.samples == 0) return nullptr;
            source->m_open = true;
            source->m_channels = source->m_dec.info.channels;
            return source;
        }
        ~Mp3Source() override { if (m_open) mp3dec_ex_close(&m_dec); }
        int rate() const override { return m_dec.info.hz; }
        size_t length() const override { return static_cast<size_t>(m_dec.samples) / static_cast<size_t>(m_channels); }
        size_t position() const override { return static_cast<size_t>(m_dec.cur_sample) / static_cast<size_t>(m_channels); }
        void seek(size_t frame) override {
            const size_t total = length();
            mp3dec_ex_seek(&m_dec, static_cast<uint64_t>(total ? frame % total : 0) * static_cast<uint64_t>(m_channels));
        }
        size_t read(int16_t* stereo, size_t frames) override {
            size_t done = 0;
            int empty = 0;
            m_tmp.resize(frames * static_cast<size_t>(m_channels));
            while (done < frames && empty < 2) {
                const size_t got = mp3dec_ex_read(&m_dec, m_tmp.data(), (frames - done) * static_cast<size_t>(m_channels)) / static_cast<size_t>(m_channels);
                if (got == 0) { mp3dec_ex_seek(&m_dec, 0); ++empty; continue; } // fin: vuelve al principio
                empty = 0;
                for (size_t i = 0; i < got; ++i) {
                    const int16_t l = m_tmp[i * static_cast<size_t>(m_channels)];
                    const int16_t r = m_channels > 1 ? m_tmp[i * static_cast<size_t>(m_channels) + 1] : l;
                    stereo[(done + i) * 2] = l;
                    stereo[(done + i) * 2 + 1] = r;
                }
                done += got;
            }
            return done;
        }
        private:
        Mp3Source() = default;
        mp3dec_ex_t m_dec = {};
        bool m_open = false;
        int m_channels = 2;
        std::shared_ptr<const void> m_keep;
        std::vector<int16_t> m_tmp;
    };
}
