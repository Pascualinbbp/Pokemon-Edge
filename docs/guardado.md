[← Volver al índice](../README.md)

# Guardado de partidas

## Ranuras

- Hay **4 ranuras** de partida. Cada una es un archivo `app/saves/slotN.json` (`slot1.json` … `slot4.json`).
- **Nueva partida:** ocupa la primera ranura libre desde el primer momento. Si las 4 están ocupadas, el juego pide elegir una para **eliminarla** (con confirmación) y empezar la nueva.
- **Cargar partida:** lista las ranuras ocupadas con su fecha y hora de guardado.

## Cuándo se guarda

No hay botón de guardar. La partida se guarda sola:

| Momento | Detalle |
|---|---|
| Autoguardado periódico | Cada 30 s de juego; el primero a los 10 s de entrar a la partida |
| Al salir al menú | Desde la pausa → MENU PRINCIPAL |
| Al cerrar el juego | Si se cierra en plena partida |

Cada autoguardado muestra el aviso `Guardando...` durante unos segundos.

## Qué se guarda

| Dato | Descripción |
|---|---|
| Posición del jugador | X, Y, Z |
| Hora del mundo | Segundos dentro del ciclo de día y noche |
| Inventario | Unidades de cada objeto (por nombre) |
| Pokéball equipada | Por nombre |

**No se guarda** (de momento): contador de capturas de la sesión, estado de los Pokémon objetivo, cofres y nodos del mundo.

## Formato del archivo

```json
{
    "version": 1,
    "savedAt": 1791234567,
    "player": { "position": [0.0, 0.0, 0.0] },
    "world": { "time": 25.0 },
    "inventory": {
        "items": { "Poké Ball": 10, "Madera": 6 },
        "selected": "Poké Ball"
    }
}
```

- `savedAt` es una marca de tiempo Unix; se muestra como `dd/mm/aaaa hh:mm`.
- Los objetos que no aparecen en el guardado conservan sus unidades iniciales.
- Un `world.time` ausente (partidas antiguas) hace que se use la hora de inicio.

## Casos especiales

- **Archivo dañado:** la ranura sigue contando como ocupada, pero sin fecha. Al cargarla se empieza una partida nueva en esa ranura.
- **Actualizaciones:** la carpeta `app/saves` se conserva al actualizar el juego. Ver [Actualizaciones](actualizaciones.md).
- **Copia de seguridad:** copia la carpeta `app/saves`.

## Código relacionado

`SaveManager` (ranuras y disco), `SessionManager` (partida en curso), `SaveData` (estructura), `JsonUtil` (lectura/escritura), `PathsUtil::saveSlotPath`. Ver [Referencia de archivos](referencia-archivos.md).

---

[← Volver al índice](../README.md) · Anterior: [Jugabilidad](jugabilidad.md) · Siguiente: [Actualizaciones](actualizaciones.md)
