#include <iostream>
#include <Windows.h>
#include <thread>

#include "core_engine.hpp"
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
using namespace spider_engine::core_engine;

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
    const wchar_t CLASS_NAME[] = L"DX12WindowClass";

    WNDCLASS wc      = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"DirectX 12 Triangle", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 720, nullptr, nullptr, hInstance, nullptr
    );
    ShowWindow(hwnd, nCmdShow);

    CoreEngine core_engine;
	core_engine.intitializeRenderingSystems(hwnd, 2, false, false, 0);
	core_engine.initializeDebugSystems(true, true, true);
    
	DX12Renderer& renderer = core_engine.getRenderer();
	DX12Compiler& compiler = core_engine.getCompiler();

    FastArray<ShaderDescription> descriptions = { 
        { vertexShaderSrc, ShaderStage::STAGE_VERTEX }, { pixelShaderSrc, ShaderStage::STAGE_PIXEL }
    };
	RenderPipeline pipeline = compiler.createRenderPipeline<UseSourcePolicy>(descriptions);

    Renderizable renderizable = renderer.createRenderizable(cubeVertices, indices);
	flecs::entity entity = core_engine.createEntity("Cube");
	core_engine.getWorld().entity(entity).set<Renderizable>(std::move(renderizable));

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

        renderer.beginFrame();
        renderer.draw(entity, pipeline, camera);
        renderer.endFrame();
        renderer.present();
    }

    return 0;
}