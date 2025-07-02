#pragma once
#include <cassert>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxc/dxcapi.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#include "d3dx12.h"

#include "fast_array.hpp"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxcompiler.lib")

#ifdef NDEBUG
#define assert(expr) expr
#endif

namespace spider_engine {
	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

	enum class ShaderStage {
		Vertex,
		Pixel,
	};

	struct VertexArrayBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexArrayBuffer;
		D3D12_VERTEX_BUFFER_VIEW			   vertexArrayBufferView;
	};
	struct IndexArrayBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> indexArrayBuffer;
		D3D12_INDEX_BUFFER_VIEW			       indexArrayBufferView;
	};

	struct Mesh {
		uint8_t* data;
		uint8_t  size;

		~Mesh() {
			switch (size)
			{
			case 1:
				reinterpret_cast<VertexArrayBuffer*>(data)->~VertexArrayBuffer();
				break;
			case 2:
				reinterpret_cast<VertexArrayBuffer*>(data)->~VertexArrayBuffer();
				reinterpret_cast<IndexArrayBuffer*>(data + sizeof(VertexArrayBuffer))->~IndexArrayBuffer();
				break;

			default:
				throw std::runtime_error("Mesh has invalid components!");
				break;
			}

			delete[] data;
		}
	};

	struct RenderPipeline {
		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		D3D12_RESOURCE_BARRIER                      barrier;
	};

	const D3D12_INPUT_ELEMENT_DESC psInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	template <typename uint8_t bufferCount_ = 2>
	class Renderer {
	private:
		template <typename Ty>
		using ComPtr = Microsoft::WRL::ComPtr<Ty>;

		HWND hwnd_;

		ComPtr<ID3D12Device>  device_; 
		ComPtr<IDXGIFactory7> factory_;

		ComPtr<ID3D12CommandAllocator>    commandAllocator_;
		ComPtr<ID3D12CommandQueue>	      commandQueue_;
		ComPtr<ID3D12GraphicsCommandList> commandList_;

		ComPtr<IDXGISwapChain4>      swapChain_;
		ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		ComPtr<ID3D12Resource>		 backBuffers_[bufferCount_];

		ComPtr<IDxcUtils>          compilerUtils_;
		ComPtr<IDxcCompiler3>      compiler_;
		ComPtr<IDxcIncludeHandler> compilerIncludeHandler_;

		ComPtr<ID3D12Fence> fence_;

		UINT frameIndex_;

		BOOL isFullScreen_;
		BOOL isVSync_;

	public:
		Renderer(HWND hwnd, BOOL isFullScreen = FALSE, BOOL isVSync = TRUE, uint8_t deviceId = 0) : 
			isFullScreen_(isFullScreen), isVSync_(isVSync), hwnd_(hwnd), frameIndex_(0) {
			// Initialize factory
			assert(CreateDXGIFactory1(IID_PPV_ARGS(&factory_)) == S_OK);

			// Get a list of adapters
			ComPtr<IDXGIAdapter1> adapter;
			factory_->EnumAdapters1(deviceId, &adapter);

			// Create the device using a specific adapter
			assert(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)) == S_OK);

			// Create command allocator, queue, and list
			this->createCommandAllocatorQueueAndList();

			this->createSwapChain();
			this->createRTVs();

			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&compilerUtils_));
			DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
			compilerUtils_->CreateDefaultIncludeHandler(&compilerIncludeHandler_);
		}
		Renderer(HWND hwnd, bool isFullScreen = false, bool isVSync = true, uint8_t deviceId = 0) : 
			isFullScreen_(isFullScreen == true), isVSync_(isVSync == true), hwnd_(hwnd), frameIndex_(0) {
			// Initialize factory
			assert(CreateDXGIFactory1(IID_PPV_ARGS(&factory_)) == S_OK);

			// Get a list of adapters
			ComPtr<IDXGIAdapter1> adapter;
			factory_->EnumAdapters1(deviceId, &adapter);

			// Create the device using a specific adapter
			assert(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)) == S_OK);

			// Create command allocator, queue, and list
			this->createCommandAllocatorQueueAndList();

			this->createSwapChain();
			this->createRTVs();

			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&compilerUtils_));
			DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
			compilerUtils_->CreateDefaultIncludeHandler(&compilerIncludeHandler_);
		}

		void createCommandAllocatorQueueAndList() {
			// Create command allocator
			assert(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_)) == S_OK);

			// Create command queue
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Type					   = D3D12_COMMAND_LIST_TYPE_DIRECT;
			queueDesc.Flags					   = D3D12_COMMAND_QUEUE_FLAG_NONE;
			assert(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)) == S_OK);

			// Create command list
			assert(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_)) == S_OK);
		}

		void createSwapChain() {
			// Use a temporary swap chain
			ComPtr<IDXGISwapChain1> tempSwapChain;

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.BufferCount			= bufferCount_;
			swapChainDesc.BufferUsage			= DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.SampleDesc.Count		= 1;
			swapChainDesc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.Flags				    = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
			assert(factory_->CreateSwapChainForHwnd(
				commandQueue_.Get(),
				hwnd_, 
				&swapChainDesc, 
				nullptr, 
				nullptr, 
				&tempSwapChain
			) == S_OK);
			tempSwapChain.As(&swapChain_);
		}

		void createRTVs() {
			// Create descriptor heap for RTV
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors             = bufferCount_;
			rtvHeapDesc.Type					   = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_));

			// Get cpu descriptor handle and increment size
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
			UINT rtvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			// Create back buffers and RTV
			for (UINT i = 0; i < bufferCount_; ++i) {
				assert(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])) == S_OK);
				device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(rtvDescriptorSize);
			}
		}

		VertexArrayBuffer createVertexBuffer(const FastArray<Vertex>& vertices) {
			VertexArrayBuffer vertexArrayBuffer = {};

			const size_t bufferSize = sizeof(Vertex) * vertices.size();

			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

			device_->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&vertexArrayBuffer.vertexArrayBuffer)
			);

			UINT8* vertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			vertexArrayBuffer.vertexArrayBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
			memcpy(vertexDataBegin, vertices.data(), bufferSize);
			vertexArrayBuffer.vertexArrayBuffer->Unmap(0, nullptr);

			vertexArrayBuffer.vertexArrayBufferView.BufferLocation = vertexArrayBuffer.vertexArrayBuffer->GetGPUVirtualAddress();
			vertexArrayBuffer.vertexArrayBufferView.StrideInBytes  = sizeof(Vertex);
			vertexArrayBuffer.vertexArrayBufferView.SizeInBytes    = bufferSize;

			return vertexArrayBuffer;
		}

		IndexArrayBuffer createIndexArrayBuffer(const FastArray<uint32_t>& indices) {
			IndexArrayBuffer indexArrayBuffer = {};

			const size_t bufferSize = sizeof(uint32_t) * indices.size();

			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

			device_->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&indexArrayBuffer.indexArrayBuffer)
			);

			UINT8* indexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			indexArrayBuffer.indexArrayBuffer->Map(0, &readRange, reinterpret_cast<void**>(&indexDataBegin));
			memcpy(indexDataBegin, indices.data(), bufferSize);
			indexArrayBuffer.indexArrayBuffer->Unmap(0, nullptr);

			indexArrayBuffer.indexArrayBufferView.BufferLocation = indexArrayBuffer.indexArrayBuffer->GetGPUVirtualAddress();
			indexArrayBuffer.indexArrayBufferView.Format         = DXGI_FORMAT_R32_UINT;
			indexArrayBuffer.indexArrayBufferView.SizeInBytes    = bufferSize;

			return indexArrayBuffer;
		}

		Mesh createMesh(const FastArray<Vertex>& vertices) {
			Mesh mesh;
			const size_t totalSize = sizeof(VertexArrayBuffer);
			mesh.data = new uint8_t[totalSize];
			mesh.size = 1;

			new (mesh.data) VertexArrayBuffer(createVertexBuffer(vertices));

			return mesh;
		}
		Mesh createMesh(const FastArray<Vertex>& vertices, const FastArray<uint32_t>& indices) {
			Mesh mesh;
			const size_t totalSize = sizeof(VertexArrayBuffer) + sizeof(IndexArrayBuffer);
			mesh.data			   = new uint8_t[totalSize];
			mesh.size			   = 2;

			new (mesh.data) VertexArrayBuffer(createVertexBuffer(vertices));
			new (mesh.data + sizeof(VertexArrayBuffer)) IndexArrayBuffer(createIndexArrayBuffer(indices));

			return mesh;
		}
		Mesh createMesh(const VertexArrayBuffer& vertices) {
			Mesh mesh;
			const size_t totalSize = sizeof(VertexArrayBuffer);
			mesh.data = new uint8_t[totalSize];
			mesh.size = 1;

			new (mesh.data) VertexArrayBuffer(vertices);

			return mesh;
		}
		Mesh createMesh(VertexArrayBuffer&& vertices) {
			Mesh mesh;
			const size_t totalSize = sizeof(VertexArrayBuffer);
			mesh.data = new uint8_t[totalSize];
			mesh.size = 1;

			new (mesh.data) VertexArrayBuffer(std::move(vertices));

			return mesh;
		}
		Mesh createMesh(const VertexArrayBuffer& vertices, const IndexArrayBuffer& indices) {
			Mesh mesh;
			const size_t totalSize = sizeof(VertexArrayBuffer) + sizeof(IndexArrayBuffer);
			mesh.data			   = new uint8_t[totalSize];
			mesh.size			   = 2;

			new (mesh.data) VertexArrayBuffer(vertices);
			new (mesh.data + sizeof(VertexArrayBuffer)) IndexArrayBuffer(indices);

			return mesh;
		}
		Mesh createMesh(VertexArrayBuffer&& vertices, IndexArrayBuffer&& indices) {
			Mesh mesh;
			const size_t totalSize = sizeof(VertexArrayBuffer) + sizeof(IndexArrayBuffer);
			mesh.data			   = new uint8_t[totalSize];
			mesh.size			   = 2;

			new (mesh.data) VertexArrayBuffer(vertices);
			new (mesh.data + sizeof(VertexArrayBuffer)) IndexArrayBuffer(std::move(indices));

			return mesh;
		}

		ComPtr<ID3DBlob> compileShader(const wchar_t* path, const wchar_t* target) {
			assert(compilerUtils_ != nullptr);
			assert(compiler_ != nullptr);
			assert(compilerIncludeHandler_ != nullptr);

			ComPtr<IDxcBlobEncoding> sourceBlob;
			assert(compilerUtils_->LoadFile(path, nullptr, &sourceBlob) == S_OK);

			ComPtr<IDxcOperationResult> resultBuff;

			// Prepare compiler arguments
			LPCWSTR args[] = {
				L"-E", L"main",             // Entry point
				L"-T", target,           // Target profile
				L"-Zi",                     // Debug info
				L"-Qembed_debug",           // Embed debug info
				L"-Zpr",                    // Row major
			};

			DxcBuffer sourceBuffer = {};
			sourceBuffer.Ptr	   = sourceBlob->GetBufferPointer();
			sourceBuffer.Size      = sourceBlob->GetBufferSize();
			sourceBuffer.Encoding  = DXC_CP_UTF8;

			assert(compiler_->Compile(
				&sourceBuffer,
				args,
				_countof(args),
				compilerIncludeHandler_.Get(),
				IID_PPV_ARGS(&resultBuff)
			) == S_OK);

			ComPtr<IDxcBlob> shaderBlob;
			resultBuff->GetResult(&shaderBlob);

			ComPtr<ID3DBlob> blob;
			assert(D3DCreateBlob(shaderBlob->GetBufferSize(), &blob) == S_OK);
			memcpy(blob->GetBufferPointer(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

			return blob;
		}
		ComPtr<ID3DBlob> compileShader(const char* source, const wchar_t* target) {
			assert(compilerUtils_ != nullptr);
			assert(compiler_ != nullptr);
			assert(compilerIncludeHandler_ != nullptr);

			ComPtr<IDxcOperationResult> resultBuff;
			
			// Prepare compiler arguments
			LPCWSTR args[] = {
				L"-E", L"main",             // Entry point
				L"-T", target,				// Target profile
				L"-Zi",                     // Debug info
				L"-Qembed_debug",           // Embed debug info
				L"-Zpr",                    // Row major
			};

			DxcBuffer sourceBuffer = {};
			sourceBuffer.Ptr	   = source;
			sourceBuffer.Size      = strlen(source);
			sourceBuffer.Encoding  = DXC_CP_UTF8;

			assert(compiler_->Compile(
				&sourceBuffer,
				args,
				_countof(args),
				compilerIncludeHandler_.Get(),
				IID_PPV_ARGS(&resultBuff)
			) == S_OK);

			// Check for errors
			HRESULT hrStatus;
			resultBuff->GetStatus(&hrStatus);
			if (FAILED(hrStatus)) {
				ComPtr<IDxcBlobUtf8> errors;
				resultBuff->G(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
				if (errors && errors->GetStringLength() > 0) {
					printf("Shader compilation failed:\n%s\n", errors->GetStringPointer());
				}
				else {
					printf("Shader compilation failed with unknown error.\n");
				}
				return nullptr;
			}

			ComPtr<IDxcBlob> shaderBlob;
			assert(resultBuff->GetResult(&shaderBlob) == S_OK);

			ComPtr<ID3DBlob> blob;
			assert(D3DCreateBlob(shaderBlob->GetBufferSize(), &blob) == S_OK);
			memcpy(blob->GetBufferPointer(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

			return blob;
		}

		RenderPipeline createRenderPipeline(const char* vertexShader, const char* pixelShader) {
			RenderPipeline renderPipeline = {};

			ComPtr<ID3DBlob> errorBlob;

			// Compile vertex and pixel shader
			renderPipeline.vertexShader = compileShader(vertexShader, L"vs_6_0");
			renderPipeline.pixelShader = compileShader(pixelShader, L"ps_6_0");

			// Serialize root signature
			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> rootSignatureBlob;

			assert(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorBlob) == S_OK);

			// Create root signature
			assert(device_->CreateRootSignature(
				0,
				rootSignatureBlob->GetBufferPointer(),
				rootSignatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&renderPipeline.rootSignature)
			) == S_OK);

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout						   = { psInputLayout, _countof(psInputLayout) };
			psoDesc.pRootSignature					   = renderPipeline.rootSignature.Get();
			psoDesc.VS								   = CD3DX12_SHADER_BYTECODE(renderPipeline.vertexShader.Get());
			psoDesc.PS								   = CD3DX12_SHADER_BYTECODE(renderPipeline.pixelShader.Get());
			psoDesc.RasterizerState					   = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState						   = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState				   = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.SampleMask						   = UINT_MAX;
			psoDesc.NodeMask						   = 0;
			psoDesc.PrimitiveTopologyType			   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets				   = 1;
			psoDesc.RTVFormats[0]					   = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count				   = 1;

			// Create the pipeline state object
			assert(device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderPipeline.pipelineState)) == S_OK);

			return renderPipeline;
		}
		RenderPipeline createRenderPipeline(const wchar_t* vertexShader, const wchar_t* pixelShader) {
			RenderPipeline renderPipeline = {};

			ComPtr<ID3DBlob> errorBlob;

			// Compile vertex and pixel shader
			renderPipeline.vertexShader = compileShader(vertexShader, L"vs_6_0");
			renderPipeline.pixelShader  = compileShader(pixelShader, L"ps_6_0");

			// Serialize root signature
			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> rootSignatureBlob;

			D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorBlob);

			// Create root signature
			assert(device_->CreateRootSignature(
				0,
				rootSignatureBlob->GetBufferPointer(),
				rootSignatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&renderPipeline.rootSignature)
			) == S_OK);

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout						   = { psInputLayout, _countof(psInputLayout) };
			psoDesc.pRootSignature					   = renderPipeline.rootSignature.Get();
			psoDesc.VS								   = CD3DX12_SHADER_BYTECODE(renderPipeline.vertexShader.Get());
			psoDesc.PS								   = CD3DX12_SHADER_BYTECODE(renderPipeline.pixelShader.Get());
			psoDesc.RasterizerState					   = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState						   = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState				   = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.SampleMask						   = UINT_MAX;
			psoDesc.NodeMask						   = 0;
			psoDesc.PrimitiveTopologyType			   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets				   = 1;
			psoDesc.RTVFormats[0]					   = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count				   = 1;

			// Create the pipeline state object
			assert(device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderPipeline.pipelineState)) == S_OK);

			return renderPipeline;
		}

		void draw(RenderPipeline& pipeline, Mesh& mesh) {
			VertexArrayBuffer* vertexArrayBuffer;
			IndexArrayBuffer* indexArrayBuffer;
			switch (mesh.size)
			{
			case 1:
				vertexArrayBuffer = reinterpret_cast<VertexArrayBuffer*>(mesh.data);
				break;
			case 2:
				vertexArrayBuffer = reinterpret_cast<VertexArrayBuffer*>(mesh.data);
				indexArrayBuffer  = reinterpret_cast<IndexArrayBuffer*>(mesh.data + sizeof(IndexArrayBuffer));
				break;

			default:
				throw std::runtime_error("Mesh has invalid components!");
				break;
			}

			auto& barrier = pipeline.barrier;

			commandAllocator_->Reset();
			commandList_->Reset(commandAllocator_.Get(), pipeline.pipelineState.Get());

			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffers_[frameIndex_].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			commandList_->ResourceBarrier(1, &barrier);

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
				rtvHeap_->GetCPUDescriptorHandleForHeapStart(),
				frameIndex_,
				device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
			);
			
			commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

			const float clearColor[] = { 0.1f, 0.1f, 0.4f, 1.0f };
			commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
			commandList_->SetGraphicsRootSignature(pipeline.rootSignature.Get());

			CD3DX12_VIEWPORT viewport(0.0f, 0.0f, 1280.0f, 720.0f);
			CD3DX12_RECT rect(0, 0, 1280, 720);
			commandList_->RSSetViewports(1, &viewport);
			commandList_->RSSetScissorRects(1, &rect);

			commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList_->IASetVertexBuffers(0, 1, &vertexArrayBuffer->vertexArrayBufferView);

			commandList_->SetPipelineState(pipeline.pipelineState.Get());
			commandList_->DrawInstanced(3, 1, 0, 0);

			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffers_[frameIndex_].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT
			);
			commandList_->ResourceBarrier(1, &barrier);

			commandList_->Close();
			ID3D12CommandList* ppCommandLists[] = { commandList_.Get() };
			commandQueue_->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		}

		void present() {
			swapChain_->Present(isVSync_, isVSync_ ? NULL : DXGI_PRESENT_ALLOW_TEARING);
			frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
		}

		void setFullScreen(bool enabled) {
			assert(swapChain_ != nullptr);
			assert(factory_ != nullptr);

			if (enabled) {
				assert(swapChain_->SetFullscreenState(TRUE, nullptr) == S_OK);
			}
			else {
				assert(swapChain_->SetFullscreenState(FALSE, nullptr) == S_OK);
			}
		}
		void setFullScreen(BOOL enabled) {
			assert(swapChain_ != nullptr);
			assert(factory_ != nullptr);

			assert(swapChain_->SetFullscreenState(enabled, nullptr) == S_OK);
			isFullScreen_ = enabled;
		}
		bool isFullScreenBool() const {
			return isFullScreen_ == TRUE;
		}
		BOOL isFullScreen() const {
			return isFullScreen_;
		}

		void setVSync(bool enabled) {
			isVSync_ = enabled ? TRUE : FALSE;
		}
		void setVSync(BOOL enabled) {
			isVSync_ = enabled;
		}
		bool isVSyncBool() const {
			return isVSync_ == TRUE;
		}
		BOOL isVSync() const {
			return isVSync_;
		}
	};
}