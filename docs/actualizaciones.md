[← Volver al índice](../README.md)

# Actualizaciones

El juego se actualiza solo al arrancar, a partir del *latest release* de GitHub.

## Funcionamiento

1. Al iniciar (`UpdateManager::checkAndHandleUpdate`), se lee la versión local de `app/data/version.json` (si no existe, se asume `0.0.0`).
2. Se descarga la versión remota:
   `https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/version.json`
3. Si no se puede obtener la versión remota (sin red, error HTTP…), se registra el error y el juego **arranca normalmente**.
4. Si ambas versiones son **iguales**, no se hace nada.
5. Si **difieren**, se lanza un script de PowerShell oculto y el juego se cierra.

## Script de actualización

El script (generado en `UpdateUtil::buildScript`) hace, en orden:

1. Crea la carpeta temporal `temp_download`.
2. Descarga `https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/PokemonEdge.zip`.
3. Lo extrae en `temp_download` y borra el ZIP.
4. Espera a que el proceso del juego termine.
5. Borra el contenido de la carpeta del juego, **excepto**:
   - `temp_download`
   - dentro de `app`: las carpetas `logs` y `saves`
6. Copia el contenido descargado a la carpeta del juego.
7. Borra `temp_download`.
8. Vuelve a lanzar `PokemonEdge.exe`.

> Todo lo que no sea `app/logs` ni `app/saves` se elimina en cada actualización, incluida la base de datos extraída (`app/data/db`), que se vuelve a generar desde el nuevo ejecutable.

## Versión

- `version.json` se genera en el workflow a partir de `release_config.json` (sección `userjson`) y se incluye en el ZIP y como archivo del release.
- La comparación es de **texto exacto**: cualquier diferencia entre la versión local y la remota (incluso una anterior) dispara la actualización.

```json
{
    "name": "Pokemon Edge",
    "version": "0.0.2",
    "releaseDate": "2026-09-04",
    "author": "Pascualinbbp",
    "repository": "https://github.com/Pascualinbbp/Pokemon-Edge"
}
```

## Detalles técnicos

- Las peticiones HTTP usan WinHTTP con redirecciones automáticas (GitHub Releases redirige las descargas) y tiempos de espera de 5 s (resolver y conectar) y 10 s (enviar y recibir).
- Las rutas y URLs viven únicamente en `app/utils/core/pathsUtil.hpp`.
- Las carpetas de datos de usuario protegidas se definen en `PathsUtil::USER_DATA_DIRS`. Si añades una carpeta de datos nueva, añádela ahí.

## Publicar una versión nueva

Ver [Compilación y releases](compilacion.md).

---

[← Volver al índice](../README.md) · Anterior: [Guardado de partidas](guardado.md) · Siguiente: [Base de datos](base-de-datos.md)
