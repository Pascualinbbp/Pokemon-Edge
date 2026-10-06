[← Volver al índice](../README.md)

# Jugabilidad

Todos los valores indicados proceden del código actual (versión 0.0.2) y de los datos de prueba de la base de datos. El juego está en desarrollo: el mundo es una zona de pruebas.

## Mundo

- Zona cuadrada de 80 × 80 unidades (de −40 a +40 en X y Z) con suelo plano en tablero.
- Dos paredes sólidas que bloquean el paso, las Pokéballs y la luz.
- El jugador empieza en el origen mirando hacia +Z.

## Movimiento

| Acción | Velocidad / valor |
|---|---|
| Caminar | 5,0 |
| Correr | 9,0 |
| Agachado | 2,5 |
| Caminar apuntando | 3,5 |
| Deslizamiento | 10,5 durante 0,9 s (baja a la velocidad de correr al final) |
| Salto | 7,0 (conserva el impulso en el aire) |

- Correr termina al dejar de avanzar o al apuntar.
- Pulsar agacharse **corriendo** inicia un deslizamiento; en cualquier otro caso agacha o levanta. Pulsado en el aire, se aplica al aterrizar.
- Durante el deslizamiento, A/D (o el stick) giran la dirección.
- Saltar desde un deslizamiento conserva el impulso de la carrera.

## Cámara

- **Normal:** orbital en tercera persona a 7 unidades del jugador.
- **Apuntado:** pasa suavemente a una cámara sobre el hombro derecho (3,2 unidades), con menor campo de visión y sensibilidad reducida (60 %).
- **Fijado de objetivo:** la cámara se orienta suavemente hacia un Pokémon dentro del alcance (24 unidades; se mantiene hasta 3 unidades más lejos).

## Captura de Pokémon

En esta versión los Pokémon son **cubos objetivo** inmóviles (el cubito amarillo indica hacia dónde miran). Hay 4 en posiciones fijas, con distinta orientación.

1. Entra en modo captura (clic derecho / L2).
2. Apunta con la cruceta del centro de la pantalla. Si apuntas a un Pokémon en alcance se muestra su **porcentaje de captura** (color: verde ≥ 70 %, amarillo ≥ 45 %, naranja ≥ 25 %, rojo por debajo).
3. Lanza la Pokéball (clic izquierdo / R2). Cooldown de 0,45 s.

### Porcentaje de captura

```
porcentaje = base × multiplicador de la bola × (1,15 si por la espalda) × (1,25 si agachado)   [máx. 99 %]
```

| Factor | Valor |
|---|---|
| Base | Aleatoria entre 5 % y 95 % (se vuelve a sortear cuando el Pokémon reaparece o escapa) |
| Bonificación por la espalda | ×1,15 (HUD: `ESPALDA`) |
| Bonificación por sigilo (agachado) | ×1,25 (HUD: `SIGILO`) |
| Máximo | 99 % |

### Lanzamientos especiales

| Tipo | Probabilidad | Efecto |
|---|---|---|
| Con suerte | 2 % | Captura segura, un solo giro, 12 estrellas doradas |
| Super suerte | 0,5 % | Captura segura, un solo giro, 16 estrellas azules |

### Resultado y animación

El resultado se decide al instante del impacto; la animación solo lo reproduce: el Pokémon se encoge dentro de la bola, la bola cae, se tambalea y termina con estrellas (captura) o abriéndose (fallo).

- Captura normal: 3 giros. Un fallo da 0, 1 o 2 giros.
- Capturado: desaparece y reaparece en su sitio a los 2,5 s con un porcentaje nuevo.
- Escapado: vuelve a su sitio con un porcentaje nuevo.

### Pokéballs

| Pokéball | Multiplicador | Color |
|---|---|---|
| Poké Ball | ×1,0 | Rojo |
| Super Ball | ×1,5 | Azul |
| Ultra Ball | ×2,0 | Amarillo |

La trayectoria de la bola se calcula para pasar por el punto apuntado (velocidad de lanzamiento 40, con la mitad de gravedad). Bota y rueda al caer y desaparece a los 8 s. Como máximo hay 12 bolas a la vez.

