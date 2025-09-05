#include <iostream>
#include <Windows.h>

#include "dx12_renderer.hpp"
#include "camera.hpp"

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
cbuffer frameData : register(b0)
{
    float4x4 projection;
    float4x4 view;
    float4x4 model;
    float4   cameraPosition;
};

struct VSInput {
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput o;

    float4 worldPos = mul(float4(input.position, 1.0), model);
    float4 viewPos  = mul(worldPos, view);
    o.position      = mul(viewPos, projection);
    o.color         = input.color;

    return o;
}
)";

std::wstring pixelShaderSrc = LR"(
struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

float4 main(PSInput input) : SV_TARGET {
    return input.color;
}
)";

using namespace spider_engine;
using namespace spider_engine::d3dx12;
using namespace spider_engine::rendering;

FastArray<Vertex> cubeVertices = {
    { {  0.0f,  0.5f, 0.0f }, { 1,0,0,1 } },
    { {  0.5f, -0.5f, 0.0f }, { 0,1,0,1 } },
    { { -0.5f, -0.5f, 0.0f }, { 0,0,1,1 } },
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

    std::ios::sync_with_stdio();

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
    FastArray<ShaderDescription> descriptions = { { vertexShaderSrc, ShaderStage::STAGE_VERTEX }, { pixelShaderSrc, ShaderStage::STAGE_PIXEL } };
	RenderPipeline pipeline                   = compiler.createRenderPipeline<UseSourcePolicy>(descriptions);

    Camera camera(1200, 720);
	camera.transform.position = DirectX::XMVectorSet(0.0f, 0.0f, -2.0f, 1.0f);

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

        camera.updateViewMatrix();
		camera.updateProjectionMatrix();

        FrameData frameData  = {};
        frameData.projection = camera.getProjectionMatrix();
        frameData.view       = camera.getViewMatrix();
        frameData.model      = DirectX::XMMatrixIdentity();
        XMStoreFloat4(&frameData.cameraPosition, camera.transform.position);
        pipeline.bindBuffer<>("frameData", ShaderStage::STAGE_VERTEX, frameData);

        renderer.beginFrame();
        renderer.draw(pipeline, mesh);
        renderer.endFrame();
        renderer.present();
    }

    return 0;
}