[← Volver al índice](../README.md)

# Base de datos

Los datos de juego viven en **Neon (PostgreSQL)**. En cada compilación, GitHub Actions los vuelca (secret `NEON_DATABASE_URL`) a una base **SQLite** que se incrusta en el `.exe`; `DatabaseManager::loadGameData` la lee una sola vez y el motor solo recibe un `GameData`. Los DAO (`app/daos`) leen con `SqliteUtil` y siempre con `ORDER BY`.

## Tablas

| Tabla | Columnas | Para qué |
|---|---|---|
| `effect` | `id`, `name`, `kind`, `type_id`, `multiplier`, `shiny_multiplier`, `hp_below`, `amount`, `color` | Efecto de una habilidad o de un objeto. `kind`: `TYPE_BOOST`, `FLOAT`, `WATER_BREATHING`, `CAPTURE`, `CAPTURE_EXCLUSIVE`, `HEAL`, `HEAL_PERCENT` (`amount` = % de los PS máximos; la Max poción usa 100), `REVIVE`, `LEVEL`, `EV`, `EVOLVE`. Un efecto lo pueden usar varias habilidades u objetos |
| `ability` | `id`, `name`, `description`, `effect_id` | Habilidades de los pokémon |
| `rarity` | `id`, `name`, `spawn_weight`, `xp_multiplier` | Rareza de pokémon y cofres (probabilidad de aparecer y experiencia) |
| `category` | `id`, `name` | Pestaña de la mochila (objetos, materiales y herramientas) |
| `item` | `id`, `name`, `description`, `category_id`, `effect_id`, `buy_price`, `sell_price`, `gift_every`, `gift_reward_id` | Objetos con efecto: pokéballs, medicinas, entrenamiento y piedras evolutivas. `gift_*`: la tienda regala esa recompensa cada tantas unidades compradas |
| `material` | `id`, `name`, `description`, `category_id`, `color`, `buy_price`, `sell_price` | Recursos que se sacan del mundo |
| `tool`, `tool_level`, `tool_upgrade` | | Herramientas, sus niveles y los materiales que cuesta mejorarlas |
| `skill` | `id`, `name`, `description`, `animation` | Habilidad de recolección (talar, picar, regar…) |
| `resource_node`, `habitat_resource` | | Nodos de recolección y dónde salen |
| `reward` | `id` (grupo), `kind`, `target_id`, `probability`, `min_quantity`, `max_quantity` | Todas las recompensas. `kind`: `ITEM`, `MATERIAL`, `XP`. Al sortear un grupo sale **una** entrada según su probabilidad |
| `chest` | `id`, `name`, `rarity_id`, `reward_id` | Cofres |
| `dex_milestone` | `id`, `kind`, `threshold`, `species_id`, `description`, `progress`, `reward_id` | Hitos de la Pokédex (la experiencia que dan está en `reward`) |
| `generation` | `id`, `name`, `region` | Generación de cada pokémon |
| `pokemon`, `evolution`, `pokemon_habitat`, `pokemon_ecotone`, `pokemon_condition` | | Especies y sus tablas secundarias. `evolution`: `from_id`, `to_id`, `method`, `level`, `item_id` (piedra) |
| `type`, `type_matchup` | `type.color` | Tipos y fortalezas / debilidades |
| `habitat`, `weather`, `habitat_weather`, `weather_fusion` | | Hábitats y clima |
| `radio` | `id`, `name`, `genre` | Emisoras |

Grupos de `reward`: 1-4 cofres (4 = legendario, rareza 7: pocas unidades, lo mejor), 10 objetos del suelo (`RewardGroup::GROUND`, el común), 11-14 objetos del suelo por clima (templado, frío, árido, húmedo), 100+ hitos de la Pokédex, 200 regalo de la tienda.

Objetos del suelo: aparecen por todo el mundo cerca del jugador (`GroundField`). Su grupo es `habitat.ground_reward_id` del hábitat dominante en ese punto (NULL = grupo 10). Así las semillas varían por clima (la fresa no sale en el árido).

Equipables (categoría 9): efectos `HOLD_EXP_SHARE`, `HOLD_QUICK_CLAW`, `HOLD_RAZOR_CLAW`, `HOLD_FOCUS_BAND`, `HOLD_LEFTOVERS`, `HOLD_LIFE_ORB`; `amount` es el porcentaje de cada uno. Cultivos: materiales de semillas (categoría 10) cuyo nombre debe coincidir con `Crops::LIST` y nodos 24-26 (`waterable`, `ephemeral`, no están en `habitat_resource`; se siembran en las parcelas del inicio).

## Cómo se reparte `item` en el código

Cada fila se clasifica por su efecto: `CAPTURE` → `PokeballType`; `HEAL`/`HEAL_PERCENT`/`REVIVE`/`LEVEL`/`EV` → `TrainingItem`; `EVOLVE` → piedra evolutiva. Los materiales y las herramientas entran en el inventario con ids `1000 + id` y `2000 + id`. `CAPTURE_EXCLUSIVE` es la bola del pokémon inicial (no es un objeto).

## Assets según la base de datos

```
assets/
  sprites/  items/<categoría>/<id>.png · types/<id>.png · weather/<id>.png · radio/<id>.png · ui/
  models/   items/<categoría>/<id>.glb
  audio/    radio/<id>.mp3 · sfx/*.wav
```

`<categoría>` es el nombre de `category` sin tildes ni espacios (`pokeballs`, `medicinas`, `entrenamiento`, `piedras_evolutivas`). Un objeto sin sprite se dibuja con un icono de reserva.

## Cambiar la base de datos

`neon_cambios.sql` migra una base antigua a este esquema en una sola sentencia `DO $$ … $$;` (ejecutar en el editor SQL de Neon con **Run**). Si ya está migrada no hace nada.
