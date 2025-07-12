#include <iostream>
#include <Windows.h>

#include "dx12_renderer.hpp"

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
std::wstring vertexShaderSrc = LR"(

struct VSInput {
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    return output;
}
)";

std::wstring pixelShaderSrc = LR"(
cbuffer colorData : register(b0)
{
    float4 colorr;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

float4 main(PSInput input) : SV_TARGET {
    return colorr;
}
)";

using namespace spider_engine;
using namespace spider_engine::d3dx12;

FastArray<Vertex> cubeVertices = {
    { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, // Top (vermelho)
    { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // Right (verde)
    { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }, // Left (azul)
};

FastArray<uint32_t> indices = {
    0, 1, 2
};


template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    AllocConsole();

    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);  // Redirect stdout
    freopen_s(&fp, "CONOUT$", "w", stderr);  // Redirect stderr
    freopen_s(&fp, "CONIN$", "r", stdin);    // Redirect stdin

    // Sync C++ streams with C I/O
    std::ios::sync_with_stdio();

    // Optional: clear error flags
    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();

    const wchar_t CLASS_NAME[] = L"DX12WindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"DirectX 12 Triangle", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 720, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);

    DX12Renderer renderer(hwnd, 2, false, false, 0);
    DX12Compiler compiler(renderer);
    
	Mesh mesh                                 = renderer.createMesh(cubeVertices, indices);
    FastArray<ShaderDescription> descriptions = { { vertexShaderSrc, ShaderStage::STAGE_VERTEX}, { pixelShaderSrc, ShaderStage::STAGE_PIXEL } };
	RenderPipeline pipeline                   = compiler.createRenderPipeline<UseSourcePolicy>(descriptions);

    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pipeline.bindBuffer("colorData", ShaderStage::STAGE_PIXEL, color);

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

        std::cout << "Requirement name: " << std::get<0>(pipeline.getRequirements()[0]) << std::endl;

        renderer.beginFrame();
        renderer.draw(pipeline, mesh);
        renderer.endFrame();
        renderer.present();
    }

    return 0;
}