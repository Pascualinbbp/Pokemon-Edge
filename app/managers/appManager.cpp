#include "appManager.hpp"
#include "updateManager.hpp"
#include "databaseManager.hpp"
#include "../utils/loggerUtil.hpp"
#include <iostream>

AppManager::AppManager() {}

void AppManager::start() {
    Logger::logInfo("APP_MANAGER", "Inicializando verificación de sistema...");
    UpdateManager::checkAndHandleUpdate();

    DatabaseManager::init();

    Logger::logInfo("APP_MANAGER", "Iniciando motor principal...");
    runMainLogic();
}

void AppManager::runMainLogic() {
    Logger::logInfo("MAIN_CORE", "¡Bienvenido a Pokemon Edge! Ejecutando aplicación...");

    int opcion = -1;
    while (opcion != 0) {
        std::cout << "\n==============================\n";
        std::cout << "      POKEMON EDGE - MENU     \n";
        std::cout << "==============================\n";
        std::cout << "1. Buscar tipo de Pokemon por ID\n";
        std::cout << "2. Listar todos los tipos\n";
        std::cout << "0. Salir\n";
        std::cout << "Seleccione una opcion: ";

        if (!(std::cin >> opcion)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Entrada no valida. Intente de nuevo.\n";
            continue;
        }

        switch (opcion) {
            case 1: {
                int idSearch;
                std::cout << "Ingrese el ID del tipo a buscar: ";
                std::cin >> idSearch;
                
                PokemonType result = DatabaseManager::getTypeDao().findById(idSearch);
                if (result.id != -1) {
                    std::cout << "\n[Encontrado] ID: " << result.id << " | Nombre: " << result.name << "\n";
                } else {
                    std::cout << "\n[Aviso] No se encontro ningun tipo con el ID " << idSearch << "\n";
                }
                break;
            }
            case 2: {
                std::vector<PokemonType> list = DatabaseManager::getTypeDao().findAll();
                std::cout << "\n--- LISTA DE TIPOS DE POKEMON ---\n";
                for (const auto& t : list) {
                    std::cout << "ID: " << t.id << " - " << t.name << "\n";
                }
                std::cout << "--------------------------------\n";
                break;
            }
            case 0:
                std::cout << "Saliendo de Pokemon Edge. ¡Hasta pronto!\n";
                break;
            default:
                std::cout << "Opcion desconocida. Intente de nuevo.\n";
        }
    }
}