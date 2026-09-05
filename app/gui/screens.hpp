#pragma once
#include "imgui.h"
#include "../managers/databaseManager.hpp"
#include <string>
#include <vector>

enum class GameState {
    TITLE_SCREEN,
    MAIN_MENU,
    TYPE_DATABASE_STUDIO
};

namespace Screens {
    inline void renderTitleScreen(GameState& gameState, ID3D11ShaderResourceView* logoTexture, int logoW, int logoH) {
        ImVec2 windowSize = ImGui::GetWindowSize();

        ImGui::SetCursorPosY(windowSize.y * 0.15f);
        if (logoTexture) {
            float renderW = 320.0f;
            float renderH = (logoH > 0 && logoW > 0) ? (renderW * ((float)logoH / (float)logoW)) : 120.0f;
            ImGui::SetCursorPosX((windowSize.x - renderW) * 0.5f);
            ImGui::Image((void*)logoTexture, ImVec2(renderW, renderH));
        } else {
            std::string titleBig = "P O K E M O N   E D G E";
            float tWidth = ImGui::CalcTextSize(titleBig.c_str()).x;
            ImGui::SetCursorPosX((windowSize.x - tWidth) * 0.5f);
            ImGui::TextColored(ImVec4(0.003f, 0.725f, 0.992f, 1.0f), "%s", titleBig.c_str());
        }

        // Texto estático en gris medio en lugar de parpadeo molesto
        ImGui::SetCursorPosY(windowSize.y * 0.65f);
        std::string pressText = "- PRESIONA UN BOTÓN PARA CONTINUAR -";
        float pressWidth = ImGui::CalcTextSize(pressText.c_str()).x;
        ImGui::SetCursorPosX((windowSize.x - pressWidth) * 0.5f);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "%s", pressText.c_str());

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        float btnWidth = 260.0f;
        ImGui::SetCursorPosX((windowSize.x - btnWidth) * 0.5f);
        if (ImGui::Button("INICIAR AVENTURA", ImVec2(btnWidth, 48))) {
            gameState = GameState::MAIN_MENU;
        }
    }

    inline void renderMainMenu(GameState& gameState) {
        ImVec2 windowSize = ImGui::GetWindowSize();
        
        ImGui::SetCursorPosY(windowSize.y * 0.2f);
        std::string menuTitle = "MENÚ PRINCIPAL";
        float mTitleWidth = ImGui::CalcTextSize(menuTitle.c_str()).x;
        ImGui::SetCursorPosX((windowSize.x - mTitleWidth) * 0.5f);
        ImGui::TextColored(ImVec4(0.003f, 0.725f, 0.992f, 1.0f), "%s", menuTitle.c_str());
        
        ImGui::Dummy(ImVec2(0.0f, 40.0f));
        float menuBtnWidth = 340.0f;
        
        ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
        if (ImGui::Button("NUEVA PARTIDA [Estudio de Tipos BD]", ImVec2(menuBtnWidth, 48))) {
            gameState = GameState::TYPE_DATABASE_STUDIO;
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
        if (ImGui::Button("OPCIONES DEL SISTEMA", ImVec2(menuBtnWidth, 48))) {
            // Acción de opciones
        }

        ImGui::Dummy(ImVec2(0.0f, 15.0f));
        ImGui::SetCursorPosX((windowSize.x - menuBtnWidth) * 0.5f);
        if (ImGui::Button("VOLVER AL TÍTULO", ImVec2(menuBtnWidth, 48))) {
            gameState = GameState::TITLE_SCREEN;
        }
    }

    inline void renderTypeStudio(GameState& gameState, char* searchBuffer, size_t bufferSize, std::string& searchResultMsg, std::vector<PokemonType>& cachedList, bool& listLoaded) {
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