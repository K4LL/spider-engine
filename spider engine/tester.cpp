#include <iostream>
#include <Windows.h>

#include "renderer.hpp"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

bool isButtonDown(int vkey) {
    static bool prev[256] = {};

    bool down = (GetAsyncKeyState(vkey) & 0x8000) != 0;
    bool pressed = down && !prev[vkey];

    prev[vkey] = down;
    return pressed;
}

// Shader Source Code
const char* vertexShaderSrc = R"(
// vertex.hlsl
struct VSInput { float3 pos : POSITION; };
struct VSOutput { float4 pos : SV_POSITION; };
VSOutput main(VSInput input) {
    VSOutput output;
    output.pos = float4(input.pos, 1.0f);
    return output;
}
)";

const char* pixelShaderSrc = R"(
// pixel.hlsl
float4 main() : SV_TARGET {
    return float4(1, 0, 0, 1); // red
}
)";

using namespace spider_engine;

FastArray<Vertex> triangleVertices = {
    { {  0.0f,  0.5f, 0.0f } },
    { { 0.5f, -0.5f, 0.0f } },
    { { -0.5f, -0.5f, 0.0f } },
};

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    AllocConsole();

    const wchar_t CLASS_NAME[] = L"DX12WindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"DirectX 12 Triangle", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 720, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);

    Renderer<2> renderer(hwnd, 0, 1, 0);

	Mesh mesh               = renderer.createMesh(triangleVertices);
	RenderPipeline pipeline = renderer.createRenderPipeline(vertexShaderSrc, pixelShaderSrc);

    // Message Loop
    MSG msg = {};
    while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

        if (isButtonDown(VK_F11)) {
			renderer.setFullScreen(!renderer.isFullScreen());
        }

        renderer.draw(pipeline, mesh);
        renderer.present();
    }

    return 0;
}
