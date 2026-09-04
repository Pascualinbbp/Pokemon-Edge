#include "app/managers/app_manager.hpp"
#include "app/utils/logger_util.hpp"

int main() {
    Logger::logInfo("SYSTEM", "--- INICIO DE APLICACIÓN (C++) ---");
    
    AppManager app;
    app.start();

    return 0;
}