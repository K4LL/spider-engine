#pragma once
#include <iostream>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "hub_page.hpp"

#include "core_engine.hpp"

#include "project_manager.hpp"

namespace spider_engine::hub {
    class HubHomePage : public IPage {
    private:
        spider_engine::core_engine::CoreEngine* coreEngine_;

        ProjectManager* projectManager_;

    public:
        HubHomePage(spider_engine::core_engine::CoreEngine* coreEngine, 
                    ProjectManager*                         projectManager) :
            coreEngine_(coreEngine),
            projectManager_(projectManager)
        {}

        void draw() override {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Menu"))
                {
                    ImGui::MenuItem("Home");
                    ImGui::MenuItem("Settings");
                    ImGui::MenuItem("About");
                    ImGui::Separator();
                    if (ImGui::MenuItem("Quit"))
                    {
                        coreEngine_->getWindow().isRunning = false;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            static char        projectName[256] = "";
            static std::string projectPath;

            if (ImGui::Button("Create new Project")) {
                ImGui::OpenPopup("New Project");
            }

            if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::InputText("Project Name", projectName, IM_ARRAYSIZE(projectName));
                ImGui::Text("Project Path");
                ImGui::SameLine();
                if (ImGui::Button("Browse")) {
                    projectPath = spider_engine::helpers::toString(spider_engine::helpers::openFolderDialog());
                }

                ImGui::Spacing();

                if (ImGui::Button("Create"))
                {
                    projectManager_->addProject(projectName, projectPath);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            auto it = projectManager_->getProjects().begin();
            while (it != projectManager_->getProjects().end()) {
                ImGui::Separator();
                ImGui::Text(std::string("Name: " + it.current->value.first).c_str());
                ImGui::Text(std::string("Path: " + it.current->value.second).c_str());
                ImGui::SameLine();
                if (ImGui::Button(("Open##" + it->first).c_str())) {

                }
                ImGui::SameLine();
                if (ImGui::Button(("Delete##" + it->first).c_str())) {
                    projectManager_->removeProject(it.current->value.first);
                    it = projectManager_->getProjects().begin();
                    if (it == projectManager_->getProjects().end()) break;
                }

                ++it;
            }

            ImGui::End();
        }
	};
}