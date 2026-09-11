#pragma once
#include "../gui/mainWindow.hpp"
#include "../utils/loggerUtil.hpp"

class GuiManager {
public:
    static void init() {
        Logger::logInfo("GUI_MANAGER", "Delegando inicialización a la GUI principal...");
        MainWindow::init();
    }

    static void run() {
        Logger::logInfo("GUI_MANAGER", "Arrancando bucle de la interfaz...");
        MainWindow::run();
    }
};