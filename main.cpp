#include "app/managers/appManager.hpp"
#include "app/utils/loggerUtil.hpp"
#include <exception>

int main() {
    try {
        Logger::logInfo("SYSTEM", "--- INICIO DE APLICACIÓN (C++) ---");
        Logger::logInfo("SYSTEM", "Creando instancia de AppManager...");
        
        AppManager app;
        
        Logger::logInfo("SYSTEM", "Llamando a app.start()...");
        app.start();
        
        Logger::logInfo("SYSTEM", "--- FIN DE APLICACIÓN ---");
    } catch (const std::exception& e) {
        Logger::logError("SYSTEM", std::string("Fallo crítico (Excepción no capturada en main): ") + e.what());
    } catch (...) {
        Logger::logError("SYSTEM", "Fallo crítico (Excepción desconocida no capturada en main).");
    }

    return 0;
}