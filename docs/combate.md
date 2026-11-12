# Combate por turnos y movimientos

- **Datos:** `move`, `move_class`, `move_tag`, `learn_tag` (ver base-de-datos.md). Los efectos viven en `effect` (columnas `kind`, `param`, `target`, `amount`, `chance`, `multiplier`): STAT_STAGE, STATUS, HEAL_SELF, RECOIL, DRAIN, PROTECT, CRIT_BOOST, FLINCH, ACCURACY_BOOST. Las habilidades añaden `trigger` (PASSIVE, ON_ENTER, ON_CONTACT, END_TURN) y `priority`.
- **Aprendizaje (`MoveRules`):** un movimiento al crearse el pokémon (siempre de daño) y otro cada 2 niveles. Al azar según `learn_weight`, solo de sus tipos o de sus etiquetas de aprendizaje. Lo intentado queda *desbloqueado* y no vuelve a salir. Máximo 4 equipados; si no caben, `LearnComponent` pregunta (olvidar uno / no aprenderlo). Se editan desde Pokémon > Movimientos.
- **Daño (`BattleRules`):** fórmula de la serie con tipo (tabla de tipos), STAB, crítico (1/16, +1 nivel por movimiento, habilidad u objeto), niveles de estadística, quemadura, Vidasfera, precisión/evasión, variación 85-100 %.
- **Orden:** cambio de pokémon > prioridad (etiquetas) > Garra rápida > velocidad (parálisis x0,5) > azar.
- **Combate (`Battle`):** 1 contra 1 con cambios; sin PP usa Forcejeo; el rival elige por daño esperado. Los mensajes del turno se reproducen uno a uno (`BattleComponent`).
- **Entrenadores de prueba** (junto al inicio): Rojo = duelo en tiempo real (su pokémon sale al mundo, como un salvaje); Azul = combate por turnos (carga, cámara tras tu pokémon, sin movimiento).

## Zona de combate por turnos
Un disco circular en el suelo (`Scene::ARENA_*`, centro al sur de los entrenadores) y nada más. Al empezar, durante la pantalla de carga, el entrenador y el jugador caminan a puntos opuestos del borde (por fuera) y los pokémon pelean en medio; al terminar vuelven a donde estaban. La cámara mira de lado a la línea de combate: nuestro pokémon a la izquierda y el rival a la derecha; se gira alrededor de la zona con A/D (o flechas) o arrastrando con el botón derecho, y W/S sube y baja. Sale siempre el primer pokémon del equipo que no esté debilitado.

Interfaz: abajo, de izquierda a derecha, texto | controles (Luchar, Pokémon...) con la tarjeta de nuestro pokémon justo encima; los movimientos y el equipo ocupan el sitio de los controles. Cada tarjeta de movimiento lleva todo en una sola fila: símbolo de clase y logo del tipo a la izquierda, nombre en medio y POT / PREC / PP a la derecha; el borde indica la eficacia (negro nulo, gris oscuro x1/4, gris x1/2, verde x2, naranja x4) y un texto pequeño sobre el borde superior la nombra (nada si el daño es normal). La descripción sale con clic derecho. Los tipos se muestran como logos. Al subir/bajar una estadística o activarse una habilidad hay aviso flotante y aros en 3D. Al cambiar o debilitarse un pokémon se anima la pokéball (recoge con rayo rojo / la lanza y se abre / se hunde).

## Experiencia
Una sola fórmula (`ExperienceRules`) para todo: más experiencia cuanto más nivel tenga el rival respecto a quien la gana, dividida entre los pokémon que participaron y siguen en pie al derrotarlo (el debilitado no cobra ni cuenta), por la rareza de la especie rival. Capturar da la misma experiencia reducida a la mitad. En combates contra entrenadores no hay dinero; el jugador gana su propia experiencia por cada pokémon rival derrotado. No se pueden capturar los pokémon de un entrenador (la bola se recupera y avisa).

## Obstáculos
`Steering::avoid` (tres rayos) lo usan el compañero y los salvajes/pokémon de entrenadores al ir hacia su rival: rodean lo que haya de por medio.

## Tienda
El Tendero (junto al inicio) abre la tienda: pestañas COMPRAR / VENDER, categorías a la izquierda, cantidad elegida en un diálogo (VENDER TODO vende todo). Vende los objetos del apartado «Pokémon» con `buy_price > 0` (un precio 0 los excluye: Honor Ball, Starter Ball, Shiny Ball) y compra lo que tenga `sell_price > 0`. Regalos por compra: `item.gift_every` / `gift_reward_id`.

## Coco
`assets/models/fruits/coconut.glb` (nodos `mitad_a` y `mitad_b`, sin reescalar). Un coco puede partirse al caer del árbol (12 %) o cuando un pokémon se lo come; cada mitad es un cuerpo físico propio y es el objeto «Mitad de coco» (material nuevo), que se recoge. Sonido: `assets/audio/sfx/coconut_break.wav`.

## Plátanos
`assets/models/fruits/banana_bunch.glb` (nodos `corona` y `platano_1..5`). Un racimo cae del árbol y cada plátano tiene un 30 % de soltarse al tocar el suelo (cada uno es un cuerpo físico). Cuando un pokémon come, se quita un solo plátano del racimo (se encoge suelto); sin plátanos, el racimo desaparece. El jugador recoge el racimo entero como «Racimo de plátanos» (material nuevo) o, si le faltan, tantos «Plátano» como queden.

## Combate por turnos: dobles y detalles
- Hueco del campo = lado × 2 + posición (0–1 jugador, 2–3 rival). En individuales solo se usan 0 y 2. El Entrenador Verde lucha en dobles (2 contra 2): se da una orden por pokémon y, si el movimiento ataca al rival y quedan dos, se elige a cuál.
- La tarjeta de cada pokémon propio va pegada al borde derecho; hay un botón «Tabla de tipos» (al volver, el combate sigue donde estaba).
- Al salir y volver a su pokéball se usa la misma animación que el acompañante (`BallTransfer`, con la bola de cada pokémon).
- Cámara: empieza detrás de nuestro pokémon (el nuestro a la izquierda, el rival a la derecha) y orbita siempre desde el borde del círculo (radio 10 m).
- El Entrenador Azul da una medalla de gimnasio al derrotarlo (una vez por sesión: mecánica de prueba).
- En el duelo en tiempo real, al apuntar al pokémon de un entrenador sale «No se puede capturar» y no se lanza la pokéball; si le llega una, rebota en su cuerpo.
## Registro de cierres
`CrashUtil` instala un manejador vectorial (dirección real del fallo), guarda el volcado con la excepción, anota los últimos hechos (`note`) y la pantalla en curso. `IM_ASSERT` de ImGui se redirige con `/FI imguiAssert.hpp`. La compilación genera `PokemonEdge.pdb` (se publica aparte).
