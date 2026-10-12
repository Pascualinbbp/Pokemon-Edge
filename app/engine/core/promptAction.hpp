#pragma once
#include <string>

// Botones que el juego puede pedir al jugador en una ayuda en pantalla ("pulsa F para...").
// El motor solo dice cuál; la interfaz decide el icono según el dispositivo (teclado, Xbox, PlayStation...).
enum class PromptAction { JUMP, CROUCH, SPRINT, PAUSE, AIM, THROW, LOCK, BALL_SWITCH, INTERACT, INVENTORY, MODE, MAP, RELEASE };

// Una ayuda que la escena pide dibujar (pila inferior): qué botón y qué hace.
struct Prompt {
    PromptAction action = PromptAction::INTERACT;
    std::string text;
    bool enabled = true; // false = se ve apagada (p. ej. capturar sin pokéballs)
};
