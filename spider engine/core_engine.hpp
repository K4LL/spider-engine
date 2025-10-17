#pragma once
#include <memory>

#include "dx12_renderer.hpp"

#include "flecs.h"

namespace spider_engine::core_engine {
	class CoreEngine {
	private:
		flecs::world world_;

		std::unique_ptr<d3dx12::DX12Renderer> renderer_;
		std::unique_ptr<d3dx12::DX12Compiler> compiler_;

	public:
		template <typename... Types>
		CoreEngine() {
			// Register internal components (dx12 types)
			world_.component<d3dx12::ShaderStage>();
			world_.component<d3dx12::Vertex>();
			world_.component<d3dx12::VertexArrayBuffer>();
			world_.component<d3dx12::IndexArrayBuffer>();
			world_.component<d3dx12::ConstantBufferVariable>();
			world_.component<d3dx12::ConstantBufferData>();
			world_.component<d3dx12::ShaderDescription>();
			world_.component<d3dx12::ConstantBuffer>();
			world_.component<d3dx12::ConstantBuffers>();
			world_.component<d3dx12::ResourceBindingData>();
			world_.component<d3dx12::ShaderData>();
			world_.component<d3dx12::Mesh>();
			world_.component<d3dx12::Renderizable>();
			world_.component<d3dx12::Shader>();
			world_.component<d3dx12::RenderPipeline>();

			// Initialize internal components (rendering)
			world_.component<rendering::Transform>();
			world_.component<rendering::FrameData>();

			// Register user components
			(world_.component<Types>(), ...);
		}

		void intitializeRenderingSystems(HWND          hwnd,
										 const uint8_t bufferCount,
										 const bool    isFullScreen = false,
										 const bool    isVSync      = true,
										 const uint8_t deviceId     = 0) 
		{
			renderer_ = std::make_unique<d3dx12::DX12Renderer>(&world_, hwnd, bufferCount, isFullScreen, isVSync, deviceId);
			compiler_ = std::make_unique<d3dx12::DX12Compiler>(&world_, *renderer_);
		}
		void initializeDebugSystems(const bool enableLogs     = true,
									const bool enableWarnings = true,
									const bool enableErrors   = true) 
		{
			AllocConsole();

			FILE* fp;
			freopen_s(&fp, "CONOUT$", "w", stdout);
			freopen_s(&fp, "CONOUT$", "w", stderr);
			freopen_s(&fp, "CONIN$", "r", stdin);

			std::ios::sync_with_stdio();

			std::wcout.clear();
			std::cout.clear();
			std::wcerr.clear();
			std::cerr.clear();
			std::wcin.clear();
			std::cin.clear();

			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

			system("cls");
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
			
			std::wstring title =
				LR"(  ___ ___ ___ ___  ___ ___   ___ _  _  ___ ___ _  _ ___ 
 / __| _ \_ _|   \| __| _ \ | __| \| |/ __|_ _| \| | __|
 \__ \  _/| || |) | _||   / | _|| .` | (_ || || .` | _| 
 |___/_| |___|___/|___|_|_\ |___|_|\_|\___|___|_|\_|___|                                                      
)";

			SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			std::wcout << title << L"\n";

			auto printFlag = [&](const std::wstring& label, bool enabled, WORD color) {
				SetConsoleTextAttribute(hConsole, color | (enabled ? FOREGROUND_INTENSITY : 0));
				std::wcout << L"[" << (enabled ? L"ENABLED" : L"DISABLED") << L"] " << label << L"\n";
				};

			printFlag(L"Logs", enableLogs, FOREGROUND_GREEN);
			printFlag(L"Warnings", enableWarnings, FOREGROUND_RED | FOREGROUND_GREEN);
			printFlag(L"Errors", enableErrors, FOREGROUND_RED);

			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

			std::wcout << L"\n";
		}

		flecs::entity createEntity(const std::string& name = "") {
			if (name.empty()) return world_.entity();
			return world_.entity(name.c_str());
		}

		template <typename Ty>
		void addComponent(flecs::entity entity, Ty&& item) {
			entity.set<Ty>(std::forward<Ty>(item));
		}
		template <typename Ty>
		Ty* getComponent(flecs::entity entity) {
			return entity.get_mut<Ty>();
		}
		template <typename Ty>
		const Ty* getConstComponent(flecs::entity entity) {
			return entity.get<Ty>();
		}

		flecs::world& getWorld() {
			return world_;
		}

		d3dx12::DX12Renderer& getRenderer() {
			return *renderer_;
		}
		d3dx12::DX12Compiler& getCompiler() {
			return *compiler_;
		}
	};
}