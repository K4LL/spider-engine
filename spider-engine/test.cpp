#include <iostream>
#include <Windows.h>
#include <thread>

#include "core_engine.hpp"
#include "camera.hpp"

// Shader Source Code
std::wstring vertexShaderSrc = LR"(
cbuffer frameData : register(b0)
{
    float4x4 projection;
    float4x4 view;
    float4x4 model;
};

struct VSInput {
    float3 pos      : POSITION;
    float3 norm     : NORMAL;
    float2 uv       : TEXCOORD0;
    float3 tangent  : TANGENT;
};

struct VSOutput {
    float4 pos  : SV_POSITION;
    float3 norm : NORMAL;
    float2 uv   : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput o;

    float4 worldPos = mul(float4(input.pos, 1.0), model);
    float4 viewPos  = mul(worldPos, view);
    o.pos           = mul(viewPos, projection);

    o.norm = input.norm;
    o.uv   = input.uv;

    return o;
}
)";

std::wstring pixelShaderSrc = LR"(
Texture2D myTexture : register(t0);
SamplerState mySampler : register(s0);

struct PSInput {
    float4 pos  : SV_POSITION;
    float3 norm : NORMAL;
    float2 uv   : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    // Simple texture lookup
    float4 color = myTexture.Sample(mySampler, input.uv);

    return color;
}
)";

using namespace spider_engine;
using namespace spider_engine::d3dx12;
using namespace spider_engine::rendering;
using namespace spider_engine::core_engine;

std::vector<Vertex> cubeVertices = {
    {{0,0,0}, {0,0,1}, {0,0}},
    {{1,0,0}, {0,0,1}, {1,0}},
    {{1,1,0}, {0,0,1}, {1,1}},
    {{0,1,0}, {0,0,1}, {0,1}},
};

std::vector<uint32_t> indices = {
    0, 1, 2
};


template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	RenderingSystemDescription renderingSystemDescription;
	renderingSystemDescription.windowName = L"Spider Engine DX12 Test Window";

    CoreEngine coreEngine;
    coreEngine.initializeDebugSystems(true, true, true);
	coreEngine.intitializeRenderingSystems(renderingSystemDescription);
    
	DX12Renderer& renderer = coreEngine.getRenderer();
	DX12Compiler& compiler = coreEngine.getCompiler();

	Texture2D texture = renderer.createTexture2D(L"C:\\Users\\gupue\\source\\repos\\spider engine\\texture.png");

    std::vector<ShaderDescription> descriptions;

    ShaderDescription vertex(vertexShaderSrc, ShaderStage::STAGE_VERTEX);
    ShaderDescription pixel(pixelShaderSrc, ShaderStage::STAGE_PIXEL);

    descriptions.push_back(std::move(vertex));
    descriptions.push_back(std::move(pixel));

	RenderPipeline pipeline = compiler.createRenderPipeline<UseSourcePolicy>(descriptions);

    pipeline.bindShaderResourceForTexture2D("myTexture", ShaderStage::STAGE_PIXEL, texture);

    Renderizable  renderizable = renderer.createRenderizable(L"C:\\Users\\gupue\\source\\repos\\spider engine\\Wolf_obj.obj");
	flecs::entity entity      = coreEngine.createEntity("Cube");
    coreEngine.getWorld().entity(entity).set<Renderizable>(std::move(renderizable));
    Camera& camera = coreEngine.getCamera();
    camera.transform.position = DirectX::XMVectorSet(0.0f, 0.0f, -20.0f, 1.0f);

    auto updateLoop = [&]() {
        if (isButtonDown(VK_F11)) {
            //renderer.setFullScreen(!renderer.isFullScreen());
        }

        if (isButtonDown(VK_F11)) {
            camera.transform.position = DirectX::XMVectorSubtract(
                camera.transform.position,
                DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f)
            );

            DirectX::XMFLOAT3 c;
            DirectX::XMStoreFloat3(&c, camera.transform.position);
            std::cout << "Camera x: " << c.x << "y: " << c.y << "z" << c.z << std::endl;
        }

        camera.updateViewMatrix();
        camera.updateProjectionMatrix();

        renderer.beginFrame();
        renderer.draw(entity, pipeline, camera);
        renderer.endFrame();
        renderer.present();
    };

	coreEngine.start(updateLoop);

    return 0;
}