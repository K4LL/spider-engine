#pragma once
#include <memory>

#include "window.hpp"
#include "dx12_renderer.hpp"
#include "camera.hpp"

#include "flecs.h"

namespace spider_engine::core_engine {
	struct RenderingSystemDescription {
		std::wstring windowName;
		std::wstring windowClassName;

		uint_t x;
		uint_t y;

		uint_t width;
		uint_t height;

		uint8_t  bufferCount;
		uint32_t threadCount;

		bool isFullScreen;
		bool isVSync;

		uint8_t deviceId;

		RenderingSystemDescription() :
			windowName(L"Spider Engine Window"),
			windowClassName(L"SpiderEngineMainWindowClass"),
			x(0),
			y(0),
			width(0),
			height(0),
			bufferCount(2),
			threadCount(4),
			isFullScreen(false),
			isVSync(true),
			deviceId(0)
		{}
	};

	class CoreEngine {
	private:
		flecs::world world_;

		std::unique_ptr<Window> window_;

		std::unique_ptr<d3dx12::DX12Renderer> renderer_;
		std::unique_ptr<d3dx12::DX12Compiler> compiler_;

		std::unique_ptr<spider_engine::rendering::Camera> camera_;

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
			world_.component<d3dx12::ShaderResourceView>();
			world_.component<d3dx12::ShaderResourceViews>();
			world_.component<d3dx12::ShaderResourceViewData>();
			world_.component<d3dx12::Sampler>();
			world_.component<d3dx12::Samplers>();
			world_.component<d3dx12::SamplerData>();
			world_.component<d3dx12::ShaderDescription>();
			world_.component<d3dx12::ConstantBuffer>();
			world_.component<d3dx12::ConstantBuffers>();
			world_.component<d3dx12::ResourceBindingData>();
			world_.component<d3dx12::ShaderData>();
			world_.component<d3dx12::Mesh>();
			world_.component<d3dx12::Texture2D>();
			world_.component<d3dx12::Renderizable>();
			world_.component<d3dx12::Shader>();
			world_.component<d3dx12::RenderPipeline>();

			// Initialize internal components (rendering)
			world_.component<rendering::Transform>();
			world_.component<rendering::FrameData>();

			// Register user components
			(world_.component<Types>(), ...);
		}

		void intitializeRenderingSystems(const RenderingSystemDescription& description) 
		{
			window_ = std::make_unique<Window>(
				description.windowName,
				description.width,
				description.height,
				description.x,
				description.y,
				description.windowClassName
			);
			renderer_ = std::make_unique<d3dx12::DX12Renderer>(
				&world_,
				window_->hwnd_,
				description.bufferCount,
				description.threadCount,
				description.isFullScreen,
				description.isVSync,
				description.deviceId
			);
			compiler_ = std::make_unique<d3dx12::DX12Compiler>(&world_, *renderer_);

			camera_ = std::make_unique<spider_engine::rendering::Camera>(window_->width_, window_->height_);
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
		void initializeDebugSystemsOnDebugMode(const bool enableLogs     = true,
											   const bool enableWarnings = true,
											   const bool enableErrors   = true)
		{
#ifdef _DEBUG
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
#endif
		}

		void start(std::function<void()> fn) {
			MSG msg = {};
			while (msg.message != WM_QUIT && window_->isRunning_) {
				while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				fn();
			}
		}
		void stop() {}

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

		spider_engine::rendering::Camera& getCamera() {
			return *camera_;
		}
	};
}