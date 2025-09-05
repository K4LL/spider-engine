#pragma once
// STL, DirectX and Windows includes
#include <print>
#include <cassert>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <wrl/client.h>
#include <comdef.h>
#include <unordered_map>

// DirectX Helper includes
#include "d3dx12.h"
#include "dxcapi.h"
#include "d3d12shader.h"

// Framework includes
#include "fast_array.hpp"
#include "definitions.hpp"
#include "policies.hpp"

// DirectX 12 Types include
#include "dx12_types.hpp"

// Link DirectX libraries
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace spider_engine::d3dx12 {
	// Simple syncronization object for CPU-GPU sync
	class SynchronizationObject {
	public:
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		uint64_t*					values_;
		uint64_t					currentValue_;

		HANDLE* handles_;

		size_t bufferCount_;

		SynchronizationObject() = default;
		SynchronizationObject(ID3D12Device* device, 
						  const size_t  bufferCount) : 
			currentValue_(0),
			bufferCount_(bufferCount)
		{
			device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));

			// Set values and its handles
			values_  = new uint64_t[bufferCount_]();
			handles_ = new HANDLE[bufferCount_];

			// Populate handles
			for (auto it = handles_; it != handles_ + bufferCount_; ++it) {
				*it = CreateEventW(nullptr, FALSE, FALSE, nullptr);
				if (*it == nullptr) {
					throw std::runtime_error("Failed to create synchronization event.");
				}
			}
		}

		~SynchronizationObject() {
			if (handles_) {
				for (size_t i = 0; i < bufferCount_; ++i) {
					if (handles_[i]) CloseHandle(handles_[i]);
				}
				delete[] handles_;
			}
			delete[] values_;
		}

		void wait(size_t index) {
			assert(index < bufferCount_);

			if (fence_->GetCompletedValue() < values_[index]) {
				HANDLE& handle = this->handles_[index];
				fence_->SetEventOnCompletion(values_[index], handle);
				WaitForSingleObject(handle, INFINITE);
			}
		}

		void signal(ID3D12CommandQueue* queue, size_t index) {
			assert(index < bufferCount_);

			currentValue_++;
			values_[index] = currentValue_;
			queue->Signal(fence_.Get(), currentValue_);
		}

		SynchronizationObject(const SynchronizationObject&) 			   = delete;
		SynchronizationObject& operator=(const SynchronizationObject&) = delete;
		SynchronizationObject(SynchronizationObject&&) 			   = delete;
		SynchronizationObject& operator=(SynchronizationObject&&)      = delete;
	};

	class DX12Renderer {
	private:
		template <typename Ty>
		using ComPtr = Microsoft::WRL::ComPtr<Ty>;

		HWND hwnd_;

		ComPtr<ID3D12Device>  device_;
		ComPtr<IDXGIFactory7> factory_;

		ComPtr<ID3D12CommandAllocator>*    commandAllocators_;
		ComPtr<ID3D12CommandQueue>	       commandQueue_;
		ComPtr<ID3D12GraphicsCommandList>* commandList_;

		ComPtr<IDXGISwapChain4>      swapChain_;
		ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		ComPtr<ID3D12Resource>*      backBuffers_;

		std::unique_ptr<SynchronizationObject> synchronizationObject_;

		UINT frameIndex_;

		UINT rtvDescriptorSize_;

		BOOL isFullScreen_;
		BOOL isVSync_;

		size_t bufferCount_;

		void createCommandAllocatorQueueAndList() {
			HRESULT hr;

			commandAllocators_ = new ComPtr<ID3D12CommandAllocator>[bufferCount_];
			commandList_       = new ComPtr<ID3D12GraphicsCommandList>[bufferCount_];

			// Create a Command Allocator for each buffer
			for (UINT i = 0; i < bufferCount_; ++i) {
				SPIDER_DX12_ERROR_CHECK(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators_[i])));
			}

			// Create command queue
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Type				   = D3D12_COMMAND_LIST_TYPE_DIRECT;
			queueDesc.Flags				   = D3D12_COMMAND_QUEUE_FLAG_NONE;
			SPIDER_DX12_ERROR_CHECK(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)));

			// Create command list
			for (UINT i = 0; i < bufferCount_; ++i) {
				SPIDER_DX12_ERROR_CHECK(
					device_->CreateCommandList(
						0,
						D3D12_COMMAND_LIST_TYPE_DIRECT,
						commandAllocators_[i].Get(),
						nullptr,
						IID_PPV_ARGS(&commandList_[i])
					)
				);

				// Close the command list
				commandList_[i]->Close();
			}
		}

		void createSwapChain() {
			HRESULT hr;

			// Use a temporary swap chain
			ComPtr<IDXGISwapChain1> tempSwapChain;

			// Create swap chain
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.BufferCount	    = bufferCount_;
			swapChainDesc.BufferUsage	    = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.Format		    = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.SampleDesc.Count	= 1;
			swapChainDesc.SwapEffect	    = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.Flags		        = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
			SPIDER_DX12_ERROR_CHECK(
				factory_->CreateSwapChainForHwnd(
					commandQueue_.Get(),
					hwnd_,
					&swapChainDesc,
					nullptr,
					nullptr,
					&tempSwapChain
				)
			);
			tempSwapChain.As(&swapChain_);
		}

		void createRTVs() {
			HRESULT hr;

			if (backBuffers_) delete[] backBuffers_;
			this->backBuffers_ = new ComPtr<ID3D12Resource>[bufferCount_];

			// Create descriptor heap for RTV
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors		       = bufferCount_;
			rtvHeapDesc.Type			           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			SPIDER_DX12_ERROR_CHECK(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_)));

			// Get cpu descriptor handle and increment size
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
			rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			// Create back buffers and RTV
			for (UINT i = 0; i < bufferCount_; ++i) {
				SPIDER_DX12_ERROR_CHECK(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])));
				device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, rtvDescriptorSize_);
			}
		}

		VertexArrayBuffer createVertexBuffer(const FastArray<Vertex>& vertices) {
			HRESULT hr;

			// Create Vertex Array Buffer (struct)
			VertexArrayBuffer vertexArrayBuffer = {};

			// Calculate buffer size
			const size_t bufferSize = sizeof(Vertex) * vertices.size();
			vertexArrayBuffer.size  = vertices.size();

			// Create Vertex Array Buffer (resource)
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
			SPIDER_DX12_ERROR_CHECK(
				device_->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&resDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&vertexArrayBuffer.vertexArrayBuffer)
				)
			);

			// Copy vertex data to the vertex array buffer
			UINT8*        vertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			vertexArrayBuffer.vertexArrayBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
			memcpy(vertexDataBegin, vertices.data(), bufferSize);
			vertexArrayBuffer.vertexArrayBuffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view
			vertexArrayBuffer.vertexArrayBufferView.BufferLocation = vertexArrayBuffer.vertexArrayBuffer->GetGPUVirtualAddress();
			vertexArrayBuffer.vertexArrayBufferView.StrideInBytes  = sizeof(Vertex);
			vertexArrayBuffer.vertexArrayBufferView.SizeInBytes    = bufferSize;

			return vertexArrayBuffer;
		}

		IndexArrayBuffer createIndexArrayBuffer(const FastArray<uint32_t>& indices) {
			HRESULT hr;

			// Create Index Array Buffer (struct)
			IndexArrayBuffer indexArrayBuffer = {};

			// Calculate buffer size
			const size_t bufferSize = sizeof(uint32_t) * indices.size();
			indexArrayBuffer.size   = indices.size();

			// Create Index Array Buffer (resource)
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
			SPIDER_DX12_ERROR_CHECK(
				device_->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&resDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&indexArrayBuffer.indexArrayBuffer)
				)
			);

			// Copy index data to the index array buffer
			UINT8* indexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			indexArrayBuffer.indexArrayBuffer->Map(0, &readRange, reinterpret_cast<void**>(&indexDataBegin));
			memcpy(indexDataBegin, indices.data(), bufferSize);
			indexArrayBuffer.indexArrayBuffer->Unmap(0, nullptr);

			// Initialize the index buffer view
			indexArrayBuffer.indexArrayBufferView.BufferLocation = indexArrayBuffer.indexArrayBuffer->GetGPUVirtualAddress();
			indexArrayBuffer.indexArrayBufferView.Format         = DXGI_FORMAT_R32_UINT;
			indexArrayBuffer.indexArrayBufferView.SizeInBytes    = bufferSize;

			return indexArrayBuffer;
		}

	public:
		friend class DX12Compiler;

		DX12Renderer(HWND          hwnd,
				     const uint8_t bufferCount,
				     const bool    isFullScreen = false,
					 const bool    isVSync      = true,
					 const uint8_t deviceId     = 0) :
			bufferCount_(bufferCount),
			isFullScreen_(isFullScreen),
			isVSync_(isVSync),
			hwnd_(hwnd),
			frameIndex_(0)
		{
			HRESULT hr;

			// If on debug mode, enable the debug layer
#ifdef _DEBUG
			ComPtr<ID3D12Debug> debug;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
				debug->EnableDebugLayer();
				ComPtr<ID3D12Debug1> debug1;
				if (SUCCEEDED(debug.As(&debug1))) {
					debug1->SetEnableGPUBasedValidation(TRUE);
				}
			}
