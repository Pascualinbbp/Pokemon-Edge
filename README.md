# Pokemon Edge

Juego Pokémon en 3D para Windows, escrito en C++17 con un motor propio sobre DirectX 11 e interfaz Dear ImGui.
Exploras un mundo con ciclo de día y noche, apuntas y lanzas Pokéballs para capturar, abres cofres y recolectas materiales. La partida se guarda automáticamente.

- **Versión actual:** 0.0.2
- **Autor:** [Pascualinbbp](https://github.com/Pascualinbbp)
- **Repositorio:** https://github.com/Pascualinbbp/Pokemon-Edge
- **Plataforma:** Windows 10/11 (64 bits)

---

## Descarga

> **[⬇ Descargar la última versión (PokemonEdge.zip)](https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/PokemonEdge.zip)**

Este enlace descarga directamente el ZIP del *latest release*. Todas las versiones están en la [página de Releases](https://github.com/Pascualinbbp/Pokemon-Edge/releases).

**Instalación rápida**

1. Descarga `PokemonEdge.zip` con el enlace de arriba.
2. Extrae el contenido en una carpeta con permisos de escritura (por ejemplo, `Documentos\Pokemon Edge`).
3. Ejecuta `PokemonEdge.exe`.

Más detalles en [Instalación](docs/instalacion.md).

---

## Índice de la documentación

| Apartado | Contenido |
|---|---|
| [Instalación](docs/instalacion.md) | Requisitos, descarga, primer arranque, carpetas y desinstalación |
| [Controles](docs/controles.md) | Teclado y ratón, mando de Xbox y de PlayStation |
| [Jugabilidad](docs/jugabilidad.md) | Captura, cofres, recursos, inventario, día y noche, HUD |
| [Guardado de partidas](docs/guardado.md) | Ranuras, autoguardado y formato del archivo |
| [Actualizaciones](docs/actualizaciones.md) | Cómo se actualiza el juego automáticamente |
| [Base de datos](docs/base-de-datos.md) | Tablas, scripts SQL, datos iniciales y cifrado |
| [Arquitectura](docs/arquitectura.md) | Visión general, capas, flujo de arranque y bucle principal |
| [Motor de juego](docs/motor.md) | Física, escena, cámara, jugador, captura y render 3D |
| [Interfaz (GUI)](docs/interfaz.md) | Estados, componentes, entrada y estilo |
| [Compilación y releases](docs/compilacion.md) | Dependencias, GitHub Actions y publicación de versiones |
| [Referencia de archivos](docs/referencia-archivos.md) | Qué hace cada archivo del proyecto |

---

## Estructura del repositorio

```
Pokemon Edge
├── .github/workflows/build-release.yml   # CI: compila y publica el release
├── app/
│   ├── daos/        # Acceso a la base de datos
│   ├── data/        # Base de datos, icono y logo
│   ├── engine/      # Motor: core, physics, render, world
│   ├── gui/         # Interfaz: components, style, window
│   ├── managers/    # Coordinación de la aplicación
│   ├── models/      # Estructuras de datos
│   └── utils/       # Utilidades: core, data, graphics, input, network
├── docs/            # Esta documentación
├── dependencies.json
├── release_config.json
├── resources.rc
├── main.cpp
└── README.md
```

Para la descripción completa consulta [Arquitectura](docs/arquitectura.md) y [Referencia de archivos](docs/referencia-archivos.md).
