#pragma once
#include "../gui/window/mainWindow.hpp"
#include "../utils/core/loggerUtil.hpp"

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