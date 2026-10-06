[← Volver al índice](../README.md)

# Compilación y releases

La compilación y la publicación se hacen automáticamente con **GitHub Actions** en cada push a la rama `main` (`.github/workflows/build-release.yml`, runner `windows-latest`).

## Archivos de configuración

| Archivo | Función |
|---|---|
| `release_config.json` | Datos del release (`configs`) y del `version.json` que se distribuye (`userjson`) |
| `dependencies.json` | Dependencias externas que se descargan al compilar |
| `resources.rc` | Recursos incrustados: base de datos e icono |
| `app/utils/core/resourceIds.h` | IDs de los recursos (`IDR_DATABASE`, `IDI_ICON1`). Debe terminar en salto de línea (si no, error RC1004 de `rc.exe`) |

### `release_config.json`

```json
{
  "configs": {
    "tag": "v0.0.2",
    "name": "Pokemon Edge v0.0.2",
    "description": "…",
    "prerelease": false,
    "make_latest": true,
    "branch": "main"
  },
  "userjson": {
    "name": "Pokemon Edge",
    "version": "0.0.2",
    "releaseDate": "2026-09-04",
    "author": "Pascualinbbp",
    "repository": "https://github.com/Pascualinbbp/Pokemon-Edge"
  }
}
```

### `dependencies.json`

| Sección | Dependencia | Destino |
|---|---|---|
| `files` | nlohmann/json 3.11.3 (`json.hpp`) | `app/utils/nlohmann/` |
| `files` | `stb_image.h` | `app/utils/imgui/` |
| `zips` | SQLite amalgamation 3.45.1 | `app/utils/sqlite/` |
| `gits` | Dear ImGui (cabeceras, fuentes, backends Win32 y DX11) | `app/utils/imgui/` |

Estas dependencias **no se guardan en el repositorio**: se descargan en cada compilación.

## Pasos del workflow

1. **Checkout** del código.
2. **Configuración del release:** lee `release_config.json`, exporta las variables del release y genera `version.json` (en `app/data/` y en la raíz).
3. **Dependencias:** descarga archivos, ZIPs y repositorios de `dependencies.json`.
4. **Cifrado de la base de datos:** XOR con la clave del secret `DB_ENCRYPTION_KEY` (por defecto `90`).
5. **Compilación:** con Visual Studio (`vcvars64.bat`): compila `resources.rc` y todos los `.cpp` de `app` más `main.cpp`, con `/std:c++17 /EHsc /O2 /MP`, `XOR_KEY_VAL` definido y subsistema Windows (sin consola). Librerías: `winhttp`, `shell32`, `user32`, `d3d11`, `dxgi`, `d3dcompiler`.
6. **ZIP final:** `PokemonEdge.exe`, `app/data/version.json` y `app/data/logo.png` (si existe). El icono y la base de datos van dentro del `.exe`.
7. **Publicación:** crea el release con `PokemonEdge.zip` y `version.json` (acción `softprops/action-gh-release`).

## Publicar una versión nueva

1. Edita `release_config.json`: sube `tag`, `name`, `description`, `version` y `releaseDate`.
2. Haz commit y push a `main`.
3. El workflow compila y publica el release. Al ser *latest release*, el enlace de descarga del [README](../README.md) y el actualizador del juego apuntan automáticamente a la nueva versión.

> Si publicas con el mismo `tag`, el release se sobrescribe (`overwrite: true`).

## Secret necesario (opcional)

| Secret | Uso |
|---|---|
| `DB_ENCRYPTION_KEY` | Número de 0 a 255 usado como clave XOR de la base de datos. Si no se define, se usa 90 |

## Compilar en local

No hay proyecto de Visual Studio: la compilación se hace por línea de comandos reproduciendo el workflow.

1. Instala Visual Studio con las herramientas de C++.
2. Descarga manualmente las dependencias de `dependencies.json` en las rutas indicadas.
3. Abre el *x64 Native Tools Command Prompt* y, desde la raíz del repositorio, ejecuta los mismos comandos `rc.exe` y `cl.exe` del paso 5 del workflow. Para pruebas sin definir `XOR_KEY_VAL` se usa 90 por defecto, pero la base de datos incrustada debe estar cifrada con esa misma clave.

---

[← Volver al índice](../README.md) · Anterior: [Interfaz (GUI)](interfaz.md) · Siguiente: [Referencia de archivos](referencia-archivos.md)