## Cofres

- 4 puntos fijos de aparición. Tras abrir un cofre, aparece otro en su punto a los **20 s**.
- Se abren con **Interactuar** a menos de 2,2 unidades. Al abrirse levantan la tapa, sueltan chispas del color de su rareza y se desvanecen.
- Cada cofre da **una sola** recompensa, elegida según su probabilidad (se normalizan, no hace falta que sumen 100).

| Cofre | Aparición (peso) | Recompensas (cantidad, probabilidad) |
|---|---|---|
| Cofre común | 70 | Poké Ball ×10 (40), Madera ×6 (20), Piedra ×6 (20), Carbón ×3 (12), Hierro ×1 (8) |
| Cofre raro | 25 | Super Ball ×5 (35), Poké Ball ×15 (15), Hierro ×4 (20), Carbón ×6 (10), Madera ×12 (10), Piedra ×12 (10) |
| Cofre épico | 5 | Ultra Ball ×1 (35), Super Ball ×10 (20), Hierro ×12 (25), Carbón ×15 (10), Madera ×30 (5), Piedra ×30 (5) |

Aspecto por rareza: común (tapa de madera), raro (azul, 10 chispas), épico (morado, 16 chispas).

## Recolección de recursos

- 10 puntos fijos de aparición. Tras agotarse un nodo, aparece otro en su punto a los **60 s**.
- Se golpean con **Interactuar** a menos de 1,6 unidades del borde de su hitbox (mínimo 0,4 s entre golpes). Al agotarse dan su material directamente al inventario.

| Nodo | Acción | Material | Golpes | Cantidad | Aparición (peso) |
|---|---|---|---|---|---|
| Árbol | Talar | Madera | 4 | 2–4 | 40 |
| Roca | Picar | Piedra | 5 | 2–4 | 30 |
| Mena de hierro | Picar | Hierro | 6 | 1–3 | 20 |
| Filón de carbón | Picar | Carbón | 5 | 1–3 | 10 |

## Inventario

- Una ranura por cada objeto del juego (Pokéballs y materiales). Se empieza con 0 unidades de todo: se consiguen en cofres y recursos.
- Con varias Pokéballs disponibles puedes cambiar la equipada (Q / E, rueda, L1 / R1) en modo captura. Si la equipada no tiene unidades, aparece el aviso `¡SIN UNIDADES!`.
- El inventario y la Pokéball equipada se guardan con la partida.

## Ciclo de día y noche

- Ciclo completo de 300 s: 180 s de día y 120 s de noche. La partida nueva empieza por la mañana.
- El sol cruza el cielo de este a oeste; la luna va en el punto opuesto. Hay amanecer y atardecer anaranjados, estrellas con parpadeo por la noche y sombras con el sol o la luna como luz activa.
- La hora del mundo se guarda con la partida.

## Interfaz en partida (HUD)

- FPS, contador de capturas de la sesión y materiales que tienes.
- Mira circular con el porcentaje de captura y las bonificaciones activas; indicador `FIJADO` si hay objetivo fijado.
- Selector de Pokéball con unidades y multiplicador (solo apuntando).
- Ayudas contextuales de controles a la izquierda, según la acción disponible (interactuar, modo captura, etc.).
- Avisos grandes: `¡CAPTURADO!`, `¡SE HA ESCAPADO!`, `¡LANZAMIENTO CON SUERTE!`, `¡SUPER SUERTE!`, `¡SIN UNIDADES!`, `¡HAS OBTENIDO!` (con la recompensa).
- Aviso `Guardando...` en la esquina inferior derecha en cada autoguardado.

## Menús

- **Título:** pulsa cualquier tecla, clic o botón del mando.
- **Menú principal:** CARGAR PARTIDA (solo si hay partidas), NUEVA PARTIDA, VOLVER AL TÍTULO.
- **Pausa:** CONTINUAR, CONTROLES, MENU PRINCIPAL.

Más información en [Guardado de partidas](guardado.md).

---

[← Volver al índice](../README.md) · Anterior: [Controles](controles.md) · Siguiente: [Guardado de partidas](guardado.md)
