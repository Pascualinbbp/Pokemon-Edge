# Combate por turnos y movimientos

- **Datos:** `move`, `move_class`, `move_tag`, `learn_tag` (ver base-de-datos.md). Los efectos viven en `effect` (columnas `kind`, `param`, `target`, `amount`, `chance`, `multiplier`): STAT_STAGE, STATUS, HEAL_SELF, RECOIL, DRAIN, PROTECT, CRIT_BOOST, FLINCH, ACCURACY_BOOST. Las habilidades añaden `trigger` (PASSIVE, ON_ENTER, ON_CONTACT, END_TURN) y `priority`.
- **Aprendizaje (`MoveRules`):** un movimiento al crearse el pokémon (siempre de daño) y otro cada 2 niveles. Al azar según `learn_weight`, solo de sus tipos o de sus etiquetas de aprendizaje. Lo intentado queda *desbloqueado* y no vuelve a salir. Máximo 4 equipados; si no caben, `LearnComponent` pregunta (olvidar uno / no aprenderlo). Se editan desde Pokémon > Movimientos.
- **Daño (`BattleRules`):** fórmula de la serie con tipo (tabla de tipos), STAB, crítico (1/16, +1 nivel por movimiento, habilidad u objeto), niveles de estadística, quemadura, Vidasfera, precisión/evasión, variación 85-100 %.
- **Orden:** cambio de pokémon > prioridad (etiquetas) > Garra rápida > velocidad (parálisis x0,5) > azar.
- **Combate (`Battle`):** 1 contra 1 con cambios; sin PP usa Forcejeo; el rival elige por daño esperado. Los mensajes del turno se reproducen uno a uno (`BattleComponent`).
- **Entrenadores de prueba** (junto al inicio): Rojo = duelo en tiempo real (su pokémon sale al mundo, como un salvaje); Azul = combate por turnos (carga, cámara tras tu pokémon, sin movimiento).
