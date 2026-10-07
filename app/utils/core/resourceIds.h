#pragma once

// Identificadores de los recursos incrustados en el ejecutable.
// Es un .h de C puro porque lo leen a la vez resources.rc y el código C++.
// IMPORTANTE: rc.exe exige que la última línea termine en salto de línea (si no, error RC1004).
#define IDR_DATABASE 101
#define IDI_ICON1 102
#define IDR_ASSET_INDEX 103
