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

        void initializeStyle() {
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 4.0f;
            style.FrameRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.ScrollbarRounding = 6.0f;
            style.WindowBorderSize = 0.0f;
            style.FrameBorderSize = 0.0f;
            style.TabRounding = 3.0f;
            
            auto& colors = style.Colors;
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.10f, 0.11f, 0.14f, 1.0f);
            colors[ImGuiCol_Header]               = ImVec4(0.16f, 0.18f, 0.24f, 1.0f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.22f, 0.25f, 0.32f, 1.0f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.11f, 0.14f, 1.0f);
            colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(.1, .1, .1, .25);
            colors[ImGuiCol_WindowBg]             = ImVec4(0.09f, 0.09f, 0.12f, 1.0f);
            colors[ImGuiCol_Border]               = ImVec4(0.35f, 0.40f, 0.50f, 0.40f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.07f, 0.07f, 0.10f, 1.0f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
            colors[ImGuiCol_TitleBgCollapsed]     = colors[ImGuiCol_TitleBg];
            colors[ImGuiCol_ChildBg]              = ImGui::ColorConvertU32ToFloat4(IM_COL32(19, 19, 26, 165));
            colors[ImGuiCol_WindowBg]             = ImGui::ColorConvertU32ToFloat4(IM_COL32(19, 19, 26, 165));
            colors[ImGuiCol_MenuBarBg]            = ImGui::ColorConvertU32ToFloat4(IM_COL32(29, 29, 39, 255));
            colors[ImGuiCol_Separator]            = ImGui::ColorConvertU32ToFloat4(IM_COL32(42, 42, 55, 255));
            colors[ImGuiCol_Text]                 = ImGui::ColorConvertU32ToFloat4(IM_COL32(231, 230, 255, 255));
            colors[ImGuiCol_ScrollbarGrab]        = ImGui::ColorConvertU32ToFloat4(IM_COL32(41, 46, 61, 255));
            colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(57, 64, 90, 255));
            colors[ImGuiCol_ScrollbarGrabActive]  = ImGui::ColorConvertU32ToFloat4(IM_COL32(31, 34, 43, 255));
            colors[ImGuiCol_Button]               = ImGui::ColorConvertU32ToFloat4(IM_COL32(41, 46, 61, 255));
            colors[ImGuiCol_ButtonHovered]        = ImGui::ColorConvertU32ToFloat4(IM_COL32(57, 64, 90, 255));
            colors[ImGuiCol_ButtonActive]         = ImGui::ColorConvertU32ToFloat4(IM_COL32(31, 34, 43, 255));

            ImGuiWindowFlags hostFlags =
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_MenuBar;

            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->WorkPos);
            ImGui::SetNextWindowSize(vp->WorkSize);
            ImGui::SetNextWindowViewport(vp->ID);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGui::Begin("###DockHost", nullptr, hostFlags);

            ImGui::PopStyleVar(2);
        }

    public:
        HubHomePage(spider_engine::core_engine::CoreEngine* coreEngine, 
                    ProjectManager*                         projectManager) :
            coreEngine_(coreEngine),
            projectManager_(projectManager)
        {
            renderer_ = &coreEngine_->getRenderer();

            logoDescriptorHeap_ = renderer_->createUserDescriptorHeap("LogoDescriptorHeap");

            logoTexture2D_ = renderer_->createTexture2D(
                L"C:\\Users\\gupue\\source\\repos\\spider-engine\\docs\\transparent\\1280x720_transparent.png",
                320, 
                180
            );
            
            logoSrv_ = renderer_->createShaderResourceViewForTexture2D(
                "HubLogoSrv", 
                logoTexture2D_,
                spider_engine::d3dx12::ShaderStage::STAGE_PIXEL,
                logoDescriptorHeap_);
            
            logoImGuiTextureID = static_cast<ImTextureID>(logoSrv_.getGPUDescriptorHandle().ptr);
        }

        void draw() override {
            initializeStyle();

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

            ImVec2 windowSize = ImGui::GetContentRegionAvail();

            ImVec2 imageSize(1280.0f / 4.0f, 720.0f / 4.0f);
            float offsetX = (windowSize.x - imageSize.x) * 0.5f;

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            ImGui::Image(logoImGuiTextureID, imageSize);
            
            static char        projectName[256] = "";
            static std::string projectPath;

            ImVec2 buttonSize(140, 27.5);
            offsetX = (windowSize.x - buttonSize.x) * 0.5f;

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.26f, 0.36f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.34f, 0.46f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.22f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.50f, 0.55f, 0.80f, 0.70f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
            if (ImGui::Button("Create new Project", buttonSize)) {
                ImGui::OpenPopup("New Project");
            }

            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();

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

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

            auto it = projectManager_->getProjects().begin();
            while (it != projectManager_->getProjects().end()) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
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