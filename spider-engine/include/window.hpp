#pragma once

#define IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS
#include <string>
#include <windows.h>
#include <windowsx.h>
#include <functional>
#include <thread>
#include <future>
#include <memory>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "definitions.hpp"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

	switch (msg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_SIZE:
			return 0;

		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}

namespace spider_engine::core_engine {
	class Window {
	private:
		HWND hwnd_;

		std::wstring title_;
		std::wstring windowClassName_;
			
		uint_t width_;
		uint_t height_;

		uint_t x_;
		uint_t y_;

	public:
		friend class CoreEngine;

		std::atomic<bool> isRunning = true;

		Window() = default;
		Window(const std::wstring& title, 
			   const uint_t        width, 
			   const uint_t        height,
			   const uint_t        x,
			   const uint_t        y,
			   const std::wstring  windowClassName = L"SpiderEngineMainWindowClass") :
			title_(title), 
			windowClassName_(windowClassName),
			width_(width), 
			height_(height),
			x_(x),
			y_(y),
			isRunning(true)
		{
			HINSTANCE hInstance = GetModuleHandle(nullptr);

			if (width == 0) {
				width_ = GetSystemMetrics(SM_CXSCREEN) / 2;
			}
			if (height == 0) {
				height_ = GetSystemMetrics(SM_CYSCREEN) / 2;
			}

			if (x == 0) {
				x_ = (GetSystemMetrics(SM_CXSCREEN) - width_) / 2;
			}
			if (y == 0) {
				y_ = (GetSystemMetrics(SM_CYSCREEN) - height_) / 2;
			}

			RECT rect = { 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };

			int adjustedWidth  = rect.right - rect.left;
			int adjustedHeight = rect.bottom - rect.top;

			WNDCLASSEXW wc   = {};
			wc.cbSize        = sizeof(WNDCLASSEXW);
			wc.style		 = CS_HREDRAW | CS_VREDRAW;
			wc.lpfnWndProc   = WindowProc;
			wc.hInstance     = hInstance;
			wc.hCursor       = LoadCursor(nullptr, IDC_ARROW); 
			wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wc.lpszClassName = windowClassName_.c_str();
			if (!RegisterClassExW(&wc)) {
				throw std::runtime_error("RegisterClassExW failed");
			}

			hwnd_ = CreateWindowExW(
				0,
				windowClassName_.c_str(),
				title_.c_str(),
				WS_OVERLAPPEDWINDOW,
				x_, y_,
				adjustedWidth, adjustedHeight,
				nullptr,
				nullptr,
				hInstance,
				nullptr
			);

			if (!hwnd_) {
				DWORD err = GetLastError();
				throw std::runtime_error("CreateWindowExW failed, error code: " + std::to_string(err));
			}

			ShowWindow(hwnd_, SW_SHOWDEFAULT);
		}
		Window(const Window& other) = delete;
		Window(Window&& other) noexcept :
			hwnd_(other.hwnd_),
			title_(other.title_),
			windowClassName_(other.windowClassName_),
			width_(other.width_),
			height_(other.height_),
			x_(other.x_),
			y_(other.y_),
			isRunning(other.isRunning.load())
		{
			other.hwnd_            = nullptr;
			other.windowClassName_ = L"";
		}

		~Window() {
			if (hwnd_) {
				DestroyWindow(hwnd_);
			}
			isRunning = false;
		}

		void setWidth(const uint_t width) {
			width_ = width;
			SetWindowPos(hwnd_, nullptr, x_, y_, width_, height_, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		void setHeight(const uint_t height) {
			height_ = height;
			SetWindowPos(hwnd_, nullptr, x_, y_, width_, height_, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		void setX(const uint_t x) {
			x_ = x;
			SetWindowPos(hwnd_, nullptr, x_, y_, width_, height_, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		void setY(const uint_t y) {
			y_ = y;
			SetWindowPos(hwnd_, nullptr, x_, y_, width_, height_, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		void setTitle(const std::wstring& title) {
			title_ = title;
			SetWindowTextW(hwnd_, title.c_str());
		}
		void setClassName(const std::wstring& className) {
			windowClassName_ = className;
		}

		uint_t getWidth() const {
			return width_;
		}
		uint_t getHeight() const {
			return height_;
		}

		uint_t getX() const {
			return x_;
		}
		uint_t getY() const {
			return y_;
		}

		HWND getHWND() const {
			return hwnd_;
		}
		std::wstring_view getTitle() const {
			return title_;
		}
		std::wstring_view getClassName() const {
			return windowClassName_;
		}

		Window& operator=(const Window& other) = delete;
		Window& operator=(Window&& other) noexcept {
			if (this != &other) {
				this->hwnd_			   = other.hwnd_;
				this->title_		   = other.title_;
				this->windowClassName_ = other.windowClassName_;
				this->width_		   = other.width_;
				this->height_		   = other.height_;
				this->x_			   = other.x_;
				this->y_			   = other.y_;

				other.hwnd_            = nullptr;
				other.windowClassName_ = L"";
			}
			return *this;
		}
	};
}