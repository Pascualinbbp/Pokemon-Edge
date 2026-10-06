[← Volver al índice](../README.md)

# Instalación

## Descarga

> **[⬇ Descargar la última versión (PokemonEdge.zip)](https://github.com/Pascualinbbp/Pokemon-Edge/releases/latest/download/PokemonEdge.zip)**

El enlace apunta siempre al ZIP del *latest release*, así que descarga la versión más reciente publicada. El resto de versiones están en [Releases](https://github.com/Pascualinbbp/Pokemon-Edge/releases).

## Requisitos

| Requisito | Detalle |
|---|---|
| Sistema operativo | Windows 10 u 11, 64 bits |
| Gráficos | GPU compatible con DirectX 11 (feature level 11.0). Si no hay GPU compatible, el juego usa el renderizador por software WARP (más lento) |
| Red | Opcional. Solo se usa al arrancar para comprobar si hay una versión nueva (con tiempos de espera cortos: sin red el juego arranca igualmente) |
| PowerShell | Incluido en Windows. Lo usa el actualizador |
| Mando | Opcional. Xbox (XInput) o PlayStation (DualShock 4 / DualSense) |

No hace falta instalar nada más: la base de datos y el icono van incrustados en el ejecutable.

## Pasos

1. Descarga `PokemonEdge.zip`.
2. Extrae todo el contenido en una **carpeta propia con permisos de escritura** (por ejemplo `Documentos\Pokemon Edge`). Evita `Archivos de programa`.
3. Ejecuta `PokemonEdge.exe`.

## Contenido del ZIP

```
PokemonEdge.zip
├── PokemonEdge.exe
└── app/
    └── data/
        ├── version.json    # versión instalada
        └── logo.png        # logo de la pantalla de título (opcional)
```

## Carpetas que crea el juego

Tras el primer arranque, junto al ejecutable aparecen:

```
app/
├── data/db/pokemonEdge.db   # base de datos descifrada (se extrae del .exe si no existe)
├── logs/app.log             # registro de la aplicación
└── saves/slot1.json ...     # partidas guardadas (hasta 4)
```

## Importante: no guardes otros archivos en la carpeta del juego

Cuando se actualiza, el actualizador **borra todo el contenido de la carpeta del juego** salvo `temp_download` y, dentro de `app`, las carpetas `logs` y `saves`. Cualquier otro archivo colocado en esa carpeta se perderá. Ver [Actualizaciones](actualizaciones.md).

## Desinstalación

Borra la carpeta del juego. Si quieres conservar tus partidas, copia antes `app/saves`.

## Problemas frecuentes

| Síntoma | Causa probable |
|---|---|
| No guarda ni crea carpetas | La carpeta no tiene permisos de escritura. Muévela fuera de `Archivos de programa` |
| El juego se cierra y vuelve a abrirse al arrancar | Se ha detectado una versión nueva y se está actualizando |
| Pantalla de título sin logo | Falta `app/data/logo.png`; es opcional y solo se registra un aviso en el log |
| El mando no responde | Reconéctalo; el juego detecta cambios de dispositivos automáticamente |
| Cualquier otro fallo | Revisa `app/logs/app.log` |

---

[← Volver al índice](../README.md) · Siguiente: [Controles](controles.md)
