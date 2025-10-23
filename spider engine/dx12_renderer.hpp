/**
 * @file dx12_renderer.hpp
 * 
 * @brief Defines main classes for managing DirectX 12 rendering.
 * 
 * @version 0.2
 */

#pragma once

#pragma warning(push, 0)

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
#include "DirectXTex/DirectXTex.h"

// Framework includes
#include "fast_array.hpp"
#include "definitions.hpp"
#include "policies.hpp"
#include "types.hpp"
#include "flecs.h"

// DirectX 12 Types include
#include "dx12_types.hpp"

// Other includes
#include "camera.hpp"

// Link DirectX libraries
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace spider_engine::d3dx12 {
	/// @class DX12Renderer 
	/// @brief Main class for managing DirectX 12 rendering.
	/// @note Almost all function can throw a runtime_exception.
	class DX12Renderer {
	private:
		template <typename Ty>
		using ComPtr = Microsoft::WRL::ComPtr<Ty>;

		flecs::world* world_;

		HWND hwnd_;

		ComPtr<ID3D12Device>  device_;
		ComPtr<IDXGIFactory7> factory_;

		std::vector<ComPtr<ID3D12CommandAllocator>>    commandAllocators_;
		ComPtr<ID3D12CommandQueue>	                   commandQueue_;
		std::vector<ComPtr<ID3D12GraphicsCommandList>> commandLists_;

		std::vector<ComPtr<ID3D12CommandAllocator>>    nonRenderingRelatedCommandAllocators_;
		std::vector<ComPtr<ID3D12GraphicsCommandList>> nonRenderingRelatedCommandLists_;
		uint32_t                                       threadCount_;

		ComPtr<IDXGISwapChain4>			    swapChain_;
		std::vector<ComPtr<ID3D12Resource>> backBuffers_;
		std::vector<ComPtr<ID3D12Resource>> depthBuffers_;

		std::unique_ptr<SynchronizationObject> synchronizationObject_;
		std::unique_ptr<SynchronizationObject> nonRenderingRelatedSynchronizationObject_;

		std::unique_ptr<HeapAllocator> heapAllocator_;

		DescriptorHeap* rtvDescriptorHeap_;
		DescriptorHeap* dsvDescriptorHeap_;
		DescriptorHeap* cbvSrvUavDescriptorHeap_;
		DescriptorHeap* samplerDescriptorHeap_;

		UINT frameIndex_;

		BOOL isFullScreen_;
		BOOL isVSync_;

		size_t bufferCount_;

		void createCommandAllocatorQueueAndList() {
			// Create command queue
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Type					   = D3D12_COMMAND_LIST_TYPE_DIRECT;
			queueDesc.Flags					   = D3D12_COMMAND_QUEUE_FLAG_NONE;
			SPIDER_DX12_ERROR_CHECK(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)));

			// Allocate command allocators and lists arrays
			commandAllocators_.reserve(bufferCount_);
			commandLists_.reserve(bufferCount_);

			// Create a Command Allocator and a Command List for each back buffer
			for (UINT i = 0; i < bufferCount_; ++i) {
				commandAllocators_.emplace_back();
				SPIDER_DX12_ERROR_CHECK(
					device_->CreateCommandAllocator(
						D3D12_COMMAND_LIST_TYPE_DIRECT,
						IID_PPV_ARGS(&commandAllocators_[i])
					)
				);
				commandLists_.emplace_back();
				SPIDER_DX12_ERROR_CHECK(
					device_->CreateCommandList(
						0,
						D3D12_COMMAND_LIST_TYPE_DIRECT,
						commandAllocators_[i].Get(),
						nullptr,
						IID_PPV_ARGS(&commandLists_[i])
					)
				);

				// Close the command list
				commandLists_[i]->Close();
			}

			// Create non-rendering related command allocators and lists
			nonRenderingRelatedCommandAllocators_.reserve(threadCount_);
			nonRenderingRelatedCommandLists_.reserve(threadCount_);

			// Create a Command Allocator and a Command List for each thread
			for (UINT i = 0; i < threadCount_; ++i) {
				nonRenderingRelatedCommandAllocators_.emplace_back();
				SPIDER_DX12_ERROR_CHECK(
					device_->CreateCommandAllocator(
						D3D12_COMMAND_LIST_TYPE_DIRECT,
						IID_PPV_ARGS(&nonRenderingRelatedCommandAllocators_[i])
					)
				);
				nonRenderingRelatedCommandLists_.emplace_back();
				SPIDER_DX12_ERROR_CHECK(
					device_->CreateCommandList(
						0,
						D3D12_COMMAND_LIST_TYPE_DIRECT,
						nonRenderingRelatedCommandAllocators_[i].Get(),
						nullptr,
						IID_PPV_ARGS(&nonRenderingRelatedCommandLists_[i])
					)
				);

				// Close the command list
				nonRenderingRelatedCommandLists_[i]->Close();
			}
		}

		void createSwapChain() {
			// Use a temporary swap chain
			ComPtr<IDXGISwapChain1> tempSwapChain;

			// Create swap chain
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.BufferCount			= bufferCount_;
			swapChainDesc.BufferUsage			= DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.SampleDesc.Count		= 1;
			swapChainDesc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.Flags					= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
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

		void createRenderTargetViewsAndDepthStencilViews() {
			if (rtvDescriptorHeap_) return;
			// Create descriptor heap for RTV
			rtvDescriptorHeap_ = heapAllocator_->createDescriptorHeap(
				"rtvDescriptorHeap",
				bufferCount_,
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				D3D12_DESCRIPTOR_HEAP_FLAG_NONE
			);

			// Create back buffers and RTV
			size_t i = 0;
			auto rtvFn = [&i, this](DescriptorHeap* descriptorHeap) {
				backBuffers_.emplace_back();

				SPIDER_DX12_ERROR_CHECK(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])));
				device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, rtvDescriptorHeap_->cpuHandle);

				SPIDER_DBG_CODE(
					backBuffers_[i]->SetName(
						std::wstring(
							L"BackBuffer_" + std::to_wstring(i)
						).c_str()
					);
				)

				++i;
			};
			heapAllocator_->writeOnDescriptorHeap(rtvDescriptorHeap_, bufferCount_, rtvFn);

			dsvDescriptorHeap_ = heapAllocator_->createDescriptorHeap(
				"dsvDescriptorHeap",
				bufferCount_,
				D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
				D3D12_DESCRIPTOR_HEAP_FLAG_NONE
			);

			// Get back buffer description to set depth buffer size
			D3D12_RESOURCE_DESC backDesc = backBuffers_[frameIndex_]->GetDesc();
			const float width            = static_cast<float>(backDesc.Width);
			const float height           = static_cast<float>(backDesc.Height);

			i = 0;
			auto dsvFn = [&i, width, height, this](DescriptorHeap* descriptorHeap) {
				depthBuffers_.emplace_back();

				CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
				D3D12_RESOURCE_DESC depthDesc = {};
				depthDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				depthDesc.Width               = width;
				depthDesc.Height              = height;
				depthDesc.DepthOrArraySize    = 1;
				depthDesc.MipLevels           = 1;
				depthDesc.Format              = DXGI_FORMAT_D24_UNORM_S8_UINT;
				depthDesc.SampleDesc.Count    = 1;
				depthDesc.SampleDesc.Quality  = 0;
				depthDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				depthDesc.Flags               = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

				D3D12_CLEAR_VALUE clearValue    = {};
				clearValue.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
				clearValue.DepthStencil.Depth   = 1.0f;
				clearValue.DepthStencil.Stencil = 0;

				SPIDER_DX12_ERROR_CHECK(
					device_->CreateCommittedResource(
						&heapProps,
						D3D12_HEAP_FLAG_NONE,
						&depthDesc,
						D3D12_RESOURCE_STATE_DEPTH_WRITE,
						&clearValue,
						IID_PPV_ARGS(depthBuffers_[i].GetAddressOf())
					)
				);

				// Create the DSV
				D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
				dsvDesc.Format						  = DXGI_FORMAT_D24_UNORM_S8_UINT;
				dsvDesc.ViewDimension				  = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Flags						  = D3D12_DSV_FLAG_NONE;

				device_->CreateDepthStencilView(depthBuffers_[i].Get(), &dsvDesc, dsvDescriptorHeap_->cpuHandle);

				SPIDER_DBG_CODE(
					depthBuffers_[i]->SetName(
						std::wstring(L"DepthStencil_" + std::to_wstring(i)).c_str()
					);
				)

				++i;
			};
			heapAllocator_->writeOnDescriptorHeap(dsvDescriptorHeap_, bufferCount_, dsvFn);
		}

		VertexArrayBuffer createVertexBuffer(const std::vector<Vertex>& vertices) {
			// Create Vertex Array Buffer (struct)
			VertexArrayBuffer vertexArrayBuffer = {};

			// Calculate buffer size
			const size_t bufferSize = sizeof(Vertex) * vertices.size();
			vertexArrayBuffer.size  = vertices.size();

			// Create Vertex Array Buffer (resource)
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC   resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
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
			uint8_t*      vertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			vertexArrayBuffer.vertexArrayBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
			memcpy(vertexDataBegin, vertices.data(), bufferSize);
			vertexArrayBuffer.vertexArrayBuffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view
			vertexArrayBuffer.vertexArrayBufferView.BufferLocation = vertexArrayBuffer.vertexArrayBuffer->GetGPUVirtualAddress();
			vertexArrayBuffer.vertexArrayBufferView.StrideInBytes  = sizeof(Vertex);
			vertexArrayBuffer.vertexArrayBufferView.SizeInBytes    = bufferSize;

			SPIDER_DBG_CODE(vertexArrayBuffer.vertexArrayBuffer->SetName(L"VertexArrayBuffer"));

			return vertexArrayBuffer;
		}
		VertexArrayBuffer createVertexBuffer(const Vertex* verticesBegin,
			const Vertex* verticesEnd)
		{
			const size_t verticesSize = static_cast<size_t>(verticesEnd - verticesBegin);

			// Create Vertex Array Buffer (struct)
			VertexArrayBuffer vertexArrayBuffer = {};

			// Calculate buffer size
			const size_t bufferSize = sizeof(Vertex) * verticesSize;
			vertexArrayBuffer.size  = verticesSize;

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
			UINT8*		  vertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			vertexArrayBuffer.vertexArrayBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
			memcpy(vertexDataBegin, verticesBegin, bufferSize);
			vertexArrayBuffer.vertexArrayBuffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view
			vertexArrayBuffer.vertexArrayBufferView.BufferLocation = vertexArrayBuffer.vertexArrayBuffer->GetGPUVirtualAddress();
			vertexArrayBuffer.vertexArrayBufferView.StrideInBytes = sizeof(Vertex);
			vertexArrayBuffer.vertexArrayBufferView.SizeInBytes = bufferSize;

			SPIDER_DBG_CODE(vertexArrayBuffer.vertexArrayBuffer->SetName(L"VertexArrayBuffer"));

			return vertexArrayBuffer;
		}

		IndexArrayBuffer createIndexArrayBuffer(const std::vector<uint32_t>& indices) {
			// Create Index Array Buffer (struct)
			IndexArrayBuffer indexArrayBuffer = {};

			// Calculate buffer size
			const size_t bufferSize = sizeof(uint32_t) * indices.size();
			indexArrayBuffer.size   = indices.size();

			// Create Index Array Buffer (resource)
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC   resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
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
			UINT8*		  indexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			indexArrayBuffer.indexArrayBuffer->Map(0, &readRange, reinterpret_cast<void**>(&indexDataBegin));
			memcpy(indexDataBegin, indices.data(), bufferSize);
			indexArrayBuffer.indexArrayBuffer->Unmap(0, nullptr);

			// Initialize the index buffer view
			indexArrayBuffer.indexArrayBufferView.BufferLocation = indexArrayBuffer.indexArrayBuffer->GetGPUVirtualAddress();
			indexArrayBuffer.indexArrayBufferView.Format		 = DXGI_FORMAT_R32_UINT;
			indexArrayBuffer.indexArrayBufferView.SizeInBytes	 = bufferSize;

			SPIDER_DBG_CODE(indexArrayBuffer.indexArrayBuffer->SetName(L"IndexArrayBuffer"));

			return indexArrayBuffer;
		}
		IndexArrayBuffer createIndexArrayBuffer(const uint32_t* indicesBegin,
												const uint32_t* indicesEnd)
		{
			const size_t indicesSize = static_cast<size_t>(indicesEnd - indicesBegin);

			// Create Index Array Buffer (struct)
			IndexArrayBuffer indexArrayBuffer = {};

			// Calculate buffer size
			const size_t bufferSize = sizeof(uint32_t) * indicesSize;
			indexArrayBuffer.size   = indicesSize;

			// Create Index Array Buffer (resource)
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC   resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
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
			UINT8*		  indexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			indexArrayBuffer.indexArrayBuffer->Map(0, &readRange, reinterpret_cast<void**>(&indexDataBegin));
			memcpy(indexDataBegin, indicesBegin, bufferSize);
			indexArrayBuffer.indexArrayBuffer->Unmap(0, nullptr);

			// Initialize the index buffer view
			indexArrayBuffer.indexArrayBufferView.BufferLocation = indexArrayBuffer.indexArrayBuffer->GetGPUVirtualAddress();
			indexArrayBuffer.indexArrayBufferView.Format		 = DXGI_FORMAT_R32_UINT;
			indexArrayBuffer.indexArrayBufferView.SizeInBytes    = bufferSize;

			SPIDER_DBG_CODE(indexArrayBuffer.indexArrayBuffer->SetName(L"IndexArrayBuffer"));

			return indexArrayBuffer;
		}

	public:
		friend class DX12Compiler;

		DX12Renderer(flecs::world*  world,
					 HWND           hwnd,
					 const uint8_t  bufferCount,
					 const uint32_t threadCount,
					 const bool     isFullScreen = false,
					 const bool     isVSync      = true,
					 const uint8_t  deviceId     = 0) :
			world_(world),
			bufferCount_(bufferCount),
			threadCount_(threadCount),
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

			heapAllocator_ = std::make_unique<HeapAllocator>(device_);
			cbvSrvUavDescriptorHeap_ = heapAllocator_->createDescriptorHeap(
				"CbvUavDescriptorHeap", 
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
				D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
			);
			samplerDescriptorHeap_ = heapAllocator_->createDescriptorHeap(
				"SamplerDescriptorHeap_", 
				D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 
				D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
			);

			// Create Command Allocator, Command Queue and Command List
			this->createCommandAllocatorQueueAndList();

			// Create Swap Chain, Render Target Views and Depth Stencil Views
			this->createSwapChain();
			this->createRenderTargetViewsAndDepthStencilViews();

			// Create Synchronization Objects
			synchronizationObject_					  = std::make_unique<SynchronizationObject>(device_.Get(), bufferCount_);
			nonRenderingRelatedSynchronizationObject_ = std::make_unique<SynchronizationObject>(device_.Get(), threadCount_);
		}
		DX12Renderer(const DX12Renderer&) = delete;
		DX12Renderer(DX12Renderer&& other) noexcept :
			world_(std::move(other.world_)),
			hwnd_(other.hwnd_),
			device_(std::move(other.device_)),
			factory_(std::move(other.factory_)),
			commandAllocators_(other.commandAllocators_),
			commandQueue_(std::move(other.commandQueue_)),
			commandLists_(other.commandLists_),
			nonRenderingRelatedCommandAllocators_(other.nonRenderingRelatedCommandAllocators_),
			nonRenderingRelatedCommandLists_(other.nonRenderingRelatedCommandLists_),
			threadCount_(other.threadCount_),
			swapChain_(std::move(other.swapChain_)),
			backBuffers_(other.backBuffers_),
			depthBuffers_(other.depthBuffers_),
			synchronizationObject_(std::move(other.synchronizationObject_)),
			nonRenderingRelatedSynchronizationObject_(std::move(other.nonRenderingRelatedSynchronizationObject_)),
			frameIndex_(other.frameIndex_),
			isFullScreen_(other.isFullScreen_),
			isVSync_(other.isVSync_),
			bufferCount_(other.bufferCount_)
		{}

		~DX12Renderer() {
			// Ensure that GPU is done executing
			if (synchronizationObject_ && commandQueue_) {
				synchronizationObject_->signal(commandQueue_.Get(), frameIndex_);
				synchronizationObject_->wait(frameIndex_);
			}

			if (nonRenderingRelatedSynchronizationObject_ && commandQueue_) {
				nonRenderingRelatedSynchronizationObject_->signal(commandQueue_.Get(), 0);
				nonRenderingRelatedSynchronizationObject_->wait(0);
			}
		}

		/// @brief Creates a texture 2D that can be used on Shader Resource Views and Meshes.
		/// @param path Path to the texture.
		/// @param width Texture width.
		/// @param height Texture height.
		Texture2D createTexture2D(const std::wstring& path,
								  const uint32_t      width,
								  const uint32_t      height) 
		{
			// Get command allocator and list
			ID3D12CommandAllocator*    commandAllocator = nonRenderingRelatedCommandAllocators_[0].Get();
			ID3D12GraphicsCommandList* commandList      = nonRenderingRelatedCommandLists_[0].Get();

			// Reset command allocator and list
			commandAllocator->Reset();
			commandList->Reset(
				commandAllocator,
				nullptr
			);

			// Load image from file
			DirectX::ScratchImage image;
			DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);

			const DirectX::Image* img  = image.GetImage(0, 0, 0);
			const uint8_t*        data = img->pixels;

			// Create Texture2D (struct)
			Texture2D texture;
			texture.width  = width;
			texture.height = height;

			// Create texture description
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.Dimension		    = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			textureDesc.Alignment		    = 0;
			textureDesc.Width			    = width;
			textureDesc.Height			    = height;
			textureDesc.DepthOrArraySize	= 1;
			textureDesc.MipLevels		    = 1;
			textureDesc.Format			    = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.SampleDesc.Count    = 1;
			textureDesc.SampleDesc.Quality  = 0;
			textureDesc.Layout			    = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			textureDesc.Flags			    = D3D12_RESOURCE_FLAG_NONE;

			// Create Texture2D (gpu resource)
			D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			SPIDER_DX12_ERROR_CHECK(
				device_->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&textureDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&texture.resource)
				)
			);

			// Create Texture2D (upload resource)
			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.resource.Get(), 0, 1);

			heapProps                            = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
			SPIDER_DX12_ERROR_CHECK(
				device_->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&uploadBufferDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&texture.uploadResource)
				)
			);

			// Prepare data
			texture.textureData.pData      = data;
			texture.textureData.RowPitch   = width * 4; // 4 bytes per pixel (RGBA8)
			texture.textureData.SlicePitch = texture.textureData.RowPitch * height;

			// Copy data to GPU
			UpdateSubresources(
				commandList,
				texture.resource.Get(),
				texture.uploadResource.Get(),
				0,
				0,
				1,
				&texture.textureData
			);

			// After copy, transition to shader-read state
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				texture.resource.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
			nonRenderingRelatedCommandLists_[0]->ResourceBarrier(1, &barrier);

			// Close Graphics Command List
			SPIDER_DX12_ERROR_CHECK(nonRenderingRelatedCommandLists_[0]->Close());

			// Execute Graphics Command List
			ID3D12CommandList* ppCommandLists[] = { commandList };
			commandQueue_->ExecuteCommandLists(1, ppCommandLists);

			SPIDER_DBG_CODE(
				texture.resource->SetName(L"Texture2D");
				texture.uploadResource->SetName(L"Texture2D_Upload");
			)

			return texture;
		}
		/// @brief Creates a texture 2D that can be used on Shader Resource Views and Meshes.
		/// @param path Path to the texture.
		Texture2D createTexture2D(const std::wstring& path) {
			// Get command allocator and list
			ID3D12CommandAllocator*    commandAllocator = nonRenderingRelatedCommandAllocators_[0].Get();
			ID3D12GraphicsCommandList* commandList      = nonRenderingRelatedCommandLists_[0].Get();

			if (nonRenderingRelatedCommandLists_.empty()) {
				throw std::runtime_error("Command list is null!");
			}

			if (!nonRenderingRelatedCommandLists_[0].Get()) {
				throw std::runtime_error("Command list is null!");
			}

			assert(nonRenderingRelatedCommandLists_[0] != nullptr);

			// Reset command allocator and list
			SPIDER_DX12_ERROR_CHECK(commandAllocator->Reset());
			SPIDER_DX12_ERROR_CHECK(commandList->Reset(
				commandAllocator,
				nullptr
			));

			// Load image from file
			DirectX::ScratchImage image;
			HRESULT hr = DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
			if (FAILED(hr)) {
				throw std::runtime_error("Failed to load image");
			}

			const DirectX::Image* img = image.GetImage(0, 0, 0);
			if (!img || !img->pixels) {
				throw std::runtime_error("Failed to get image data");
			}

			const uint8_t* data = img->pixels;

			// Create Texture2D (struct)
			Texture2D texture;
			texture.width  = img->width;
			texture.height = img->height;

			// Create texture description
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.Dimension		    = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			textureDesc.Alignment		    = 0;
			textureDesc.Width			    = img->width;
			textureDesc.Height			    = img->height;
			textureDesc.DepthOrArraySize	= 1;
			textureDesc.MipLevels		    = 1;
			textureDesc.Format			    = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.SampleDesc.Count    = 1;
			textureDesc.SampleDesc.Quality  = 0;
			textureDesc.Layout			    = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			textureDesc.Flags			    = D3D12_RESOURCE_FLAG_NONE;

			// Create Texture2D (gpu resource)
			D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			SPIDER_DX12_ERROR_CHECK(
				device_->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&textureDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&texture.resource)
				)
			);

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.resource.Get(), 0, 1);

			// Create Texture2D (upload resource)
			heapProps                            = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
			SPIDER_DX12_ERROR_CHECK(
				device_->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&uploadBufferDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&texture.uploadResource)
				)
			);

			// Prepare data
			size_t rowPitch	  = 0;
			size_t slicePitch = 0;
			DirectX::ComputePitch(
				DXGI_FORMAT_R8G8B8A8_UNORM,
				img->width,
				img->height,
				rowPitch,
				slicePitch
			);
			texture.textureData.pData      = data;
			texture.textureData.RowPitch   = rowPitch;
			texture.textureData.SlicePitch = slicePitch;

			// Copy data to GPU
			if (UpdateSubresources(
				commandList,
				texture.resource.Get(),
				texture.uploadResource.Get(),
				0,
				0,
				1,
				&texture.textureData
			) == 0) 
			{
				throw std::runtime_error("UpdateSubresources returned 0!");
			}

			// After copy, transition to shader-read state
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				texture.resource.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
			nonRenderingRelatedCommandLists_[0]->ResourceBarrier(1, &barrier);

			// Close Graphics Command List
			SPIDER_DX12_ERROR_CHECK(nonRenderingRelatedCommandLists_[0]->Close());

			// Execute Graphics Command List
			ID3D12CommandList* ppCommandLists[] = { commandList };
			commandQueue_->ExecuteCommandLists(1, ppCommandLists);

			SPIDER_DBG_CODE(
				texture.resource->SetName(L"Texture2D");
				texture.uploadResource->SetName(L"Texture2D_Upload");
			)

			return texture;
		}

		/// @brief Creates a Constant Buffer that can be bound in a Render Pipeline.
		/// @param name Constant Buffer name.
		/// @param size Constant Buffer size.
		/// @param stage Constant Buffer stage.
		ConstantBuffer createConstantBuffer(const std::string& name, 
											const size_t	   size,
											const ShaderStage  stage) 
		{
			// Align size to 256 bytes
			size_t alignedSize = (size + 255) & ~255;

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

			ConstantBuffer constantBuffer;
			auto fn = [&name, &size, &alignedSize, &resource, &stage, &constantBuffer, this](DescriptorHeap* descriptorHeap) {
				// Get CPU and GPU descriptor handles
				CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = cbvSrvUavDescriptorHeap_->cpuHandle;
				CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = cbvSrvUavDescriptorHeap_->gpuHandle;

				// Create Constant Buffer (struct)
				constantBuffer.name_        = name;
				constantBuffer.heap_        = cbvSrvUavDescriptorHeap_->heap;
				constantBuffer.resource_    = resource;
				constantBuffer.sizeInBytes_ = alignedSize;
				constantBuffer.cpuHandle_   = cpuHandle;
				constantBuffer.gpuHandle_   = gpuHandle;
				constantBuffer.stage_       = stage;

				// Create CBV
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation			    	= resource->GetGPUVirtualAddress();
				cbvDesc.SizeInBytes		         		= static_cast<uint_t>(alignedSize);
				device_->CreateConstantBufferView(&cbvDesc, cpuHandle);
			};
			heapAllocator_->writeOnDescriptorHeap(cbvSrvUavDescriptorHeap_, 1, fn);

			// If on debug mode, set resource name
			SPIDER_DBG_CODE(resource->SetName(L"ConstantBuffer_Packed"));

			return constantBuffer;
		}
		/// @brief Creates multiple Constant Buffers that can be bound in a Render Pipeline.
		/// @param names Constant Buffer names.
		/// @param sizes Constant Buffer sizes.
		/// @param stage Constant Buffer stages.
		ConstantBuffers createConstantBuffers(const std::vector<std::string> names,
											  const std::vector<size_t>      sizes,
											  const ShaderStage			     stage)
		{
			// Calculate count (size)
			const size_t count = sizes.size();
			if (count == 0) return ConstantBuffers{};

			// Align sizes to 256 bytes
			std::vector<size_t> alignedSizes;
			size_t              totalSizeInBytes = 0;
			for (size_t i = 0; i < count; ++i) {
				auto& size = sizes[i];
				alignedSizes.push_back((size + 255) & ~255);
				totalSizeInBytes += alignedSizes[i];
			}

			// Create constant buffer (resource)
			D3D12_HEAP_PROPERTIES heapProps  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			D3D12_RESOURCE_DESC   bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalSizeInBytes);
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

			// Create CBVs
			size_t i        = 0;
			uint32_t offset = 0;
			auto fn = [&constantBuffers, &names, &resource, &stage, &offset, &alignedSizes, &i, this](DescriptorHeap* descriptorHeap) {
				// Create Constant Buffer (struct)
				ConstantBuffer& buffer = constantBuffers.begin[i];
				buffer.name_           = names[i];
				buffer.heap_           = descriptorHeap->heap;
				buffer.resource_       = resource;
				buffer.sizeInBytes_    = alignedSizes[i];
				buffer.cpuHandle_      = descriptorHeap->cpuHandle;
				buffer.gpuHandle_      = descriptorHeap->gpuHandle;
				buffer.stage_	       = stage;
				buffer.index_	       = i;

				// Create Constant Buffer View
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation					= resource->GetGPUVirtualAddress() + offset;
				cbvDesc.SizeInBytes						= static_cast<UINT>(alignedSizes[i]);
				device_->CreateConstantBufferView(&cbvDesc, descriptorHeap->cpuHandle);

				// Open (map) the buffer for writing
				buffer.open();

				offset += alignedSizes[i];

				++i;
			};
			heapAllocator_->writeOnDescriptorHeap(cbvSrvUavDescriptorHeap_, count, fn);

			SPIDER_DBG_CODE(resource->SetName(L"ConstantBuffer_Packed"));

			return constantBuffers;
		}

		/// @brief Creates a Shader Resource that can be bound in the pipeline.
		/// @param name Shader Resource name.
		/// @param data Shader Resource data.
		/// @param stage Shader Resource stage.
		ShaderResourceView createShaderResourceView(const std::string&          name,
													const std::vector<uint8_t>& data,
													const ShaderStage           stage)
		{
			ShaderResourceView shaderResourceView;

			// Create resource description
			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension			 = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment		     = 0;
			resourceDesc.Width				 = data.size();
			resourceDesc.Height				 = 1;
			resourceDesc.DepthOrArraySize	 = 1;
			resourceDesc.MipLevels			 = 1;
			resourceDesc.Format				 = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count    = 1;
			resourceDesc.SampleDesc.Quality	 = 0;
			resourceDesc.Layout				 = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags			     = D3D12_RESOURCE_FLAG_NONE;

			// Create resource
			D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			device_->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&shaderResourceView.resource_)
			);
			
			auto fn = [&shaderResourceView, name, data, stage, this](DescriptorHeap* descriptorHeap) {
				// Fill out Shader Resource View struct
				shaderResourceView.heap_        = descriptorHeap->heap;
				shaderResourceView.name_        = name;
				shaderResourceView.sizeInBytes_ = data.size();
				shaderResourceView.stage_       = stage;
				shaderResourceView.cpuHandle_   = descriptorHeap->cpuHandle;
				shaderResourceView.gpuHandle_   = descriptorHeap->gpuHandle;
				shaderResourceView.index_       = 0;

				// Copy data to the resource
				UINT8* dataBegin = nullptr;
				CD3DX12_RANGE readRange(0, 0);

				shaderResourceView.resource_->Map(0, &readRange, reinterpret_cast<void**>(&dataBegin));
				memcpy(dataBegin, data.data(), data.size());
				shaderResourceView.resource_->Unmap(0, nullptr);

				// Create Shader Resource View Description
				D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription = {};
				shaderResourceViewDescription.Format						  = DXGI_FORMAT_UNKNOWN;
				shaderResourceViewDescription.ViewDimension					  = D3D12_SRV_DIMENSION_BUFFER;
				shaderResourceViewDescription.Shader4ComponentMapping		  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				shaderResourceViewDescription.Buffer.FirstElement			  = 0;
				shaderResourceViewDescription.Buffer.NumElements			  = data.size();
				device_->CreateShaderResourceView(shaderResourceView.resource_.Get(), &shaderResourceViewDescription, shaderResourceView.cpuHandle_);
			};
			heapAllocator_->writeOnDescriptorHeap(cbvSrvUavDescriptorHeap_, 1, fn);

			SPIDER_DBG_CODE(shaderResourceView.resource_->SetName(L"ShaderResourceView_Buffer"));

			return shaderResourceView;
		}
		/// @brief Creates a Shader Resource for a Texture 2D that can be bound in a Render Pipeline.
		/// @param name Shader Resource name.
		/// @param data Shader Resource texture.
		/// @param stage Shader Resource stage.
		ShaderResourceView createShaderResourceViewForTexture2D(const std::string& name,
															    Texture2D&         data,
															    const ShaderStage  stage) 
		{
			ShaderResourceView shaderResourceView;

			const size_t dataSize = (data.width * data.height) * 4;
			
			auto fn = [&shaderResourceView, &name, dataSize, stage, &data, this](DescriptorHeap* descriptorHeap) {
				// Create Shader Resource View struct
				shaderResourceView.heap_        = descriptorHeap->heap;
				shaderResourceView.name_        = name;
				shaderResourceView.sizeInBytes_ = dataSize;
				shaderResourceView.stage_       = stage;
				shaderResourceView.cpuHandle_   = descriptorHeap->cpuHandle;
				shaderResourceView.gpuHandle_   = descriptorHeap->gpuHandle;
				shaderResourceView.index_       = 0;
				shaderResourceView.resource_    = data.resource;

				// Copy data to the resource
				UINT8*        dataBegin = nullptr;
				CD3DX12_RANGE readRange(0, 0);

				D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription = {};
				shaderResourceViewDescription.Format						  = DXGI_FORMAT_R8G8B8A8_UNORM;
				shaderResourceViewDescription.ViewDimension				      = D3D12_SRV_DIMENSION_TEXTURE2D;
				shaderResourceViewDescription.Shader4ComponentMapping		  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				shaderResourceViewDescription.Texture2D.MostDetailedMip	      = 0;
				shaderResourceViewDescription.Texture2D.MipLevels			  = 1;

				// Create Shader Resource View
				device_->CreateShaderResourceView(data.resource.Get(), &shaderResourceViewDescription, shaderResourceView.cpuHandle_);
			};
			heapAllocator_->writeOnDescriptorHeap(cbvSrvUavDescriptorHeap_, 1, fn);

			SPIDER_DBG_CODE(shaderResourceView.resource_->SetName(L"ShaderResourceView_Buffer"));

			return shaderResourceView;
		}
		/// @brief Creates multiple Shader Resource Views that can be bound in a Render Pipeline.
		/// @param names Shader Resource names.
		/// @param data Shader Resource data.
		/// @param stage Shader Resource stage.
		ShaderResourceViews createShaderResourceViews(const std::vector<std::string>&	       names,
													  const std::vector<std::vector<uint8_t>>& data,
													  const ShaderStage                        stage)
		{
			const size_t count = data.size();
			if (count <= 0) return ShaderResourceViews{};

			size_t			    totalSizeInBytes = 0;
			std::vector<size_t> sizes(count);
			for (int i = 0; i < data.size(); ++i) {
				// Get each block size
				sizes.push_back(data[i].size());
				// Calculate total size
				totalSizeInBytes += data[i].size();
			}

			ShaderResourceViews shaderResourceViews;

			// Create resource description
			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension			 = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment		     = 0;
			resourceDesc.Width				 = totalSizeInBytes;
			resourceDesc.Height				 = 1;
			resourceDesc.DepthOrArraySize	 = 1;
			resourceDesc.MipLevels			 = 1;
			resourceDesc.Format				 = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count    = 1;
			resourceDesc.SampleDesc.Quality	 = 0;
			resourceDesc.Layout				 = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags			     = D3D12_RESOURCE_FLAG_NONE;
			D3D12_HEAP_PROPERTIES heapProps  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

			// Create resource
			ComPtr<ID3D12Resource> resource;
			device_->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&resource)
			);

			// Allocate Shader Resource Views
			shaderResourceViews.begin = new ShaderResourceView[count];
			shaderResourceViews.end   = shaderResourceViews.begin + count;
			
			size_t offset    = 0;
			UINT8* dataBegin = nullptr;
			size_t i         = 0;
			auto fn = [&shaderResourceViews, &resource, &names, stage, &dataBegin, &offset, &data, &sizes, &i, this](DescriptorHeap* descriptorHeap) {
				// Create Shader Resource View struct
				ShaderResourceView& shaderResourceView = shaderResourceViews.begin[i];
				shaderResourceView.heap_               = descriptorHeap->heap;
				shaderResourceView.resource_           = resource;
				shaderResourceView.name_               = names[i];
				shaderResourceView.sizeInBytes_        = sizes[i];
				shaderResourceView.cpuHandle_          = cbvSrvUavDescriptorHeap_->cpuHandle;
				shaderResourceView.gpuHandle_		   = cbvSrvUavDescriptorHeap_->gpuHandle;
				shaderResourceView.stage_			   = stage;
				shaderResourceView.index_			   = i;

				CD3DX12_RANGE readRange(0, 0);

				// Copy data to the resource
				shaderResourceView.resource_->Map(0, &readRange, reinterpret_cast<void**>(&dataBegin));
				memcpy(dataBegin + offset, data[i].data(), sizes[i]);
				shaderResourceView.resource_->Unmap(0, nullptr);

				// Increment data pointer
				dataBegin                      += sizes[i];
				shaderResourceView.sizeInBytes_ = sizes[i];

				// Create Shader Resource View Description
				D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription = {};
				shaderResourceViewDescription.Format						  = DXGI_FORMAT_UNKNOWN;
				shaderResourceViewDescription.ViewDimension					  = D3D12_SRV_DIMENSION_BUFFER;
				shaderResourceViewDescription.Shader4ComponentMapping	      = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				shaderResourceViewDescription.Buffer.FirstElement			  = offset;
				shaderResourceViewDescription.Buffer.NumElements			  = sizes[i];
				device_->CreateShaderResourceView(shaderResourceView.resource_.Get(), &shaderResourceViewDescription, shaderResourceView.cpuHandle_);
				
				offset += sizes[i];

				++i;
			};
			heapAllocator_->writeOnDescriptorHeap(cbvSrvUavDescriptorHeap_, count, fn);

			SPIDER_DBG_CODE(resource->SetName(L"ShaderResourceView_Buffer_Packed"));

			return shaderResourceViews;
		}
		/// @brief Create multiple Shader Resources for multiple Texture 2Ds that can be bound in a Render Pipeline.
		/// @param names Shader Resource names
		/// @param data Shader Resource data
		/// @param stage Shader Resources stage
		ShaderResourceViews createShaderResourceViewsForTexture2D(std::vector<std::string>& names,
																  std::vector<Texture2D>&   data,
																  const ShaderStage         stage) 
		{
			const size_t count = data.size();
			if (count == 0) return ShaderResourceViews{};

			ShaderResourceViews shaderResourceViews;

			// Calculate sizes and total size
			size_t			  totalDataSize = 0;
			std::vector<size_t> sizes(count);
			for (UINT i = 0; i < count; ++i) {
				totalDataSize += (data[i].width * data[i].height) * 4;
				sizes.push_back(static_cast<size_t>(data[i].width * data[i].height) * 4);
			}

			// Allocate Shader Resource Views
			shaderResourceViews.begin = new ShaderResourceView[count];
			shaderResourceViews.end   = shaderResourceViews.begin + count;

			size_t i = 0;
			auto fn = [&i, &shaderResourceViews, &names, &sizes, stage, &data, this](DescriptorHeap* descriptorHeap) {
				// Create Shader Resource View struct
				shaderResourceViews.begin[i].heap_        = descriptorHeap->heap;
				shaderResourceViews.begin[i].name_        = names[i];
				shaderResourceViews.begin[i].sizeInBytes_ = sizes[i];
				shaderResourceViews.begin[i].stage_       = stage;
				shaderResourceViews.begin[i].cpuHandle_   = cbvSrvUavDescriptorHeap_->cpuHandle;
				shaderResourceViews.begin[i].gpuHandle_   = cbvSrvUavDescriptorHeap_->gpuHandle;
				shaderResourceViews.begin[i].index_       = i;

				// Create Shader Resource View Description
				D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription = {};
				shaderResourceViewDescription.Format						  = DXGI_FORMAT_R8G8B8A8_UNORM;
				shaderResourceViewDescription.ViewDimension				      = D3D12_SRV_DIMENSION_TEXTURE2D;
				shaderResourceViewDescription.Shader4ComponentMapping		  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				shaderResourceViewDescription.Texture2D.MostDetailedMip	      = 0;
				shaderResourceViewDescription.Texture2D.MipLevels			  = 1;

				// Create Shader Resource View
				device_->CreateShaderResourceView(
					data[i].resource.Get(), 
					&shaderResourceViewDescription, 
					shaderResourceViews.begin[i].cpuHandle_
				);

				data[i].resource->SetName(L"TexturedShaderResourceView");

				++i;
			};
			heapAllocator_->writeOnDescriptorHeap(cbvSrvUavDescriptorHeap_, count, fn);

			return shaderResourceViews;
		}
		
		/// @brief Creates a sampler
		/// @param name Sampler name
		/// @param stage Sampler stage
		Sampler createSampler(const std::string& name,
							  const ShaderStage  stage) 
		{
			Sampler sampler;
			auto fn = [&sampler, &name, stage, this](DescriptorHeap* descriptorHeap) {
				// Create Sampler struct
				sampler.heap_      = samplerDescriptorHeap_->heap;
				sampler.cpuHandle_ = samplerDescriptorHeap_->cpuHandle;
				sampler.gpuHandle_ = samplerDescriptorHeap_->gpuHandle;
				sampler.name_      = name;
				sampler.stage_     = stage;
				sampler.index_     = 0;

				// Create Sampler Description
				D3D12_SAMPLER_DESC sampDesc = {};
				sampDesc.Filter				= D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				sampDesc.AddressU			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampDesc.AddressV			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampDesc.AddressW			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampDesc.MipLODBias			= 0.0f;
				sampDesc.MaxAnisotropy		= 16;
				sampDesc.ComparisonFunc		= D3D12_COMPARISON_FUNC_ALWAYS;
				sampDesc.MinLOD				= 0.0f;
				sampDesc.MaxLOD				= D3D12_FLOAT32_MAX;
				sampDesc.BorderColor[0]		= sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0.0f;
				device_->CreateSampler(&sampDesc, samplerDescriptorHeap_->cpuHandle);
			};
			heapAllocator_->writeOnDescriptorHeap(samplerDescriptorHeap_, 1, fn);

			return sampler;
		}
		/// @brief Creates multiple Samplers
		/// @param names Sampler names
		/// @param stage Sampler stages
		Samplers createSamplers(const std::vector<std::string>& names,
								const ShaderStage               stage) 
		{
			const size_t count = names.size();
			if (count == 0) return Samplers{};

			Samplers samplers;

			// Allocate samplers
			samplers.begin = new Sampler[count];
			samplers.end   = samplers.begin + count;

			size_t i = 0;
			auto fn = [&names, &samplers, stage, &i, this](DescriptorHeap* descriptorHeap) {
				// Create Sampler struct
				auto& sampler      = samplers.begin[i];
				sampler.heap_      = descriptorHeap->heap;
				sampler.cpuHandle_ = samplerDescriptorHeap_->cpuHandle;
				sampler.gpuHandle_ = samplerDescriptorHeap_->gpuHandle;
				sampler.name_      = names[i];
				sampler.stage_     = stage;
				sampler.index_     = i;

				// Create Sampler Description
				D3D12_SAMPLER_DESC sampDesc = {};
				sampDesc.Filter				= D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				sampDesc.AddressU			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampDesc.AddressV			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampDesc.AddressW			= D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampDesc.MipLODBias			= 0.0f;
				sampDesc.MaxAnisotropy		= 16;
				sampDesc.ComparisonFunc		= D3D12_COMPARISON_FUNC_ALWAYS;
				sampDesc.MinLOD				= 0.0f;
				sampDesc.MaxLOD				= D3D12_FLOAT32_MAX;
				sampDesc.BorderColor[0]		= sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0.0f;
				device_->CreateSampler(&sampDesc, samplerDescriptorHeap_->cpuHandle);

				++i;
			};
			heapAllocator_->writeOnDescriptorHeap(samplerDescriptorHeap_, count, fn);

			return samplers;
		}

		/// @brief Creates a mesh that can bound in a Renderizable
		/// @param vertices Vertex array
		/// @param indices Index array
		/// @note This is the only function to create meshes, and actually there aren't any ways to load models.
		Mesh createMesh(const std::vector<Vertex>&   vertices,
						const std::vector<uint32_t>& indices)
		{
			// Create mesh (struct)
			Mesh mesh;
			
			// Populate mesh buffers
			mesh.vertexArrayBuffer = std::make_unique<VertexArrayBuffer>(createVertexBuffer(vertices));
			mesh.indexArrayBuffer  = std::make_unique<IndexArrayBuffer>(createIndexArrayBuffer(indices));

			return mesh;
		}
		
		/// @brief Creates a Renderizable that, if put on flecs, can be actually rendered.
		Renderizable createRenderizable(const std::vector<Vertex>&   vertices,
										const std::vector<uint32_t>& indices)
		{
			Renderizable renderizable;
			renderizable.mesh = this->createMesh(vertices, indices);

			return renderizable;
		}
		
		/// @brief Creates a Renderizable that, if put on flecs, can be actually rendered.
		/// @param mesh Mesh to be put on the Renderizable
		Renderizable createRenderizable(Mesh&& mesh)
		{
			Renderizable renderizable;
			renderizable.mesh = std::move(mesh);

			return renderizable;
		}

		/// @brief Begins the frame creation, allowing Renderizables to be rendered.
		void beginFrame() {
			// Get current back buffer index
			frameIndex_ = swapChain_->GetCurrentBackBufferIndex();

			// Wait until the last frame is finished
			synchronizationObject_->wait(frameIndex_);

			// Reset command allocator and list
			SPIDER_DX12_ERROR_CHECK(commandAllocators_[frameIndex_]->Reset());
			SPIDER_DX12_ERROR_CHECK(commandLists_[frameIndex_]->Reset(
				commandAllocators_[frameIndex_].Get(),
				nullptr
			));
		}

		/// @brief Draws a Renderizable.
		/// @param entity Entity containing a Renderizable.
		/// @param pipeline The Render Pipeline.
		/// @param camera The camera
		void draw(flecs::entity&     entity,
				  RenderPipeline&    pipeline,
				  rendering::Camera& camera)
		{
			// Get renderizable component
			const Renderizable* renderizable = entity.get<Renderizable>();

			// Get mesh and transform
			const Mesh&				    mesh      = renderizable->mesh;
			const rendering::Transform& transfrom = renderizable->transform;

			// Create and bind frame data
			rendering::FrameData frameData;
			frameData.view       = camera.getViewMatrix();
			frameData.projection = camera.getProjectionMatrix();

			// Model matrix
			DirectX::XMMATRIX scale       = DirectX::XMMatrixScalingFromVector(transfrom.scale);
			DirectX::XMMATRIX rotation    = DirectX::XMMatrixRotationQuaternion(transfrom.rotation);
			DirectX::XMMATRIX translation = DirectX::XMMatrixTranslationFromVector(transfrom.position);

			// Combine to form model matrix
			frameData.model = scale * rotation * translation;
			
			// Bind frame data to pipeline
			pipeline.bindBuffer<>("frameData", ShaderStage::STAGE_VERTEX, frameData);

			// Get current command list
			ID3D12GraphicsCommandList* cmd = commandLists_[frameIndex_].Get();

			// Transition the back buffer to be used as render target
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffers_[frameIndex_].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			cmd->ResourceBarrier(1, &barrier);

			// Get the cpu descriptor handle for the current back buffer
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
				rtvDescriptorHeap_->heap->GetCPUDescriptorHandleForHeapStart(),
				frameIndex_,
				rtvDescriptorHeap_->descriptorHandleIncrementSize
			);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
				dsvDescriptorHeap_->heap->GetCPUDescriptorHandleForHeapStart(),
				frameIndex_,
				dsvDescriptorHeap_->descriptorHandleIncrementSize
			);

			// Set render target and clear
			cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
			float clearColor[] = { 0.0, 0.0, 1.0, 1.0 };
			cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

			// Set pipeline state
			cmd->SetGraphicsRootSignature(pipeline.rootSignature_.Get());
			cmd->SetPipelineState(pipeline.pipelineState_.Get());
			
			// Descriptor heaps
			ID3D12DescriptorHeap* descriptorHeaps[] = {
				cbvSrvUavDescriptorHeap_->heap.Get(),
				samplerDescriptorHeap_->heap.Get()
			};
			cmd->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			cmd->SetGraphicsRootDescriptorTable(0, cbvSrvUavDescriptorHeap_->heap->GetGPUDescriptorHandleForHeapStart());
			cmd->SetGraphicsRootDescriptorTable(1, samplerDescriptorHeap_->heap->GetGPUDescriptorHandleForHeapStart());

			// Viewport and Scissor
			D3D12_RESOURCE_DESC backDesc = backBuffers_[frameIndex_]->GetDesc();
			float width				     = static_cast<float>(backDesc.Width);
			float height				 = static_cast<float>(backDesc.Height);
			CD3DX12_VIEWPORT viewport(0.0f, 0.0f, width, height);
			CD3DX12_RECT scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));
			cmd->RSSetViewports(1, &viewport);
			cmd->RSSetScissorRects(1, &scissorRect);

			// Buffers
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd->IASetVertexBuffers(0, 1, &mesh.vertexArrayBuffer->vertexArrayBufferView);
			cmd->IASetIndexBuffer(&mesh.indexArrayBuffer->indexArrayBufferView);

			// Draw
			cmd->DrawIndexedInstanced(static_cast<UINT>(mesh.indexArrayBuffer->size), 1, 0, 0, 0);

			// Transition the back buffer to be used to present
			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffers_[frameIndex_].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT
			);
			cmd->ResourceBarrier(1, &barrier);
		}

		/// @brief Ends the frame creation.
		void endFrame() {
			// Close command list
			commandLists_[frameIndex_]->Close();

			// Execute command list
			ID3D12CommandList* cmds[] = { commandLists_[frameIndex_].Get() };
			commandQueue_->ExecuteCommandLists(1, cmds);

			// Signal that the frame is finished
			synchronizationObject_->signal(commandQueue_.Get(), frameIndex_);
		}

		/// @brief Presents the created frame. 
		void present() {
			HRESULT hr;

			// Present the frame
			SPIDER_DX12_ERROR_CHECK(swapChain_->Present(isVSync_, isVSync_ ? 0 : DXGI_PRESENT_ALLOW_TEARING));
		}

		/// @brief Set full screen
		void setFullScreen(bool enabled) {
			HRESULT hr;

			if (enabled) {
				SPIDER_DX12_ERROR_CHECK(swapChain_->SetFullscreenState(TRUE, nullptr));
			}
			else {
				SPIDER_DX12_ERROR_CHECK(swapChain_->SetFullscreenState(FALSE, nullptr));
			}
		}
		/// @brief Set full screen
		void setFullScreen(BOOL enabled) {
			HRESULT hr;

			SPIDER_DX12_ERROR_CHECK(swapChain_->SetFullscreenState(enabled, nullptr));
			isFullScreen_ = enabled;
		}
		/// @brief Set full screen
		bool isFullScreenBool() const {
			HRESULT hr;

			return isFullScreen_ == TRUE;
		}
		/// @brief Is full screen?
		BOOL isFullScreen() const {
			return isFullScreen_;
		}

		/// @brief Set VSync.
		void setVSync(bool enabled) {
			HRESULT hr;

			isVSync_ = enabled ? TRUE : FALSE;
		}
		/// @brief Set VSync.
		void setVSync(BOOL enabled) {
			isVSync_ = enabled;
		}
		/// @brief Is VSync?
		bool isVSyncBool() const {
			HRESULT hr;

			return isVSync_ == TRUE;
		}
		/// @brief Is VSync?
		BOOL isVSync() const {
			HRESULT hr;

			return isVSync_;
		}

		DX12Renderer& operator=(const DX12Renderer&) = delete;
		DX12Renderer& operator=(DX12Renderer&& other) {
			if (this != &other) {
				this->~DX12Renderer();
				world_								  = other.world_;
				device_								  = std::move(other.device_);
				commandQueue_						  = std::move(other.commandQueue_);
				swapChain_							  = std::move(other.swapChain_);
				commandAllocators_					  = std::move(other.commandAllocators_);
				commandLists_						  = std::move(other.commandLists_);
				nonRenderingRelatedCommandAllocators_ = std::move(other.nonRenderingRelatedCommandAllocators_);
				nonRenderingRelatedCommandLists_	  = std::move(other.nonRenderingRelatedCommandLists_);
				backBuffers_						  = std::move(other.backBuffers_);
				depthBuffers_						  = std::move(other.depthBuffers_);
				synchronizationObject_				  = std::move(other.synchronizationObject_);
				frameIndex_							  = std::move(other.frameIndex_);
				isFullScreen_						  = std::move(other.isFullScreen_);
				isVSync_							  = std::move(other.isVSync_);
			}
			return *this;
		}
	};

	/// @class DX12Compiler
	/// @brief Class that manages Render Pipelines and Shaders
	class DX12Compiler {
	private:
		template <typename Ty>
		using ComPtr = Microsoft::WRL::ComPtr<Ty>;

		SPIDER_DX12_ERROR_CHECK_PREPARE;

		flecs::world* world_;

		DX12Renderer* renderer_;

		ComPtr<IDxcUtils>          compilerUtils_;
		ComPtr<IDxcCompiler3>      compiler_;
		ComPtr<IDxcIncludeHandler> compilerIncludeHandler_;

		void reflectConstantBufferVariables(ConstantBufferData*					  cbufferData, 
											ID3D12ShaderReflectionConstantBuffer* cbuffer, 
											uint32_t							  variableCount)
		{
			SPIDER_DX12_ERROR_CHECK_PREPARE;

			// Iterate over all variables in the constant buffer
			for (int i = 0; i < variableCount; ++i) {
				// Get variable by index
				ID3D12ShaderReflectionVariable* cvariable = cbuffer->GetVariableByIndex(i);

				// Get variable description
				D3D12_SHADER_VARIABLE_DESC variableDesc;
				SPIDER_DX12_ERROR_CHECK(cvariable->GetDesc(&variableDesc));

				// Create constant variable (struct)
				ConstantBufferVariable constantBufferVariable = {};
				constantBufferVariable.name			          = variableDesc.Name ? variableDesc.Name : "";
				constantBufferVariable.offset		          = variableDesc.StartOffset;
				constantBufferVariable.size			          = variableDesc.Size;

				// Push to cbuffer data
				cbufferData->variables.push_back(std::move(constantBufferVariable));
			}
		}
		void reflectConstantBuffers(ShaderData*				shaderData,
									ID3D12ShaderReflection* reflection) 
		{
			SPIDER_DX12_ERROR_CHECK_PREPARE;

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
				constantBufferData.name			      = cbufferDesc.Name ? cbufferDesc.Name : "";
				constantBufferData.size			      = cbufferDesc.Size;
				constantBufferData.variableCount      = cbufferDesc.Variables;

				// Reflect variables
				reflectConstantBufferVariables(&constantBufferData, cbuffer, constantBufferData.variableCount);

				// Push to shader data
				shaderData->constantBuffers.push_back(std::move(constantBufferData));
			}
		}
		void reflectShaderResourceViews(ShaderData*		    	shaderData,
										ID3D12ShaderReflection* reflection) 
		{
			SPIDER_DX12_ERROR_CHECK_PREPARE;

			// Get shader description
			D3D12_SHADER_DESC desc;
			SPIDER_DX12_ERROR_CHECK(reflection->GetDesc(&desc));

			// Iterate over all Constant Buffers
			for (UINT i = 0; i < desc.BoundResources; ++i) {
				D3D12_SHADER_INPUT_BIND_DESC resourceDesc;
				reflection->GetResourceBindingDesc(i, &resourceDesc);

				bool isTexture = false;
				if (resourceDesc.Type != D3D_SIT_TEXTURE &&
					resourceDesc.Type != D3D_SIT_STRUCTURED &&
					resourceDesc.Type != D3D_SIT_BYTEADDRESS)
				{
					continue;
				}
				isTexture = (resourceDesc.Type == D3D_SIT_TEXTURE);

				// Create Constant Buffer Data
				ShaderResourceViewData shaderResourceData = {};
				shaderResourceData.name					  = resourceDesc.Name ? resourceDesc.Name : "";
				shaderResourceData.isTexture              = isTexture;

				// Push to shader data
				shaderData->shaderResourceViews.push_back(std::move(shaderResourceData));
			}
		}
		void reflectSamplers(ShaderData*             shaderData,
							 ID3D12ShaderReflection* reflection)
		{
			SPIDER_DX12_ERROR_CHECK_PREPARE;

			// Get shader description
			D3D12_SHADER_DESC desc;
			SPIDER_DX12_ERROR_CHECK(reflection->GetDesc(&desc));

			// Iterate over all samplers
			for (UINT i = 0; i < desc.BoundResources; ++i) {
				D3D12_SHADER_INPUT_BIND_DESC resourceDesc;
				SPIDER_DX12_ERROR_CHECK(reflection->GetResourceBindingDesc(i, &resourceDesc));

				if (resourceDesc.Type != D3D_SIT_SAMPLER) {
					continue; // skip non-samplers
				}

				// Create Sampler Data
				SamplerData samplerData = {};
				samplerData.name		= resourceDesc.Name ? resourceDesc.Name : "";

				// Push to shader data
				shaderData->samplers.push_back(std::move(samplerData));
			}
		}
		void reflectResourceBindings(ShaderData*		     shaderData, 
									 ID3D12ShaderReflection* reflection, 
									 const ShaderStage		 stage) 
		{
			SPIDER_DX12_ERROR_CHECK_PREPARE;

			// Get shader description
			D3D12_SHADER_DESC desc;
			SPIDER_DX12_ERROR_CHECK(reflection->GetDesc(&desc));

			// Iterate over all resource bindings
			for (int i = 0; i < desc.BoundResources; ++i) {
				D3D12_SHADER_INPUT_BIND_DESC resourceDesc;
				SPIDER_DX12_ERROR_CHECK(reflection->GetResourceBindingDesc(i, &resourceDesc));
				
				// Create Resource Binding Data
				ResourceBindingData resourceBindingData = {};
				resourceBindingData.name			    = resourceDesc.Name ? resourceDesc.Name : "";
				resourceBindingData.type			    = resourceDesc.Type;
				resourceBindingData.bindPoint		    = resourceDesc.BindPoint;
				resourceBindingData.bindCount		    = resourceDesc.BindCount;
				resourceBindingData.space			    = resourceDesc.Space;
				resourceBindingData.stage			    = stage;

				// Push to shader data
				shaderData->shaderResourceBindingData.push_back(std::move(resourceBindingData));
			}
		}

		ShaderData reflect(IDxcBlob*         shaderBlob, 
						   const ShaderStage stage) 
		{
			if (!shaderBlob || shaderBlob->GetBufferSize() == 0) throw std::runtime_error("Shader blob is empty.");

			HRESULT hr;

			ShaderData shaderData;

			// Create shader buffer
			DxcBuffer dxcBuf = {};
			dxcBuf.Ptr       = shaderBlob->GetBufferPointer();
			dxcBuf.Size      = shaderBlob->GetBufferSize();
			dxcBuf.Encoding  = 0;

			// Create reflection using the shader buffer
			ComPtr<ID3D12ShaderReflection> reflection;
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
				ComPtr<IDxcContainerReflection> containerReflection;
				SPIDER_DX12_ERROR_CHECK(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&containerReflection)));

				// Load the shader blob into the container reflection
				SPIDER_DX12_ERROR_CHECK(containerReflection->Load(shaderBlob));

				// Find the index of the part that contains the DXIL
				UINT32 dxilPartIndex = 0;
				SPIDER_DX12_ERROR_CHECK(containerReflection->FindFirstPartKind(DXC_PART_DXIL, &dxilPartIndex));

				// Get the reflection for the DXIL part
				ComPtr<ID3D12ShaderReflection> refl2;
				SPIDER_DX12_ERROR_CHECK(containerReflection->GetPartReflection(dxilPartIndex, IID_PPV_ARGS(&refl2)));
				reflection = refl2;
			}

			// Reflect constant buffers and resource bindings
			reflectConstantBuffers(&shaderData, reflection.Get());
			reflectShaderResourceViews(&shaderData, reflection.Get());
			reflectSamplers(&shaderData, reflection.Get());
			reflectResourceBindings(&shaderData, reflection.Get(), stage);
			shaderData.rawReflection = reflection;

			return shaderData;
		}

		template <typename Policy>
		requires SameAs<Policy, UsePathPolicy>
		ComPtr<IDxcBlob> compileShader(const std::wstring& path, 
									   const ShaderStage   shaderStage) 
		{
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
		Microsoft::WRL::ComPtr<IDxcBlob> compileShader(const std::wstring& source, 
													   const ShaderStage   shaderStage) 
		{
			HRESULT hr;

			// Check compiler
			SPIDER_DX12_ERROR_CHECK(compilerUtils_ != nullptr);
			SPIDER_DX12_ERROR_CHECK(compiler_ != nullptr);
			SPIDER_DX12_ERROR_CHECK(compilerIncludeHandler_ != nullptr);

			// Prepare arguments
			std::wstring args[5];
			switch (shaderStage) {
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
			resultBuff->GetStatus(&hr);
			if (FAILED(hr)) {
				Microsoft::WRL::ComPtr<IDxcBlobEncoding> errors;
				resultBuff->GetErrorBuffer(&errors);
				if (errors) {
					std::string errMsg((char*)errors->GetBufferPointer(), errors->GetBufferSize());
					std::cerr << "DXC compile errors:\n" << errMsg << std::endl;
				}
				throw std::runtime_error("Shader compilation failed.");
			}

			IDxcBlob* shaderBlob;
			resultBuff->GetResult(&shaderBlob);

			// Release temp resource
			resultBuff->Release();

			return shaderBlob;
		}

		DXGI_FORMAT mapMaskToFormat(D3D_REGISTER_COMPONENT_TYPE componentType, 
									BYTE						mask) 
		{
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
		D3D12_ROOT_PARAMETER_TYPE mapResourceTypeToRootParameterType(D3D_SHADER_INPUT_TYPE type) {
			switch (type) {
				case D3D_SIT_CBUFFER: return D3D12_ROOT_PARAMETER_TYPE_CBV;
				case D3D_SIT_TBUFFER: return D3D12_ROOT_PARAMETER_TYPE_CBV;
				case D3D_SIT_TEXTURE: return D3D12_ROOT_PARAMETER_TYPE_SRV;
				//case D3D_SIT_SAMPLER: return D3D12_ROOT_PARAMETER_TYPE;
				case D3D_SIT_UAV_RWTYPED: return D3D12_ROOT_PARAMETER_TYPE_UAV;
				case D3D_SIT_STRUCTURED: return D3D12_ROOT_PARAMETER_TYPE_UAV;
				case D3D_SIT_UAV_RWSTRUCTURED: return D3D12_ROOT_PARAMETER_TYPE_UAV;
				case D3D_SIT_BYTEADDRESS: return D3D12_ROOT_PARAMETER_TYPE_UAV;
				case D3D_SIT_UAV_RWBYTEADDRESS: return D3D12_ROOT_PARAMETER_TYPE_UAV;
				case D3D_SIT_UAV_APPEND_STRUCTURED: return D3D12_ROOT_PARAMETER_TYPE_UAV;
				case D3D_SIT_UAV_CONSUME_STRUCTURED: return D3D12_ROOT_PARAMETER_TYPE_UAV;
			}
		}

	public:
		DX12Compiler(flecs::world* world,
					 DX12Renderer& renderer) : 
			world_(world),
			renderer_(&renderer)
		{
			HRESULT hr;

			// Create compiler instances
			SPIDER_DX12_ERROR_CHECK(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&compilerUtils_)));
			SPIDER_DX12_ERROR_CHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_)));
			SPIDER_DX12_ERROR_CHECK(compilerUtils_->CreateDefaultIncludeHandler(&compilerIncludeHandler_));
		}

		/*
		 *  @brief Creates a Render Pipeline.
		 *  @details Creates a Render Pipeline based on the descriptions.
		 *  this function returns an interface to bind Constant Buffers and Shader Resources.
		 *	@param descriptions The shader descriptions.
		 *  @tparam Policy The policy used to determine whether will use shader paths or sources.
		 */
		template <typename Policy>
		RenderPipeline createRenderPipeline(std::vector<ShaderDescription>& descriptions) {
			HRESULT hr = 0;

			RenderPipeline renderPipeline									  = {};
			renderPipeline.renderer_										  = renderer_;
			renderPipeline.createShaderResourceViewForStructuredDataFunction_ = &DX12Renderer::createShaderResourceView;
			renderPipeline.createShaderResourceViewForTexture2DFunction_      = &DX12Renderer::createShaderResourceViewForTexture2D;

			// Create Pipeline State Object (PSO) description
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout				           = { psInputLayout, _countof(psInputLayout) };
			// Set rasterizer state: disable back-face culling for debugging
			psoDesc.RasterizerState			           = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode           = D3D12_CULL_MODE_NONE;
			psoDesc.BlendState						   = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			// Disable depth testing since no depth buffer is created
			psoDesc.DepthStencilState                  = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable      = FALSE;
			psoDesc.SampleMask						   = UINT_MAX;
			psoDesc.NodeMask						   = 0;
			psoDesc.PrimitiveTopologyType			   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets				   = 1;
			psoDesc.RTVFormats[0]					   = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count		           = 1;

			size_t cbvCount		= 0;
			size_t srvCount		= 0;
			size_t samplerCount = 0;

			// Iterate over all shader descriptions, compile and reflect them, create root parameters and populate the PSO description
			std::vector<D3D12_ROOT_PARAMETER> parameters;
			std::unordered_map<std::pair<std::string, ShaderStage>, uint32_t> rootIndexMap;
			for (auto it = descriptions.cbegin(); it != descriptions.cend(); ++it) {
				// Compile and reflect shader
				Shader shader;
				shader.shader       = compileShader<Policy>(it->pathOrSource, it->stage);
				shader.pathOrSource = it->pathOrSource;
				shader.stage        = it->stage;
				shader.data         = reflect(shader.shader.Get(), it->stage);

				// Create root parameters
				std::vector<D3D12_ROOT_PARAMETER> parameters;
				parameters.reserve(shader.data.shaderResourceBindingData.size());
				for (size_t i = 0; i < shader.data.shaderResourceBindingData.size(); ++i) {
					auto& binding = shader.data.shaderResourceBindingData[i];
					rootIndexMap.emplace(std::make_pair(binding.name, binding.stage), binding.bindPoint);
					switch (binding.type) {
						case D3D_SIT_CBUFFER:
							++cbvCount;
							break;
						case D3D_SIT_TEXTURE:
							++srvCount;
							break;
						case D3D_SIT_STRUCTURED:
							++srvCount;
							break;
						case D3D_SIT_BYTEADDRESS:
							++srvCount;
							break;
						case D3D_SIT_SAMPLER:
							++samplerCount;
							break;
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
				renderPipeline.shaders_.push_back(std::move(shader));
			}

			CD3DX12_DESCRIPTOR_RANGE ranges[2];

			ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, cbvCount, 0, 0);

			ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, srvCount, 0, 0);

			CD3DX12_DESCRIPTOR_RANGE samplerRange;
			samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, samplerCount, 0);

			CD3DX12_ROOT_PARAMETER rootParameters[2];
			rootParameters[0].InitAsDescriptorTable(_countof(ranges), ranges, D3D12_SHADER_VISIBILITY_ALL);
			rootParameters[1].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

			CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
				_countof(rootParameters),
				rootParameters,
				0,
				nullptr,
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			);

			ComPtr<ID3DBlob> serializedRootSig = nullptr;
			ComPtr<ID3DBlob> errorBlob         = nullptr;

			HRESULT serializeHr = D3D12SerializeRootSignature(
				&rootSigDesc,
				D3D_ROOT_SIGNATURE_VERSION_1,
				&serializedRootSig,
				&errorBlob
			);

			if (FAILED(serializeHr)) {
				if (errorBlob) {
					const char* msg = static_cast<const char*>(errorBlob->GetBufferPointer());
					std::fprintf(stderr, "Root signature serialize error: %s\n", msg);
				}
				SPIDER_DX12_ERROR_CHECK(serializeHr);
			}

			// Create and store the root signature in the pipeline
			ComPtr<ID3D12RootSignature> localRootSig;
			SPIDER_DX12_ERROR_CHECK(renderer_->device_->CreateRootSignature(
				0,
				serializedRootSig->GetBufferPointer(),
				serializedRootSig->GetBufferSize(),
				IID_PPV_ARGS(&localRootSig)
			));

			// Important: assign to renderPipeline so psoDesc can reference it
			renderPipeline.rootSignature_ = localRootSig;

			// Now set PSO's root signature and create PSO
			psoDesc.pRootSignature = renderPipeline.rootSignature_.Get();
			SPIDER_DX12_ERROR_CHECK(renderer_->device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderPipeline.pipelineState_)));

			// Finalize PSO description
			psoDesc.pRootSignature = renderPipeline.rootSignature_.Get();

			// Create references to Shaders
			auto& shaders = renderPipeline.shaders_;

			// Create references to  Constant Buffers
			auto& requiredConstantBuffers = renderPipeline.requiredConstantBuffers_;

			// Required variables for creating constant buffers
			std::vector<std::string>     constantBufferNames;
			std::vector<size_t>	         constantBufferSizes;
			std::vector<ShaderStage>     constantBufferStages;
			std::vector<ConstantBuffers> constantBuffersArray;

			// Create constant buffers
			for (uint32_t i = 0; i < renderPipeline.shaders_.size(); ++i) {
				auto& constantBuffers     = shaders[i].data.constantBuffers;
				auto  constantBuffersSize = constantBuffers.size();
				if (constantBuffersSize == 0) continue;

				// Create an array for names, sizes and stages
				constantBufferNames.reserve(constantBuffersSize);
				constantBufferSizes.reserve(constantBuffersSize);
				constantBufferStages.reserve(constantBuffersSize);

				// Populate names, sizes and stages arrays
				for (uint32_t j = 0; j < constantBuffersSize; ++j) {
					auto& constantBufferData = constantBuffers[j];

					constantBufferNames.push_back(constantBufferData.name);
					constantBufferSizes.push_back(constantBufferData.size);
					constantBufferStages.push_back(shaders[i].stage);
				}

				// Create constant buffers
				constantBuffersArray.push_back(renderer_->createConstantBuffers(
					constantBufferNames,
					constantBufferSizes,
					renderPipeline.shaders_[i].stage
				));
			}
			// Finish the constant buffers creation, populating the required Constant Buffers Map
			for (auto it = constantBuffersArray.begin(); it != constantBuffersArray.end(); ++it) {
				uint32_t index = 0;
				for (auto constantBuffer = it->begin; constantBuffer != it->end; ++constantBuffer) {
					// Try to find root parameter index for this constant buffer using its name and stage
					auto key    = std::make_pair(constantBuffer->name_, constantBuffer->stage_);
					auto rootIt = rootIndexMap.find(key);
					if (rootIt != rootIndexMap.end()) {
						constantBuffer->index_ = rootIt->second;
					}
					else {
						constantBuffer->index_ = index;
					}
					requiredConstantBuffers.emplace(
						std::make_pair(constantBuffer->name_, constantBuffer->stage_),
						*constantBuffer
					);
					++index;
				}
			}

			// Create references to Shader Resource Views
			auto& requiredShaderResourceViews = renderPipeline.requiredShaderResourceViews_;

			// Required variables for creating shader resource views
			std::vector<ShaderResourceView> shaderResourceViewArray;
			std::vector<size_t>             shaderResourceViewSize;

			// Create Shader Resource Views
			for (size_t i = 0; i < renderPipeline.shaders_.size(); ++i) {
				auto& shaderResourceViews     = shaders[i].data.shaderResourceViews;
				auto  shaderResourceViewsSize = shaderResourceViews.size();
				if (shaderResourceViewsSize == 0) continue;

				// Populate names, sizes and stages arrays
				for (uint32_t j = 0; j < shaderResourceViewsSize; ++j) {
					auto& shaderResourceViewData = shaderResourceViews[j];

					shaderResourceViewSize.push_back(shaderResourceViewData.size);

					ShaderResourceView emptySrv = {};
					emptySrv.stage_			    = shaders[i].stage;
					emptySrv.name_				= shaderResourceViewData.name;
					shaderResourceViewArray.push_back(emptySrv);
				}
			}
			// Finish the shader resource view creation, populating the required Constant Buffers Map
			for (auto shaderResourceView = shaderResourceViewArray.begin(); shaderResourceView != shaderResourceViewArray.end(); ++shaderResourceView) {
				// Try to find root parameter index for this shader resource view using its name and stage
				auto key    = std::make_pair(shaderResourceView->name_, shaderResourceView->stage_);
				auto rootIt = rootIndexMap.find(key);
				if (rootIt != rootIndexMap.end()) {
					shaderResourceView->index_ = rootIt->second;
				}
				requiredShaderResourceViews.emplace(
					std::make_pair(shaderResourceView->name_, shaderResourceView->stage_),
					*shaderResourceView
				);
			}

			// Create references to Samplers
			auto& requiredSamplers = renderPipeline.requiredSamplers_;

			// Required variables for creating samplers
			std::vector<std::string> samplerNames;
			std::vector<ShaderStage> samplerStages;
			std::vector<Samplers>    samplerArray(shaders.size());

			// Create samplers
			for (size_t i = 0; i < renderPipeline.shaders_.size(); ++i) {
				auto&        samplers      = shaders[i].data.samplers;
				const size_t samplersSize  = samplers.size();
				if (samplersSize == 0) continue;

				// Create an array for names, sizes and stages
				samplerNames.reserve(samplersSize);
				samplerStages.reserve(samplersSize);

				// Populate names, sizes and stages arrays
				for (uint32_t j = 0; j < samplersSize; ++j) {
					auto& samplerData = samplers[j];

					samplerNames.push_back(samplerData.name);
					samplerStages.push_back(shaders[i].stage);
				}

				// Create sampler buffers
				samplerArray.push_back(renderer_->createSamplers(
					samplerNames,
					renderPipeline.shaders_[i].stage
				));
			}
			// Finish the shade resource view creation, populating the required Constant Buffers Map
			for (auto it = samplerArray.begin(); it != samplerArray.end(); ++it) {
				size_t index = 0;
				for (auto sampler = it->begin; sampler != it->end; ++sampler) {
					// Try to find root parameter index for this shader resource view using its name and stage
					auto key    = std::make_pair(sampler->name_, sampler->stage_);
					auto rootIt = rootIndexMap.find(key);
					if (rootIt != rootIndexMap.end()) {
						sampler->index_ = rootIt->second;
					}
					else {
						sampler->index_ = index;
					}
					requiredSamplers.emplace(
						std::make_pair(sampler->name_, sampler->stage_),
						*sampler
					);
					++index;
				}
			}

			return renderPipeline;
		}
	};
}

#pragma warning(pop)