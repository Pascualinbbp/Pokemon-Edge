#pragma once

// Objeto suelto que puede aparecer en el mundo (tabla ground_item): da entre minQuantity y maxQuantity unidades.
struct GroundItemType {
    int itemId = -1;
    int minQuantity = 1;
    int maxQuantity = 1;
    float spawnWeight = 1.0f;
};
