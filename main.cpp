#include "app/managers/appManager.hpp"
#include "app/utils/loggerUtil.hpp"

int main() {
    Logger::logInfo("SYSTEM", "--- INICIO DE APLICACIÓN (C++) ---");
    
    AppManager app;
    app.start();

    return 0;
}