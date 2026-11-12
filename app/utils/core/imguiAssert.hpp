#pragma once
// Se incluye a la fuerza en toda la compilación (/FI): las aserciones de ImGui dejan en el registro cuál falló y dónde antes de cerrar el juego.
#ifdef __cplusplus
extern "C++" void ImGuiAssertFailed(const char* expression, const char* file, int line);
#else
void ImGuiAssertFailed(const char* expression, const char* file, int line);
#endif
#define IM_ASSERT(_EXPR) ((void)((!!(_EXPR)) || (ImGuiAssertFailed(#_EXPR, __FILE__, __LINE__), 0)))
