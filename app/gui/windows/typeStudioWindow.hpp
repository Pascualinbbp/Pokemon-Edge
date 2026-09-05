// C:\Users\bran\Desktop\Pokemon Edge\app\gui\windows\typeStudioWindow.hpp
#pragma once
#include "imgui.h"
#include "appState.hpp"
#include "../../managers/databaseManager.hpp"
#include <string>
#include <vector>

namespace TypeStudioWindow {
    inline void render(GameState& gameState, char* searchBuffer, size_t bufferSize, std::string& searchResultMsg, std::vector<PokemonType>& cachedList, bool& listLoaded) {
        if (ImGui::Button("< Menú Principal")) {
            gameState = GameState::MAIN_MENU;
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.6f, 1.0f), "  |  MODULO: CONSULTA DE TIPOS DE POKEMON");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        if (ImGui::BeginChild("LeftControlPanel", ImVec2(350, 0), true)) {
            ImGui::Text("Panel de Control");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            ImGui::Text("ID del Tipo:");
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##searchId", searchBuffer, bufferSize, ImGuiInputTextFlags_CharsDecimal);
            
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            if (ImGui::Button("Consultar ID", ImVec2(200, 32))) {
                std::string input(searchBuffer);
                if (!input.empty()) {
                    try {
                        int id = std::stoi(input);
                        PokemonType res = DatabaseManager::getTypeDao().findById(id);
                        if (res.id != -1) {
                            searchResultMsg = "Encontrado -> ID: " + std::to_string(res.id) + " | Tipo: " + res.name;
                        } else {
                            searchResultMsg = "Aviso: No existe el ID " + input;
                        }
                    } catch (...) {
                        searchResultMsg = "Error: Formato numérico incorrecto.";
                    }
                } else {
                    searchResultMsg = "Aviso: Introduce un ID válido.";
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 20.0f));
            if (ImGui::Button("Cargar Todos los Tipos", ImVec2(300, 38))) {
                cachedList = DatabaseManager::getTypeDao().findAll();
                listLoaded = true;
                searchResultMsg = "Cargados " + std::to_string(cachedList.size()) + " registros.";
            }

            ImGui::Dummy(ImVec2(0.0f, 25.0f));
            ImGui::TextWrapped("Consola:\n%s", searchResultMsg.c_str());
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("RightOutputPanel", ImVec2(0, 0), true)) {
            ImGui::Text("Visor de Registros Indexados");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            if (listLoaded) {
                if (ImGui::BeginTable("TypesGameTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 390))) {
                    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                    ImGui::TableSetupColumn("Nombre del Tipo", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (const auto& t : cachedList) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", t.id);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", t.name.c_str());
                    }
                    ImGui::EndTable();
                }
            } else {
                ImGui::TextDisabled("Haga clic en 'Cargar Todos los Tipos' para desplegar los datos.");
            }
        }
        ImGui::EndChild();
    }
}