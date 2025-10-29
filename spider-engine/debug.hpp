#pragma once
#include <iostream>
#include <print>
#include <exception>
#include <Windows.h>

namespace spider_engine {
	enum class DebugLevel {
		Info,
		Warning,
		Error,
		Fatal
	};

	class DebugConsole {
	private:
		static HANDLE handle_;
		static size_t instanceCount_;

		DebugConsole() {
			++instanceCount_;
			if (handle_) return;

			AllocConsole();
			FILE* fp;
			freopen_s(&fp, "CONOUT$", "w", stdout);
			freopen_s(&fp, "CONOUT$", "w", stderr);
			freopen_s(&fp, "CONIN$", "r", stdin);

			std::ios::sync_with_stdio();

			handle_ = GetStdHandle(STD_OUTPUT_HANDLE);
		}

		~DebugConsole() {
			--instanceCount_;
			if (instanceCount_ > 0) return;
			FreeConsole();
		}

		static void setColor(uint16_t color) {
			SetConsoleTextAttribute(handle_, color);
		}

	public:
		static HANDLE getHandle() {
			return handle_;
		}

		template <typename... Args>
		static void print(const std::string& message, DebugLevel dbgLevel = DebugLevel::Info, Args... args) {
			switch (dbgLevel)
			{
			case spider_engine::DebugLevel::Info:
				DebugConsole::setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
				break;
			case spider_engine::DebugLevel::Warning:
				DebugConsole::setColor(FOREGROUND_RED | FOREGROUND_GREEN);
				break;
			case spider_engine::DebugLevel::Error:
				DebugConsole::setColor(FOREGROUND_RED);
				break;
			case spider_engine::DebugLevel::Fatal:
				DebugConsole::setColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
				std::cout << std::format("{}", std::format("{}", message).c_str(), args...);
				throw std::runtime_error("Fatal error: " + message);
				break;
			default:
				break;
			}
			std::cout << std::format("{}", std::format("{}", message).c_str(), args...);
		}
		template <typename... Args>
		static void println(const std::string& message, DebugLevel dbgLevel = DebugLevel::Info, Args... args) {
			switch (dbgLevel)
			{
			case spider_engine::DebugLevel::Info:
				DebugConsole::setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
				break;
			case spider_engine::DebugLevel::Warning:
				DebugConsole::setColor(FOREGROUND_RED | FOREGROUND_GREEN);
				break;
			case spider_engine::DebugLevel::Error:
				DebugConsole::setColor(FOREGROUND_RED);
				break;
			case spider_engine::DebugLevel::Fatal:
				DebugConsole::setColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
				std::cout << std::format("{}\n", std::format("{}", message).c_str(), args...);
				throw std::runtime_error("Fatal error: " + message);
				break;
			default:
				break;
			}
			std::cout << std::format("{}\n", std::format("{}", message).c_str(), args...);
		}
	};
	HANDLE DebugConsole::handle_        = nullptr;
	size_t DebugConsole::instanceCount_ = 0;
}