#endif

			// Initialize factory
			SPIDER_DX12_ERROR_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory_)));

			// Get a list of adapters
			ComPtr<IDXGIAdapter1> adapter;
			SPIDER_DX12_ERROR_CHECK(factory_->EnumAdapters1(deviceId, &adapter));

			// Create the device using a specific adapter
			SPIDER_DX12_ERROR_CHECK(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)));

			// Set break on error or corruption
			Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
			if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
			}

			// Create command allocator, queue and list
			this->createCommandAllocatorQueueAndList();

			// Create swap chain and RTVs
			this->createSwapChain();
			this->createRTVs();

			// Create synchronization object
			synchronizationObject_ = std::make_unique<SynchronizationObject>(device_.Get(), bufferCount_);
		}
		
		ConstantBuffers createConstantBuffers(const std::string* namesBegin, 
								  const std::string* namesEnd,
								  const size_t*      sizesBegin,
								  const size_t*      sizesEnd,
								  const ShaderStage  stage) 
		{
			// Calculate count (size)
			const size_t count = static_cast<size_t>(sizesEnd - sizesBegin);
			if (count == 0) return ConstantBuffers{};

			// Align sizes to 256 bytes
			size_t* alignedSizes     = new size_t[count];
			size_t  totalSizeInBytes = 0;
			for (uint32_t i = 0; i < count; ++i) {
				auto& size        = sizesBegin[i];
				alignedSizes[i]   = (size + 255) & ~255;
				totalSizeInBytes += alignedSizes[i];
			}

			// Create descriptor heap (CBV, shader visible)
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors	            = count;
			heapDesc.Type			            = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.Flags			            = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
			SPIDER_DX12_ERROR_CHECK(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));

			// Create constant buffer (resource)
			D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			D3D12_RESOURCE_DESC bufferDesc  = CD3DX12_RESOURCE_DESC::Buffer(totalSizeInBytes);
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			SPIDER_DX12_ERROR_CHECK(device_->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&resource)
			));

			// Create constant buffers
			ConstantBuffers constantBuffers;
			constantBuffers.begin = new ConstantBuffer[count];
			constantBuffers.end   = constantBuffers.begin + count;

			// Get descriptor handle increment size
			const UINT descriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// Get CPU and GPU descriptor handles
			auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
			auto gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart());

			// Create CBVs
			size_t offset = 0;
			for (size_t i = 0; i < count; ++i) {
				ConstantBuffer& buffer = constantBuffers.begin[i];
				buffer.name_           = namesBegin[i];
				buffer.heap_           = descriptorHeap;
				buffer.resource_       = resource;
				buffer.sizeInBytes_    = alignedSizes[i];
				buffer.cpuHandle_      = cpuHandle;
				buffer.gpuHandle_      = gpuHandle;
				buffer.stage_          = stage;
				buffer.index_          = i;

				// Create CBV
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation				= resource->GetGPUVirtualAddress() + offset;
				cbvDesc.SizeInBytes				= static_cast<UINT>(alignedSizes[i]);
				device_->CreateConstantBufferView(&cbvDesc, cpuHandle);

				// Open (map) the buffer for writing
				buffer.open();

				// Increment handles
				cpuHandle.Offset(1, descriptorSize);
				gpuHandle.Offset(1, descriptorSize);

				// Increment offset
				offset += alignedSizes[i];
			}
			
			// If on debug mode, set resource name
			SPIDER_DBG_CODE(resource->SetName(L"ConstantBuffer_Packed"));

			return constantBuffers;
		}
		ConstantBuffer createConstantBuffer(const std::string& name, const size_t size) {
			// Align size to 256 bytes
			size_t alignedSize = (size + 255) & ~255;

			// Create descriptor heap (CBV, shader visible)
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors		        = 1;
			heapDesc.Type			        = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.Flags			        = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
			SPIDER_DX12_ERROR_CHECK(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));

			// Create constant buffer (resource)
			D3D12_HEAP_PROPERTIES heapProps  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			D3D12_RESOURCE_DESC   bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			SPIDER_DX12_ERROR_CHECK(device_->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&resource)
			));

			// Get descriptor handle increment size
			const UINT descriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// Get CPU and GPU descriptor handles
			auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
			auto gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart());

			// Create Constant Buffer (struct)
			ConstantBuffer constantBuffer;
			constantBuffer.name_        = name;
			constantBuffer.heap_        = descriptorHeap;
			constantBuffer.resource_    = resource;
			constantBuffer.sizeInBytes_ = alignedSize;
			constantBuffer.cpuHandle_   = cpuHandle;
			constantBuffer.gpuHandle_   = gpuHandle;

			// Create CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation				= resource->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes				= static_cast<UINT>(alignedSize);
			device_->CreateConstantBufferView(&cbvDesc, cpuHandle);

			// If on debug mode, set resource name
			SPIDER_DBG_CODE(resource->SetName(L"ConstantBuffer_Packed"));

			return constantBuffer;
		}

		Mesh createMesh(const FastArray<Vertex>& vertices, const FastArray<uint32_t>& indices) {
			// Create mesh (struct)
			Mesh mesh;
			
			// Populate mesh buffers
			mesh.vertexArrayBuffer = std::make_unique<VertexArrayBuffer>(createVertexBuffer(vertices));
			mesh.indexArrayBuffer  = std::make_unique<IndexArrayBuffer>(createIndexArrayBuffer(indices));

			return mesh;
		}

		void beginFrame() {
			HRESULT hr;

			// Get current back buffer index
			frameIndex_ = swapChain_->GetCurrentBackBufferIndex();

			// Wait until the last frame is finished
			synchronizationObject_->wait(frameIndex_);

			// Reset command allocator and list
			SPIDER_DX12_ERROR_CHECK(commandAllocators_[frameIndex_]->Reset());
			SPIDER_DX12_ERROR_CHECK(commandList_[frameIndex_]->Reset(
				commandAllocators_[frameIndex_].Get(),
				nullptr
			));
		}

		void draw(RenderPipeline& pipeline, Mesh& mesh) {
			// Get current command list
			ID3D12GraphicsCommandList* cmd = commandList_[frameIndex_].Get();

			// Transition the back buffer to be used as render target
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffers_[frameIndex_].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			cmd->ResourceBarrier(1, &barrier);

			// Get the cpu descriptor handle for the current back buffer
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
				rtvHeap_->GetCPUDescriptorHandleForHeapStart(),
				frameIndex_,
				rtvDescriptorSize_
			);

			// Set render target and clear
			cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
			float clearColor[] = { 0.0, 0.0, 1.0, 1.0 };
			cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

			// Set pipeline state
			cmd->SetGraphicsRootSignature(pipeline.rootSignature_.Get());
			cmd->SetPipelineState(pipeline.pipelineState_.Get());
			
			// Set descriptor heaps and root descriptor tables
			if (!pipeline.requiredConstantBuffers_.empty()) {
				// If there are multiple descriptor heaps (one per shader), set them all
				std::vector<ID3D12DescriptorHeap*> heaps;
				heaps.reserve(pipeline.requiredConstantBuffers_.size());
				for (auto &ent : pipeline.requiredConstantBuffers_) {
					heaps.push_back(ent.second.heap_.Get());
				}
				cmd->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());

				for (auto [key, value] : pipeline.requiredConstantBuffers_) {
					cmd->SetGraphicsRootConstantBufferView(value.index_, value.resource_.Get()->GetGPUVirtualAddress());
				}
			}

			// Viewport e Scissor - use back buffer size instead of hardcoded values
			D3D12_RESOURCE_DESC backDesc = backBuffers_[frameIndex_]->GetDesc();
			float width				     = static_cast<float>(backDesc.Width);
			float height				 = static_cast<float>(backDesc.Height);
			CD3DX12_VIEWPORT viewport(0.0f, 0.0f, width, height);
			CD3DX12_RECT     scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));
			cmd->RSSetViewports(1, &viewport);
			cmd->RSSetScissorRects(1, &scissorRect);

			// Buffers
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd->IASetVertexBuffers(0, 1, &mesh.vertexArrayBuffer->vertexArrayBufferView);
			cmd->IASetIndexBuffer(&mesh.indexArrayBuffer->indexArrayBufferView);

			// Draw
			cmd->DrawIndexedInstanced(static_cast<UINT>(mesh.indexArrayBuffer->size), 1, 0, 0, 0);

			// Transição de volta
			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffers_[frameIndex_].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT
			);
			cmd->ResourceBarrier(1, &barrier);
		}

		void endFrame() {
			// Close command list
			commandList_[frameIndex_]->Close();

			// Execute command list
			ID3D12CommandList* cmds[] = { commandList_[frameIndex_].Get() };
			commandQueue_->ExecuteCommandLists(1, cmds);

			// Signal that the frame is finished
			synchronizationObject_->signal(commandQueue_.Get(), frameIndex_);
		}

		void present() {
			HRESULT hr;

			// Present the frame
			SPIDER_DX12_ERROR_CHECK(swapChain_->Present(isVSync_, isVSync_ ? 0 : DXGI_PRESENT_ALLOW_TEARING));
		}

		void setFullScreen(bool enabled) {
			HRESULT hr;

			SPIDER_DX12_ERROR_CHECK(swapChain_ != nullptr);
			SPIDER_DX12_ERROR_CHECK(factory_ != nullptr);

			if (enabled) {
				SPIDER_DX12_ERROR_CHECK(swapChain_->SetFullscreenState(TRUE, nullptr) == S_OK);
			}
			else {
				SPIDER_DX12_ERROR_CHECK(swapChain_->SetFullscreenState(FALSE, nullptr) == S_OK);
			}
		}
		void setFullScreen(BOOL enabled) {
			HRESULT hr;

			SPIDER_DX12_ERROR_CHECK(swapChain_ != nullptr);
			SPIDER_DX12_ERROR_CHECK(factory_ != nullptr);

			SPIDER_DX12_ERROR_CHECK(swapChain_->SetFullscreenState(enabled, nullptr) == S_OK);
			isFullScreen_ = enabled;
		}
		bool isFullScreenBool() const {
			HRESULT hr;

			return isFullScreen_ == TRUE;
		}
		BOOL isFullScreen() const {
			return isFullScreen_;
		}

		void setVSync(bool enabled) {
			HRESULT hr;

			isVSync_ = enabled ? TRUE : FALSE;
		}
		void setVSync(BOOL enabled) {
			isVSync_ = enabled;
		}
		bool isVSyncBool() const {
			HRESULT hr;

			return isVSync_ == TRUE;
		}
		BOOL isVSync() const {
			HRESULT hr;

			return isVSync_;
		}
	};

	class DX12Compiler {
	private:
		SPIDER_DX12_ERROR_CHECK_PREPARE;

		DX12Renderer* renderer_;

		Microsoft::WRL::ComPtr<IDxcUtils>          compilerUtils_;
		Microsoft::WRL::ComPtr<IDxcCompiler3>      compiler_;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> compilerIncludeHandler_;

		void reflectConstantBufferVariables(ConstantBufferData* cbufferData, ID3D12ShaderReflectionConstantBuffer* cbuffer, uint32_t variableCount) {
			HRESULT hr;

			// Iterate over all variables in the constant buffer
			for (int i = 0; i < variableCount; ++i) {
				// Get variable by index
				ID3D12ShaderReflectionVariable* cvariable = cbuffer->GetVariableByIndex(i);

				// Get variable description
				D3D12_SHADER_VARIABLE_DESC variableDesc;
				SPIDER_DX12_ERROR_CHECK(cvariable->GetDesc(&variableDesc));

				// Create constant variable (struct)
				ConstantBufferVariable constantBufferVariable = {};
				constantBufferVariable.name			  = variableDesc.Name ? variableDesc.Name : "";
				constantBufferVariable.offset		  = variableDesc.StartOffset;
				constantBufferVariable.size			  = variableDesc.Size;

				// Push to cbuffer data
				cbufferData->variables.pushBack(std::move(constantBufferVariable));
			}
		}
		void reflectConstantBuffers(ShaderData* shaderData, ID3D12ShaderReflection* reflection) {
			HRESULT hr;

			// Get shader description
			D3D12_SHADER_DESC desc;
			SPIDER_DX12_ERROR_CHECK(reflection->GetDesc(&desc));

			// Iterate over all Constant Buffers
			for (int i = 0; i < desc.ConstantBuffers; ++i) {
				ID3D12ShaderReflectionConstantBuffer* cbuffer = reflection->GetConstantBufferByIndex(i);
				
				// Get Constant Buffer description
				D3D12_SHADER_BUFFER_DESC cbufferDesc;
				SPIDER_DX12_ERROR_CHECK(cbuffer->GetDesc(&cbufferDesc));

				// Create Constant Buffer Data
				ConstantBufferData constantBufferData = {};
				constantBufferData.name			  = cbufferDesc.Name ? cbufferDesc.Name : "";
				constantBufferData.size			  = cbufferDesc.Size;
				constantBufferData.variableCount      = cbufferDesc.Variables;

				// Reflect variables
				reflectConstantBufferVariables(&constantBufferData, cbuffer, constantBufferData.variableCount);

				// Push to shader data
				shaderData->constantBuffers.pushBack(std::move(constantBufferData));
			}
		}
		void reflectResourceBindings(ShaderData* shaderData, ID3D12ShaderReflection* reflection, const ShaderStage stage) {
			HRESULT hr;

			// Get shader description
			D3D12_SHADER_DESC desc;
			SPIDER_DX12_ERROR_CHECK(reflection->GetDesc(&desc));

			// Iterate over all resource bindings
			for (int i = 0; i < desc.BoundResources; ++i) {
				D3D12_SHADER_INPUT_BIND_DESC resourceDesc;
				SPIDER_DX12_ERROR_CHECK(reflection->GetResourceBindingDesc(i, &resourceDesc));
				
				// Create Resource Binding Data
				ResourceBindingData resourceBindingData = {};
				resourceBindingData.name			= resourceDesc.Name ? resourceDesc.Name : "";
				resourceBindingData.type			= resourceDesc.Type;
				resourceBindingData.bindPoint		= resourceDesc.BindPoint;
				resourceBindingData.bindCount		= resourceDesc.BindCount;
				resourceBindingData.space			= resourceDesc.Space;
				resourceBindingData.stage			= stage;

				// Push to shader data
				shaderData->resourceBindingDatas.pushBack(std::move(resourceBindingData));
			}
		}

		ShaderData reflect(IDxcBlob* shaderBlob, const ShaderStage stage) {
			HRESULT hr;

			ShaderData shaderData;

			// Create shader buffer
			DxcBuffer dxcBuf = {};
			dxcBuf.Ptr       = shaderBlob->GetBufferPointer();
			dxcBuf.Size      = shaderBlob->GetBufferSize();
			dxcBuf.Encoding  = 0;

			// Create reflection using the shader buffer
			Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
			if (compilerUtils_) {
				hr = compilerUtils_->CreateReflection(&dxcBuf, IID_PPV_ARGS(&reflection));
				if (FAILED(hr)) {
					// Debug errors
					_com_error err(hr);
					std::wcerr << L"CreateReflection failed: " << err.ErrorMessage() << std::endl;
				}
			}

			// If there is no reflection, try to create it using container reflection
			if (!reflection) {
				// Create container reflection
				Microsoft::WRL::ComPtr<IDxcContainerReflection> containerReflection;
				SPIDER_DX12_ERROR_CHECK(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&containerReflection)));

				// Load the shader blob into the container reflection
				SPIDER_DX12_ERROR_CHECK(containerReflection->Load(shaderBlob));

				// Find the index of the part that contains the DXIL
				UINT32 dxilPartIndex = 0;
				SPIDER_DX12_ERROR_CHECK(containerReflection->FindFirstPartKind(DXC_PART_DXIL, &dxilPartIndex));

				// Get the reflection for the DXIL part
				Microsoft::WRL::ComPtr<ID3D12ShaderReflection> refl2;
				SPIDER_DX12_ERROR_CHECK(containerReflection->GetPartReflection(dxilPartIndex, IID_PPV_ARGS(&refl2)));
				reflection = refl2;
			}

			// Reflect constant buffers and resource bindings
			reflectConstantBuffers(&shaderData, reflection.Get());
			reflectResourceBindings(&shaderData, reflection.Get(), stage);
			shaderData.rawReflection = reflection;

			return shaderData;
		}

		template <typename Policy>
		requires SameAs<Policy, UsePathPolicy>
		Microsoft::WRL::ComPtr<IDxcBlob> compileShader(const std::wstring& path, const ShaderStage shaderStage) {
			HRESULT hr;

			// Check compiler
			SPIDER_DX12_ERROR_CHECK(compilerUtils_ != nullptr);
			SPIDER_DX12_ERROR_CHECK(compiler_ != nullptr);
			SPIDER_DX12_ERROR_CHECK(compilerIncludeHandler_ != nullptr);

			// Prepare arguments
			std::wstring args[5];
			switch (shaderStage)
			{
			case spider_engine::d3dx12::ShaderStage::STAGE_ALL:
				throw std::runtime_error("Impossible to create shader to all stages at once.");
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_VERTEX:
				args[0] = L"-E";
				args[1] = L"main";
				args[2] = L"-T";
				args[3] = L"vs_6_0";
				args[4] = L"-Zpr";
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_HULL:
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_DOMAIN:
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_GEOMETRY:
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_PIXEL:
				args[0] = L"-E";
				args[1] = L"main";
				args[2] = L"-T";
				args[3] = L"ps_6_0";
				args[4] = L"-Zpr";
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_AMPLIFICATION:
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_MESH:
				break;
			default:
				break;
			}

			// Convert arguments to LPCWSTR*
			LPCWSTR wcArgs[5] = { args[0].c_str(), args[1].c_str(), args[2].c_str(), args[3].c_str(), args[4].c_str() };

			// Load shader file
			IDxcBlobEncoding* sourceBlob;
			SPIDER_DX12_ERROR_CHECK(compilerUtils_->LoadFile(path.c_str(), nullptr, &sourceBlob));

			// Prepare source buffer
			DxcBuffer sourceBuffer = {};
			sourceBuffer.Ptr	   = sourceBlob->GetBufferPointer();
			sourceBuffer.Size      = sourceBlob->GetBufferSize();
			sourceBuffer.Encoding  = DXC_CP_UTF16;
			
			// Compile
			IDxcOperationResult* resultBuff;
			SPIDER_DX12_ERROR_CHECK(
				compiler_->Compile(
					&sourceBuffer,
					wcArgs,
					_countof(args),
					compilerIncludeHandler_.Get(),
					IID_PPV_ARGS(&resultBuff)
				)
			);
			// Further error checking
			SPIDER_DX12_ERROR_CHECK(resultBuff->GetStatus(&hr));

			// Get compiled shader
			IDxcBlob* shaderBlob;
			SPIDER_DX12_ERROR_CHECK(resultBuff->GetResult(&shaderBlob));

			// Release temp resources
			sourceBlob->Release();
			resultBuff->Release();

			return shaderBlob;
		}
		template <typename Policy>
		requires SameAs<Policy, UseSourcePolicy>
		Microsoft::WRL::ComPtr<IDxcBlob> compileShader(const std::wstring& source, const ShaderStage shaderStage) {
			HRESULT hr;

			// Check compiler
			SPIDER_DX12_ERROR_CHECK(compilerUtils_ != nullptr);
			SPIDER_DX12_ERROR_CHECK(compiler_ != nullptr);
			SPIDER_DX12_ERROR_CHECK(compilerIncludeHandler_ != nullptr);

			// Prepare arguments
			std::wstring args[5];
			switch (shaderStage)
			{
			case spider_engine::d3dx12::ShaderStage::STAGE_ALL:
				throw std::runtime_error("Impossible to create shader to all stages at once.");
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_VERTEX:
				args[0] = L"-E";
				args[1] = L"main";
				args[2] = L"-T";
				args[3] = L"vs_6_0";
				args[4] = L"-Zpr";
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_HULL:
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_DOMAIN:
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_GEOMETRY:
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_PIXEL:
				args[0] = L"-E";
				args[1] = L"main";
				args[2] = L"-T";
				args[3] = L"ps_6_0";
				args[4] = L"-Zpr";
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_AMPLIFICATION:
				break;
			case spider_engine::d3dx12::ShaderStage::STAGE_MESH:
				break;
			default:
				break;
			}

			// Convert arguments to LPCWSTR*
			LPCWSTR wcArgs[5] = { args[0].c_str(), args[1].c_str(), args[2].c_str(), args[3].c_str(), args[4].c_str() };

			// Prepare source buffer
			DxcBuffer sourceBuffer = {};
			sourceBuffer.Ptr       = source.c_str();
			sourceBuffer.Size      = source.size() * sizeof(wchar_t);
			sourceBuffer.Encoding  = DXC_CP_UTF16;

			// Compile
			IDxcOperationResult* resultBuff;
			SPIDER_DX12_ERROR_CHECK(
				compiler_->Compile(
					&sourceBuffer,
					wcArgs,
					_countof(args),
					compilerIncludeHandler_.Get(),
					IID_PPV_ARGS(&resultBuff)
				)
			);
			SPIDER_DX12_ERROR_CHECK(resultBuff->GetStatus(&hr));

			// Get compiled shader
			IDxcBlob* shaderBlob;
			SPIDER_DX12_ERROR_CHECK(resultBuff->GetResult(&shaderBlob));

			// Release temp resource
			resultBuff->Release();

			return shaderBlob;
		}

		DXGI_FORMAT mapMaskToFormat(D3D_REGISTER_COMPONENT_TYPE componentType, BYTE mask) {
			switch (componentType) {
			case D3D_REGISTER_COMPONENT_UINT32:
				switch (mask) {
				case 1: return DXGI_FORMAT_R32_UINT;
				case 3: return DXGI_FORMAT_R32G32_UINT;
				case 7: return DXGI_FORMAT_R32G32B32_UINT;
				case 15: return DXGI_FORMAT_R32G32B32A32_UINT;
				}
				break;
			case D3D_REGISTER_COMPONENT_SINT32:
				switch (mask) {
				case 1: return DXGI_FORMAT_R32_SINT;
				case 3: return DXGI_FORMAT_R32G32_SINT;
				case 7: return DXGI_FORMAT_R32G32B32_SINT;
				case 15: return DXGI_FORMAT_R32G32B32A32_SINT;
				}
				break;
			case D3D_REGISTER_COMPONENT_FLOAT32:
				switch (mask) {
				case 1: return DXGI_FORMAT_R32_FLOAT;
				case 3: return DXGI_FORMAT_R32G32_FLOAT;
				case 7: return DXGI_FORMAT_R32G32B32_FLOAT;
				case 15: return DXGI_FORMAT_R32G32B32A32_FLOAT;
				}
				break;
			}
			return DXGI_FORMAT_UNKNOWN;
		}

		FastArray<D3D12_ROOT_PARAMETER> createRootParameters(ShaderData& shaderData) {
			// Create references to constant buffers and resource bindings
			auto& cbuffers = shaderData.constantBuffers;
			auto& bindings = shaderData.resourceBindingDatas;

			// Create root parameters array
			FastArray<D3D12_ROOT_PARAMETER> rootParameters;
			const auto rootParametersSize = cbuffers.size() + bindings.size();
			if (rootParametersSize > 0) rootParameters.resize(rootParametersSize);
			
			// Populate root parameters with resource bindings
			for (auto binding = bindings.begin(); binding != bindings.end(); ++binding) {
				D3D12_ROOT_PARAMETER rootParameter = {};
				rootParameter.ParameterType		   = D3D12_ROOT_PARAMETER_TYPE_CBV;
				rootParameter.Descriptor		   = { binding->bindPoint, binding->space };
				rootParameter.ShaderVisibility     = static_cast<D3D12_SHADER_VISIBILITY>(binding->stage);
				rootParameters.pushBack(std::move(rootParameter));
			}

			return rootParameters;
		}

	public:
		DX12Compiler(DX12Renderer& renderer) : renderer_(&renderer) {
			HRESULT hr;

			// Create compiler instances
			SPIDER_DX12_ERROR_CHECK(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&compilerUtils_)));
			SPIDER_DX12_ERROR_CHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_)));
			SPIDER_DX12_ERROR_CHECK(compilerUtils_->CreateDefaultIncludeHandler(&compilerIncludeHandler_));
		}

		template <typename Policy>
		RenderPipeline createRenderPipeline(const FastArray<ShaderDescription>& descriptions) {
			HRESULT hr;

			RenderPipeline renderPipeline = {};

			// Create Pipeline State Object (PSO) description
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout				   = { psInputLayout, _countof(psInputLayout) };
			// Set rasterizer state: disable back-face culling for debugging
			psoDesc.RasterizerState			   = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.BlendState			       = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			// Disable depth testing since no depth buffer is created
			psoDesc.DepthStencilState		   = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.SampleMask			       = UINT_MAX;
			psoDesc.NodeMask			       = 0;
			psoDesc.PrimitiveTopologyType	   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets		   = 1;
			psoDesc.RTVFormats[0]			   = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count		   = 1;

			// Iterate over all shader descriptions, compile and reflect them, create root parameters and populate the PSO description
			FastArray<D3D12_ROOT_PARAMETER> parameters;
			std::unordered_map<std::pair<std::string, ShaderStage>, uint32_t> rootIndexMap;
			for (auto it = descriptions.cbegin(); it != descriptions.cend(); ++it) {
				// Compile and reflect shader
				Shader shader;
				shader.shader       = compileShader<Policy>(it->pathOrSource, it->stage);
				shader.pathOrSource = it->pathOrSource;
				shader.stage        = it->stage;
				shader.data         = reflect(shader.shader.Get(), it->stage);
				
				// Create root parameters
				FastArray<D3D12_ROOT_PARAMETER> shaderParameters = createRootParameters(shader.data);
				for (size_t p = 0; p < shaderParameters.size(); ++p) {
					auto&    param     = shaderParameters[p];
					uint32_t rootIndex = static_cast<uint32_t>(parameters.size());
					parameters.pushBack(std::move(param));
					// Map corresponding resource binding name & stage to root index. Assumes ordering matches between shaderParameters and resourceBindingDatas
					if (p < shader.data.resourceBindingDatas.size()) {
						auto& binding = shader.data.resourceBindingDatas[p];
						rootIndexMap.emplace(std::make_pair(binding.name, binding.stage), rootIndex);
					}
				}

				switch (it->stage)
				{
				case ShaderStage::STAGE_ALL:
					throw std::runtime_error("Impossible to create shader to all stages at once.");
					break;
				case ShaderStage::STAGE_VERTEX:
					psoDesc.VS = { shader.shader->GetBufferPointer(), shader.shader->GetBufferSize() };
					break;
				case ShaderStage::STAGE_HULL:
					break;
				case ShaderStage::STAGE_DOMAIN:
					break;
				case ShaderStage::STAGE_GEOMETRY:
					break;
				case ShaderStage::STAGE_PIXEL:
					psoDesc.PS = { shader.shader->GetBufferPointer(), shader.shader->GetBufferSize() };
					break;
				case ShaderStage::STAGE_AMPLIFICATION:
					break;
				case ShaderStage::STAGE_MESH:
					break;
				default:
					break;
				}
				renderPipeline.shaders_.pushBack(std::move(shader));
			}

			ID3DBlob* errorBlob;

			// Serialize root signature
			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.Flags                     = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			rootSignatureDesc.pParameters               = parameters.data();
			rootSignatureDesc.NumParameters             = parameters.size();
			ID3DBlob* rootSignatureBlob;
			SPIDER_DX12_ERROR_CHECK(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorBlob));

			// Create root signature
			SPIDER_DX12_ERROR_CHECK(
				renderer_->device_->CreateRootSignature(
					0,
					rootSignatureBlob->GetBufferPointer(),
					rootSignatureBlob->GetBufferSize(),
					IID_PPV_ARGS(&renderPipeline.rootSignature_)
				)
			);
			psoDesc.pRootSignature = renderPipeline.rootSignature_.Get();
			SPIDER_DX12_ERROR_CHECK(renderer_->device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderPipeline.pipelineState_)));

			// Finalize PSO description
			psoDesc.pRootSignature = renderPipeline.rootSignature_.Get();
			
			// Create references to Shaders and Constant Buffers
			auto& shaders                 = renderPipeline.shaders_;
			auto& requiredConstantBuffers = renderPipeline.requiredConstantBuffers_;

			// Required variables for creating constant buffers
			std::string*     constantBufferNames;
			size_t*			 constantBufferSizes;
			ShaderStage*     constantBufferStages;
			ConstantBuffers* constantBuffersArray = new ConstantBuffers[shaders.size()];

			// Create constant buffers
			for (uint32_t i = 0; i < renderPipeline.shaders_.size(); ++i) {
				auto& constantBuffers     = shaders[i].data.constantBuffers;
				auto  constantBuffersSize = constantBuffers.size();

				// Create an array for names, sizes and stages
				constantBufferNames  = new std::string[constantBuffersSize];
				constantBufferSizes  = new size_t[constantBuffersSize];
				constantBufferStages = new ShaderStage[constantBuffersSize];

				// Populate names, sizes and stages arrays
				for (uint32_t j = 0; j < constantBuffersSize; ++j) {
					auto& constantBufferData = constantBuffers[j];

					constantBufferNames[j]  = constantBufferData.name;
					constantBufferSizes[j]  = constantBufferData.size;
					constantBufferStages[j] = shaders[i].stage;
				}

				// Create constant buffers
				constantBuffersArray[i] = renderer_->createConstantBuffers(
					constantBufferNames,
				    constantBufferNames + constantBuffersSize,
					constantBufferSizes,
					constantBufferSizes + constantBuffersSize,
					renderPipeline.shaders_[i].stage
					);

				// Delete temp arrays
				delete[] constantBufferNames;
				delete[] constantBufferSizes;
				delete[] constantBufferStages;
			}
			// Finish the constant buffers creation, populating the required Constant Buffers Map
			for (auto it = constantBuffersArray; it != constantBuffersArray + shaders.size(); ++it) {
				uint32_t index = 0;
				for (auto constantBuffer = it->begin; constantBuffer != it->end; ++constantBuffer) {
					// Try to find root parameter index for this constant buffer using its name and stage
					auto key = std::make_pair(constantBuffer->name_, constantBuffer->stage_);
					auto rootIt = rootIndexMap.find(key);
					if (rootIt != rootIndexMap.end()) {
						constantBuffer->index_ = rootIt->second;
					} else {
						constantBuffer->index_ = index;
					}
					requiredConstantBuffers.emplace(
						std::make_pair(constantBuffer->name_, constantBuffer->stage_),
						*constantBuffer
					);
					++index;
				}
			}

			// Delete temp array
			delete[] constantBuffersArray;

			// Release temp resources
			rootSignatureBlob->Release();
			if (errorBlob) errorBlob->Release();

			return renderPipeline;
		}
	};
}