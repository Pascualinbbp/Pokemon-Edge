# Combate por turnos y movimientos

- **Datos:** `move`, `move_class`, `move_tag`, `learn_tag` (ver base-de-datos.md). Los efectos viven en `effect` (columnas `kind`, `param`, `target`, `amount`, `chance`, `multiplier`): STAT_STAGE, STATUS, HEAL_SELF, RECOIL, DRAIN, PROTECT, CRIT_BOOST, FLINCH, ACCURACY_BOOST. Las habilidades añaden `trigger` (PASSIVE, ON_ENTER, ON_CONTACT, END_TURN) y `priority`.
- **Aprendizaje (`MoveRules`):** un movimiento al crearse el pokémon (siempre de daño) y otro cada 2 niveles. Al azar según `learn_weight`, solo de sus tipos o de sus etiquetas de aprendizaje. Lo intentado queda *desbloqueado* y no vuelve a salir. Máximo 4 equipados; si no caben, `LearnComponent` pregunta (olvidar uno / no aprenderlo). Se editan desde Pokémon > Movimientos.
- **Daño (`BattleRules`):** fórmula de la serie con tipo (tabla de tipos), STAB, crítico (1/16, +1 nivel por movimiento, habilidad u objeto), niveles de estadística, quemadura, Vidasfera, precisión/evasión, variación 85-100 %.
- **Orden:** cambio de pokémon > prioridad (etiquetas) > Garra rápida > velocidad (parálisis x0,5) > azar.
- **Combate (`Battle`):** 1 contra 1 con cambios; sin PP usa Forcejeo; el rival elige por daño esperado. Los mensajes del turno se reproducen uno a uno (`BattleComponent`).
- **Entrenadores de prueba** (junto al inicio): Rojo = duelo en tiempo real (su pokémon sale al mundo, como un salvaje); Azul = combate por turnos (carga, cámara tras tu pokémon, sin movimiento).

## Zona de combate por turnos
Círculo en el suelo (`Scene::ARENA_*`, centro al sur de los entrenadores). Al empezar, durante la pantalla de carga, el entrenador y el jugador caminan hasta puntos opuestos del borde (por fuera) y los pokémon pelean en medio; al terminar vuelven a donde estaban. Sale siempre el primer pokémon del equipo no debilitado. El cuadro de cada combatiente muestra tipos, estado y cambios de estadística (ATQ +2...); al subir/bajar una estadística o activarse una habilidad aparece un aviso flotante y aros luminosos en 3D. Los movimientos muestran su eficacia contra el rival (supereficaz, hipereficaz, poco eficaz, muy poco eficaz, no afecta).

## Tienda
El Tendero (junto al inicio) abre la tienda: vende los objetos del apartado «Pokémon» con `buy_price > 0` (un precio 0 los excluye: Honor Ball, Starter Ball, Shiny Ball) y compra lo que tenga `sell_price > 0`. Regalos por compra: `item.gift_every` / `gift_reward_id`.

## Coco
`assets/models/fruits/coconut.glb` (nodos `mitad_a` y `mitad_b`). Un coco puede partirse al caer del árbol (12 %) o cuando un pokémon se lo come; cada mitad es un cuerpo físico propio y desaparece sola o al comerla. Sonido: `assets/audio/sfx/coconut_break.wav`.
