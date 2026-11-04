#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace Audio {
    // Sonido cargado en memoria: muestras de 16 bits mono.
    struct Clip {
        std::vector<int16_t> samples;
        int rate = 22050;
    };
    using ClipPtr = std::shared_ptr<const Clip>;

    namespace Wav {
        inline uint32_t u32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
        inline uint16_t u16(const unsigned char* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

        // WAV PCM de 16 bits (mono o estéreo, que se mezcla a mono). Devuelve nullptr si no es válido.
        inline ClipPtr parse(const unsigned char* data, size_t size) {
            if (!data || size < 44 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) return nullptr;
            int channels = 0, rate = 0, bits = 0;
            const unsigned char* pcm = nullptr;
            size_t pcmSize = 0;
            for (size_t at = 12; at + 8 <= size;) {
                const uint32_t chunk = u32(data + at + 4);
                const unsigned char* body = data + at + 8;
                if (std::memcmp(data + at, "fmt ", 4) == 0 && at + 8 + 16 <= size) {
                    if (u16(body) != 1) return nullptr; // solo PCM sin comprimir
                    channels = u16(body + 2);
                    rate = static_cast<int>(u32(body + 4));
                    bits = u16(body + 14);
                } else if (std::memcmp(data + at, "data", 4) == 0) {
                    pcm = body;
                    pcmSize = (std::min)(static_cast<size_t>(chunk), size - (at + 8));
                    break;
                }
                at += 8 + chunk + (chunk & 1);
            }
            if (!pcm || bits != 16 || (channels != 1 && channels != 2) || rate <= 0) return nullptr;
            auto clip = std::make_shared<Clip>();
            clip->rate = rate;
            const size_t frames = pcmSize / (2 * static_cast<size_t>(channels));
            clip->samples.resize(frames);
            for (size_t i = 0; i < frames; ++i) {
                const int l = static_cast<int16_t>(u16(pcm + i * 2 * channels));
                const int r = channels == 2 ? static_cast<int16_t>(u16(pcm + i * 4 + 2)) : l;
                clip->samples[i] = static_cast<int16_t>((l + r) / 2);
            }
            return clip;
        }
    }
}
