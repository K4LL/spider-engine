#pragma once
#include <iostream>
#include <shellapi.h>

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
        spider_engine::d3dx12::DX12Renderer*    renderer_;

        ProjectManager* projectManager_;

        spider_engine::d3dx12::DescriptorHeap*    logoDescriptorHeap_;
        spider_engine::d3dx12::Texture2D          logoTexture2D_;
        spider_engine::d3dx12::ShaderResourceView logoSrv_;
        ImTextureID                               logoImGuiTextureID;

    public:
        HubHomePage(spider_engine::core_engine::CoreEngine* coreEngine, 
                    ProjectManager*                         projectManager) :
            coreEngine_(coreEngine),
            projectManager_(projectManager)
        {
            renderer_ = &coreEngine_->getRenderer();

            logoDescriptorHeap_ = renderer_->createUserDescriptorHeap("HubLogoDescriptorHeap");
            logoTexture2D_      = renderer_->createTexture2D(
                L"C:\\Users\\gupue\\source\\repos\\spider-engine\\docs\\transparent\\TV - 1 (3).png"
            );
            logoSrv_ = renderer_->createShaderResourceViewForTexture2D(
                "HubLogoSrv", 
                logoTexture2D_,
                spider_engine::d3dx12::ShaderStage::STAGE_PIXEL,
                logoDescriptorHeap_);
            
            logoImGuiTextureID = static_cast<ImTextureID>(logoSrv_.getGPUDescriptorHandle().ptr);
        }

        void draw() override {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Menu"))
                {
                    if (ImGui::MenuItem("About")) {
                        ShellExecuteW(0, L"open", L"https://github.com/K4LL/spider-engine", 0, 0, SW_SHOWNORMAL);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Quit"))
                    {
                        coreEngine_->getWindow().isRunning = false;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            ImGui::Image(
                logoImGuiTextureID, 
                ImVec2(1280 / 4, 720 / 4), 
                ImVec2(0, 0), 
                ImVec2(1, 1), 
                ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                ImVec4(0.0f, 0.0f, 0.0f, 0.0f)
            );
            
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