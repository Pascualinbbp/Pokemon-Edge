#pragma once
#include <cstddef>
#include <mutex>
#include <vector>
#include <DirectXMath.h>

// Catálogo de efectos de sonido y cola de eventos. Es el ÚNICO sitio donde se registra un sonido nuevo: una entrada en 'Sfx' y su fila en 'info()'.
// El sonido pertenece al objeto, no a quien lo usa: coger un coco suena igual lo haga el jugador, su acompañante o un salvaje, porque quien
// lo emite es el propio fruto (Scene::collectFruit / eatFruit), no cada personaje. Las entidades llaman a Sound::emit y nada más; el motor
// de audio (AudioEngine) vacía la cola cada fotograma, descarta lo que queda fuera de alcance y carga el fichero solo cuando suena por primera vez.
namespace Sound {
    enum class Sfx {
        THROW,          // el jugador lanza la pokéball
        BALL_HIT,       // la pokéball toca el suelo o golpea a un pokémon
        BALL_SPLASH,    // la pokéball cae al agua
        BALL_OPEN,      // se abre y absorbe al pokémon
        BALL_CLOSE,     // se cierra con él dentro
        BALL_LAND,      // cae al suelo ya cerrada
        BALL_WOBBLE,    // cada balanceo
        CAPTURE_CLICK,  // clic de captura conseguida
        CAPTURE_BREAK,  // el pokémon se libera
        BALL_RELEASE,   // sale el pokémon acompañante
        BALL_RECALL,    // vuelve a su bola
        EVOLVE,         // evolución
        CHOP,           // talar
        MINE,           // picar
        PICK,           // recoger (plantas, frutos, objetos del suelo)
        CHEST_OPEN,     // abrir un cofre
        EAT,            // comer
        FRUIT_DROP,     // un fruto cae al suelo
        RADIO_TUNE,     // cambio de emisora (interferencia)
        COUNT
    };

    struct Info {
        const char* path;     // dentro de assets/
        float volume;         // volumen base (0..1)
        float maxDistance;    // más lejos que esto no se oye (ni se carga)
        float pitchJitter;    // variación aleatoria de tono (±fracción), para que no suene siempre igual
    };

    inline const Info& info(Sfx sfx) {
        static const Info table[static_cast<size_t>(Sfx::COUNT)] = {
            { "audio/sfx/throw.wav",         0.70f, 35.0f, 0.04f },
            { "audio/sfx/ball_hit.wav",      0.70f, 30.0f, 0.08f },
            { "audio/sfx/splash.wav",        0.80f, 40.0f, 0.08f },
            { "audio/sfx/ball_open.wav",     0.85f, 45.0f, 0.00f },
            { "audio/sfx/ball_close.wav",    0.80f, 45.0f, 0.00f },
            { "audio/sfx/ball_land.wav",     0.75f, 35.0f, 0.04f },
            { "audio/sfx/wobble.wav",        0.80f, 40.0f, 0.03f },
            { "audio/sfx/capture_click.wav", 1.00f, 55.0f, 0.00f },
            { "audio/sfx/capture_break.wav", 0.90f, 50.0f, 0.00f },
            { "audio/sfx/ball_release.wav",  0.85f, 45.0f, 0.00f },
            { "audio/sfx/ball_recall.wav",   0.85f, 45.0f, 0.00f },
            { "audio/sfx/evolve.wav",        1.00f, 70.0f, 0.00f },
            { "audio/sfx/chop.wav",          0.85f, 45.0f, 0.06f },
            { "audio/sfx/mine.wav",          0.85f, 45.0f, 0.06f },
            { "audio/sfx/pick.wav",          0.70f, 28.0f, 0.06f },
            { "audio/sfx/chest.wav",         0.90f, 40.0f, 0.00f },
            { "audio/sfx/eat.wav",           0.75f, 25.0f, 0.06f },
            { "audio/sfx/fruit_drop.wav",    0.65f, 30.0f, 0.10f },
            { "audio/sfx/radio_tune.wav",    0.60f, 0.0f,  0.00f },
        };
        return table[static_cast<size_t>(sfx)];
    }

    // Un sonido pedido: dónde suena (o 'global' = sin posición, sobre el jugador) y cuánto.
    struct Event {
        Sfx sfx;
        DirectX::XMFLOAT3 at;
        float volume;
        float pitch;
        bool global;
    };

    namespace detail {
        inline std::mutex& lock() { static std::mutex m; return m; }
        inline std::vector<Event>& queue() { static std::vector<Event> q; return q; }
        inline constexpr size_t MAX_QUEUED = 96; // sin motor de audio (pruebas) la cola no crece sin fin
    }

    inline void push(const Event& event) {
        std::lock_guard<std::mutex> guard(detail::lock());
        auto& q = detail::queue();
        if (q.size() >= detail::MAX_QUEUED) q.erase(q.begin());
        q.push_back(event);
    }

    // Suena en 'at' (se oye más fuerte cuanto más cerca esté el jugador).
    inline void emit(Sfx sfx, const DirectX::XMFLOAT3& at, float volume = 1.0f, float pitch = 1.0f) { push({ sfx, at, volume, pitch, false }); }
    // Suena sobre el jugador (interfaz, radio).
    inline void emitGlobal(Sfx sfx, float volume = 1.0f) { push({ sfx, {}, volume, 1.0f, true }); }

    // Entrega y vacía lo pedido desde la última vez.
    inline void drain(std::vector<Event>& out) {
        std::lock_guard<std::mutex> guard(detail::lock());
        out.swap(detail::queue());
        detail::queue().clear();
    }
}
