[← Volver al índice](../README.md)

# Motor de juego

Código en `app/engine/`: `core`, `physics`, `world` y `render`.

## GameEngine (`core/gameEngine.hpp`)

Fachada del motor. Contiene los datos (`GameData`), la escena (`Scene`) y el renderer (`Renderer3D`).

| Método | Función |
|---|---|
| `init(device)` | Inicializa el renderer |
| `setData(data)` | Recibe los datos de la base de datos y crea la escena |
| `newGame()` | Escena nueva |
| `applySave(save)` / `captureSave()` | Restaura / extrae la partida (posición, hora, inventario) |
| `update(dt, input)` | Avanza el ciclo de día y la escena; devuelve `true` si se pide pausar |
| `render(...)` | Dibuja la escena |
| `status()` | `GameStatus` para el HUD |

Otros archivos de `core`:

- `input.hpp`: `InputState`, acciones del frame independientes del dispositivo.
- `inputBindings.hpp`: asignación de botones del mando, umbrales y zonas muertas. Para añadir una acción: constante aquí + campo en `InputState` + mapeo en `InputHandler`.
- `inputDevice.hpp`: `InputDevice` (`KEYBOARD_MOUSE`, `XBOX`, `PLAYSTATION`).
- `gameStatus.hpp`: `GameStatus` y `Notice` (avisos en pantalla).

## Física (`physics/`)

- `Body`: posición, velocidad, `gravityScale`, `groundOffset`, `restitution`, `groundDrag`, radio y altura de colisión, `hitsCreatures`, `onGround`.
- `Physics::World`:
  - Gravedad 20; mundo de ±40 en X y Z; suelo plano (`groundHeight`, preparado para terreno generado).
  - Sólidos: `obstacles` (paredes), `creatures` (hitbox de Pokémon, se rehacen cada frame) y `props` (cofres y nodos).
  - `step(body, dt)`: gravedad, desplazamiento, límites, colisión horizontal con cajas (desliza o rebota), suelo, bote (`restitution`) y rodadura (`groundDrag`).
  - `jump(body, speed)`.

## Escena (`world/scene.hpp`)

Reúne todos los elementos del mundo y la lógica de cada frame:

- Jugador, cámara, ciclo de día, inventario, 4 `CaptureTarget`, Pokéballs lanzadas, cofres y nodos (`SpawnField`).
- **Modo captura:** `aimToggle` / `aimHold`, apuntado con rayo de la cámara, cálculo del porcentaje y lanzamiento.
- **Lanzamiento:** `launchVelocity` calcula la parábola para pasar por el punto apuntado (45° si está fuera de alcance; línea recta si está pegado). La física de las bolas va en subpasos de 1/120 s para que no atraviesen objetivos.
- **Fijado de cámara:** `cycleLock`, `validateLock`; se salta a otro objetivo si el fijado desaparece o sale de alcance.
- **Interacción:** `updateInteraction` elige el cofre o nodo más cercano al alcance; `interact` lo abre o lo golpea y entrega la recompensa.
- Genera el `GameStatus` (`status()`).

## Elementos del mundo (`world/`)

| Archivo | Contenido |
|---|---|
| `player.hpp` | Controlador del personaje: caminar, correr, agacharse, deslizarse, saltar con impulso, referencia de movimiento respecto a la cámara |
| `camera.hpp` | Cámara orbital con mezcla suave a cámara de apuntado y seguimiento de objetivo |
| `pokeball.hpp` | `Pokeball` (esfera con física propia) y `PokeballStyle::color` |
| `captureRules.hpp` | Bonificaciones, lanzamientos con suerte, `roll`, `percent`, `isBehind` |
| `captureTarget.hpp` | Objetivo (cubo): estados IDLE / CAPTURING / HIDDEN, hitbox, raycast, reaparición |
| `captureSequence.hpp` | Animación de captura: ABSORB → SETTLE → WOBBLE → CLICK / BREAK → DONE |
| `chest.hpp`, `chestRules.hpp`, `chestSpawn.hpp` | Cofre, azar (`pickType`, `pickReward`) y puntos de aparición |
| `resourceNode.hpp`, `resourceSpawn.hpp` | Nodo de recolección (golpes, sacudida, desvanecido) y puntos de aparición |
| `spawnField.hpp` | Aparición genérica en puntos fijos con temporizador de reaparición |
| `inventory.hpp` | Una ranura por objeto, lista de Pokéballs equipables, guardado y restauración por nombre |
| `dayCycle.hpp` | Ciclo día/noche: ángulo del sol, luz, ambiente, colores del cielo y estrellas |

Cómo añadir un tipo de entidad con aparición en puntos fijos: tener `spot()` y `finished()`, y usar `SpawnField<T>` con una función `make(spot)` (ver `ChestSpawn` y `ResourceSpawn`).

## Render 3D (`render/renderer3D.*`)

- **Lista única de dibujo:** cada frame, `collect()` describe todo el mundo como una lista de `Draw` (malla, matriz, tinte, si proyecta sombra). La misma lista se dibuja en el mapa de sombras y en pantalla, así todo proyecta y recibe sombras con la misma lógica.
- **Mallas generadas por código:** suelo en tablero, jugador (cápsula), cubo, esfera (Pokéball) y estrella extruida.
- **Sombras:** mapa de sombras de 3072×3072 con proyección ortográfica fija que cubre todo el mundo, desde la luz activa (sol o luna); filtrado 3×3 con comparación por hardware.
- **Cielo:** triángulo a pantalla completa con degradado, sol, luna, estrellas parpadeantes y tinte cálido de amanecer y atardecer.
- **Iluminación:** luz ambiente hemisférica + luz directa con sombra. El alfa del tinte es el brillo propio del objeto.
- **Shaders:** HLSL embebido en el propio `.cpp`, compilado al iniciar (`D3dUtil::compileShader`).
- **Orden por frame:** mapa de sombras → cielo → suelo y objetos.

Constantes de aspecto de cofres, nodos y Pokéballs: `ChestStyle`, `ResourceStyle`, `PokeballStyle` (único sitio donde se definen).

---

[← Volver al índice](../README.md) · Anterior: [Arquitectura](arquitectura.md) · Siguiente: [Interfaz (GUI)](interfaz.md)
