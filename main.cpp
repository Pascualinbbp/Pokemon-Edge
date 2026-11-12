#include "app/managers/appManager.hpp"
#include "app/utils/core/crashUtil.hpp"
#include "app/utils/core/loggerUtil.hpp"
#include <exception>
#include <string>

// Llamada por IM_ASSERT (utils/core/imguiAssert.hpp, incluido a la fuerza al compilar): registra la aserción con la etapa y los últimos hechos, guarda el volcado y cierra.
void ImGuiAssertFailed(const char* expression, const char* file, int line) { CrashUtil::assertFailed(expression, file, line); }

int main() {
    CrashUtil::install(); // cierres que no son excepciones de C++ (acceso a memoria, abort, bloqueos): quedan en el registro
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
