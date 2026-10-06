[← Volver al índice](../README.md)

# Referencia de archivos

## Raíz

| Archivo | Descripción |
|---|---|
| `main.cpp` | Punto de entrada: crea `AppManager` y lo arranca, registrando cualquier excepción |
| `README.md` | Entrada principal de la documentación y enlace de descarga |
| `release_config.json` | Configuración del release y de `version.json` |
| `dependencies.json` | Dependencias externas descargadas al compilar |
| `resources.rc` | Recursos incrustados (base de datos e icono) |
| `.gitignore` | Ignora `/testing`, `*.ini`, `*.exe`, `*.py` |
| `.github/workflows/build-release.yml` | Compilación y publicación del release |

## `app/managers`

| Archivo | Descripción |
|---|---|
| `appManager.hpp` | Secuencia de arranque de la aplicación |
| `updateManager.hpp` | Comprobación de versión y lanzamiento de la actualización |
| `databaseManager.hpp` | Instancia los DAO y carga los datos de juego (`GameData`) |
| `saveManager.hpp` | Ranuras de guardado: guardar, cargar, eliminar, caché de resúmenes |
| `sessionManager.hpp` | Partida en curso: une motor y guardado |
| `guiManager.hpp` | Inicia y ejecuta la interfaz |

## `app/models`

| Archivo | Descripción |
|---|---|
| `gameData.hpp` | Todos los datos de juego cargados de la base de datos |
| `item.hpp` | Objeto y categorías (`ItemCategory`) |
| `pokeballType.hpp` | Tipo de Pokéball |
| `material.hpp` | Material de recolección |
| `resourceNodeType.hpp` | Tipo de nodo de recolección |
| `chestType.hpp` | Tipo de cofre y sus recompensas |
| `pokemonType.hpp` | Tipo de Pokémon |
| `saveData.hpp` | Datos de una partida y resumen de ranura |

## `app/daos`

| Archivo | Descripción |
|---|---|
| `daoRow.hpp` | Lectura de columnas compartida (`text`, `integer`, `real`) |
| `pokeballDao.hpp`, `itemDao.hpp`, `materialDao.hpp`, `resourceNodeDao.hpp`, `chestDao.hpp`, `typeDao.hpp` | Consultas SQL de cada tabla. Ver [Base de datos](base-de-datos.md) |

## `app/data`

| Archivo | Descripción |
|---|---|
| `db/pokemonEdge.db` | Base de datos SQLite de desarrollo (se incrusta cifrada en el `.exe`) |
| `icon.ico` | Icono de la aplicación (incrustado) |
| `logo.png` | Logo de la pantalla de título |

## `app/engine`

| Archivo | Descripción |
|---|---|
| `core/gameEngine.hpp` | Fachada del motor |
| `core/gameStatus.hpp` | Estado que la interfaz necesita mostrar y avisos |
| `core/input.hpp` | Acciones del jugador por frame |
| `core/inputBindings.hpp` | Asignación del mando |
| `core/inputDevice.hpp` | Dispositivo activo |
| `physics/body.hpp` | Estado físico de una entidad |
| `physics/physicsWorld.hpp` | Reglas físicas y colisiones |
| `render/renderer3D.hpp` / `.cpp` | Render 3D: mallas, shaders, sombras, cielo |
| `world/scene.hpp` | Escena: lógica de cada frame |
| `world/player.hpp` | Controlador del personaje |
| `world/camera.hpp` | Cámara orbital y de apuntado |
| `world/pokeball.hpp` | Pokéball lanzada y su aspecto |
| `world/captureRules.hpp` | Reglas del porcentaje y azar de captura |
| `world/captureTarget.hpp` | Pokémon objetivo |
| `world/captureSequence.hpp` | Animación de captura |
| `world/chest.hpp`, `chestRules.hpp`, `chestSpawn.hpp` | Cofres |
| `world/resourceNode.hpp`, `resourceSpawn.hpp` | Nodos de recolección |
| `world/spawnField.hpp` | Aparición en puntos fijos |
| `world/inventory.hpp` | Inventario |
| `world/dayCycle.hpp` | Ciclo de día y noche |

Ver [Motor de juego](motor.md).

## `app/gui`

| Archivo | Descripción |
|---|---|
| `gameState.hpp` | Estados de la interfaz y sus propiedades |
| `window/mainWindow.hpp` / `.cpp` | Ventana, bucle principal y estados |
| `window/graphicsDevice.hpp` / `.cpp` | Dispositivo D3D11 y texturas |
| `window/inputHandler.hpp` | Entrada en juego (teclado, ratón, mandos) |
| `window/guiInput.hpp` | Mando en los menús |
| `components/*.hpp` | Pantallas: título, menú, ranuras, carga, HUD, pausa, controles |
| `style/*.hpp` | Paleta, maquetación, dibujo y ayudas de controles |

Ver [Interfaz (GUI)](interfaz.md).

## `app/utils`

| Carpeta | Archivo | Descripción |
|---|---|---|
| `core` | `fileUtil.hpp` | Operaciones de archivos (sin registrar errores) |
| | `loggerUtil.hpp` | Log a consola y a `app/logs/app.log` |
| | `pathsUtil.hpp` | Única definición de rutas, URLs y programas externos |
| | `processUtil.hpp` | Lanzar un programa oculto |
| | `randomUtil.hpp` | Aleatorios y elección por pesos |
| | `resourceIds.h` / `resourceUtil.hpp` | IDs y carga de recursos incrustados |
| | `stringUtil.hpp` | Formato sin reservas, comillas y unión para PowerShell |
| | `timeUtil.hpp` | Fecha formateada y cronómetro de frames |
| `data` | `cipherUtil.hpp` | XOR simétrico |
| | `databaseUtil.hpp` | Extracción y descifrado de la base de datos |
| | `jsonUtil.hpp` | JSON sin excepciones |
| | `sqliteUtil.hpp` | Consultas SELECT con conexión única |
| `graphics` | `d3dUtil.hpp` | Comprobación de HRESULT, buffers y compilación de shaders |
| | `imageUtil.hpp` / `.cpp` | Decodificación de imágenes |
| | `windowUtil.hpp` | Ventana, espera pasiva y pantalla completa |
| `input` | `cursorUtil.hpp` | Ocultar y bloquear el cursor |
| | `gamepadState.hpp` | Estado normalizado de un mando y zona muerta |
| | `rawInputUtil.hpp` | Raw Input de Windows |
| | `xinputUtil.hpp` | Mandos XInput |
| | `hidGamepadUtil.hpp` | Mandos PlayStation por HID |
| `network` | `httpUtil.hpp` | Peticiones GET con WinHTTP |
| | `updateUtil.hpp` | Versiones y script de actualización |

## Carpetas no versionadas

| Ruta | Contenido |
|---|---|
| `testing/sql/` | Scripts SQL para generar la base de datos (ignorada por Git) |
| `app/utils/imgui`, `app/utils/nlohmann`, `app/utils/sqlite` | Dependencias descargadas al compilar |

---

[← Volver al índice](../README.md) · Anterior: [Compilación y releases](compilacion.md)
