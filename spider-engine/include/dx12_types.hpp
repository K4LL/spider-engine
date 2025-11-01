#pragma once
#include <flat_hash_map.hpp>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <wrl/client.h>
#include <comdef.h>

#include "d3dx12.h"
#include "dxcapi.h"
#include "d3d12shader.h"
#include "DirectXTex/DirectXTex.h"

#include "definitions.hpp"
#include "concepts.hpp"
#include "policies.hpp"
#include "dx12_policies.hpp"

namespace spider_engine::d3dx12 {
	enum class ShaderStage : uint8_t {
		STAGE_ALL			= 0,
		STAGE_VERTEX		= 1,
		STAGE_HULL			= 2,
		STAGE_DOMAIN		= 3,
		STAGE_GEOMETRY		= 4,
		STAGE_PIXEL			= 5,
		STAGE_AMPLIFICATION = 6,
		STAGE_MESH          = 7,
	};

	enum class TextureDimension : uint8_t {
		NONE         = 0,
		TEXTURE_1D   = 1,
		TEXTURE_2D   = 2,
		TEXTURE_3D   = 3,
		TEXTURE_CUBE = 4
	};

	enum class BindingType : uint8_t {
		CONSTANT_BUFFER = 0,
		SHADER_RESOURCE = 1,
		SAMPLER         = 2,
	};
}

namespace std {
	template<>
	struct hash<std::pair<std::string, spider_engine::d3dx12::ShaderStage>> {
		std::size_t operator()(const std::pair<std::string, spider_engine::d3dx12::ShaderStage>& k) const {
			return std::hash<std::string>()(k.first) ^ (std::hash<uint8_t>()(static_cast<uint8_t>(k.second)) << 1);
		}
	};
	template<>
	struct hash<std::pair<std::string, spider_engine::d3dx12::TextureDimension>> {
		std::size_t operator()(const std::pair<std::string, spider_engine::d3dx12::TextureDimension>& k) const {
			return std::hash<std::string>()(k.first) ^ (std::hash<uint8_t>()(static_cast<uint8_t>(k.second)) << 1);
		}
	};
	template<>
	struct hash<std::pair<std::string, spider_engine::d3dx12::BindingType>> {
		std::size_t operator()(const std::pair<std::string, spider_engine::d3dx12::BindingType>& k) const {
			return std::hash<std::string>()(k.first) ^ (std::hash<uint8_t>()(static_cast<uint8_t>(k.second)) << 1);
		}
	};
}

#include "flat_hash_map.hpp"

namespace spider_engine::d3dx12 {
	class DX12Renderer;
	class DX12Compiler;

	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		DirectX::XMFLOAT3 tangent;
	};

	inline constexpr D3D12_INPUT_ELEMENT_DESC psInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		  static_cast<UINT>(offsetof(Vertex, position)),
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		  static_cast<UINT>(offsetof(Vertex, normal)),
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,
		  static_cast<UINT>(offsetof(Vertex, uv)),
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		  static_cast<UINT>(offsetof(Vertex, tangent)),
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	struct VertexArrayBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexArrayBuffer;
		D3D12_VERTEX_BUFFER_VIEW			   vertexArrayBufferView;

		size_t size;
	};
	struct IndexArrayBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> indexArrayBuffer;
		D3D12_INDEX_BUFFER_VIEW			       indexArrayBufferView;

		size_t size;
	};

	struct Texture2D {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadResource;

		D3D12_SUBRESOURCE_DATA textureData;

		uint32_t width;
		uint32_t height;
	};

	struct ConstantBufferVariable {
		std::string name;
		uint32_t    offset;
		uint32_t    size;
	};
	struct ConstantBufferData {
		std::string      name;
		uint32_t         size;
		uint32_t         variableCount;

		uint32_t bindPoint;
		uint32_t space;

		ShaderStage stage;

		std::vector<ConstantBufferVariable> variables;
	};

	struct ShaderResourceViewData {
		std::string name;
		uint32_t    size;

		uint32_t bindPoint;
		uint32_t space;

		bool isTexture;

		ShaderStage stage;
	};

	struct SamplerData {
		std::string name;
		uint32_t    size;

		uint32_t bindPoint;
		uint32_t space;

		ShaderStage stage;
	};

	class ShaderDescription {
	public:
		std::wstring pathOrSource;
		ShaderStage  stage;

		ShaderDescription() :
			stage(ShaderStage::STAGE_ALL)
		{}

		ShaderDescription(const std::wstring& pathOrSource,
						  const ShaderStage   shaderStage) :
			pathOrSource(pathOrSource),
			stage(shaderStage)
		{}
		ShaderDescription(const ShaderDescription& other) :
			pathOrSource(other.pathOrSource),
			stage(other.stage)
		{}
		ShaderDescription(ShaderDescription&& other) noexcept :
			pathOrSource(other.pathOrSource),
			stage(other.stage)
		{}

		ShaderDescription& operator=(const ShaderDescription& other) {
			if (this != &other) {
				pathOrSource = other.pathOrSource;
				stage        = other.stage;
			}
			return *this;
		}
		ShaderDescription& operator=(ShaderDescription&& other) noexcept {
			if (this != &other) {
				pathOrSource = std::move(other.pathOrSource);
				stage        = other.stage;
			}
			return *this;
		}
	};

	class ConstantBuffer {
	private:
		std::string name_;
		size_t      sizeInBytes_;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
		Microsoft::WRL::ComPtr<ID3D12Resource>       resource_;

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle_;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle_;

		ShaderStage stage_;
		uint32_t    index_;

		void* mappedData_;

	public:
		friend class DX12Renderer;
		friend class DX12Compiler;

		ConstantBuffer() = default;
		ConstantBuffer(const ConstantBuffer&) = default;
		ConstantBuffer(ConstantBuffer&&) noexcept = default;

		void open() {
			resource_->Map(0, nullptr, &mappedData_);
			mappedData_ = reinterpret_cast<uint8_t*>(mappedData_) + (index_ * sizeInBytes_);
		}
		template <typename Ty>
		void copy(const Ty& data) {
			memcpy(mappedData_, &data, sizeof(Ty));
		}
		void unmap() {
			resource_->Unmap(0, nullptr);
		}

		std::string_view getName() {
			return this->name_;
		}
		size_t getSizeInBytes() {
			return this->sizeInBytes_;
		}
		ShaderStage getStage() {
			return this->stage_;
		}
		template <typename Type>
		Type* getPtr() {
			return reinterpret_cast<Type*>(mappedData_);
		}
		D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() const {
			return resource_->GetGPUVirtualAddress();
		}
		uint32_t getIndex() const {
			return this->index_;
		}

		ConstantBuffer& operator=(const ConstantBuffer& other) = default;
		ConstantBuffer& operator=(ConstantBuffer&& other) noexcept = default;
	};

	class ShaderResourceView {
	private:
		std::string name_;
		size_t      sizeInBytes_;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
		Microsoft::WRL::ComPtr<ID3D12Resource>       resource_;

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle_;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle_;

		ShaderStage stage_;
		uint32_t    index_;

	public:
		friend class DX12Renderer;
		friend class DX12Compiler;
		friend class RenderPipeline;

		ShaderResourceView() :
			name_(""),
			sizeInBytes_(0),
			stage_(ShaderStage::STAGE_ALL),
			index_(0),
			cpuHandle_(D3D12_CPU_DESCRIPTOR_HANDLE(NULL)),
			gpuHandle_(D3D12_GPU_DESCRIPTOR_HANDLE(NULL))
		{}
		ShaderResourceView(const ShaderResourceView&)     = default;
		ShaderResourceView(ShaderResourceView&&) noexcept = default;

		std::string_view getName() {
			return this->name_;
		}

		size_t getSizeInBytes() {
			return this->sizeInBytes_;
		}

		ShaderStage getStage() {
			return this->stage_;
		}

		D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() const {
			return resource_->GetGPUVirtualAddress();
		}
		D3D12_GPU_DESCRIPTOR_HANDLE getGPUDescriptorHandle() const {
			return gpuHandle_;
		}

		uint32_t getIndex() const {
			return this->index_;
		}

		ShaderResourceView& operator=(const ShaderResourceView& other) = default;
		ShaderResourceView& operator=(ShaderResourceView&& other) noexcept = default;
	};

	class Sampler {
	private:
		std::string	name_;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
		CD3DX12_CPU_DESCRIPTOR_HANDLE				 cpuHandle_;
		CD3DX12_GPU_DESCRIPTOR_HANDLE				 gpuHandle_;

		ShaderStage	stage_;
		uint32_t	index_;

	public:
		friend class DX12Renderer;
		friend class DX12Compiler;

		Sampler() :
			name_(""),
			stage_(ShaderStage::STAGE_ALL),
			index_(0)
		{}
		Sampler(const Sampler&) = default;
		Sampler(Sampler&&) noexcept = default;

		std::string_view getName() {
			return this->name_;
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE getCPUHandle() const {
			return this->cpuHandle_;
		}
		CD3DX12_GPU_DESCRIPTOR_HANDLE getGPUHandle() const {
			return this->gpuHandle_;
		}

		ShaderStage getStage() {
			return this->stage_;
		}

		uint32_t getIndex() const {
			return this->index_;
		}

		Sampler& operator=(const Sampler& other) = default;
		Sampler& operator=(Sampler&& other) noexcept = default;
	};

	struct ResourceBindingData {
		std::string			  name;
		D3D_SHADER_INPUT_TYPE type;
		uint32_t			  bindPoint;
		uint32_t			  bindCount;
		uint32_t			  space;

		ShaderStage stage;
	};

	struct ShaderData {
		Microsoft::WRL::ComPtr<ID3D12ShaderReflection> rawReflection;
		std::vector<ConstantBufferData>				   constantBuffers;
		std::vector<ShaderResourceViewData>		       shaderResourceViews;
		std::vector<SamplerData>					   samplers;
		std::vector<ResourceBindingData>               shaderResourceBindingData;
	};

	struct Mesh {
		std::unique_ptr<VertexArrayBuffer> vertexArrayBuffer;
		std::unique_ptr<IndexArrayBuffer>  indexArrayBuffer;
	};

	struct Renderizable {
		Mesh				 mesh;
		Texture2D			 texture;
		rendering::Transform transform;
	};

	struct Shader {
		std::wstring pathOrSource;

		Microsoft::WRL::ComPtr<IDxcBlob> shader;

		ShaderStage stage;
		ShaderData  data;
	};

	class SynchronizationObject {
	public:
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		std::vector<uint64_t>				values_;
		uint64_t							currentValue_;

		std::vector<HANDLE> handles_;

		size_t bufferCount_;

		SynchronizationObject() = default;
		SynchronizationObject(ID3D12Device* device,
							  const size_t  bufferCount) :
			currentValue_(0),
			bufferCount_(bufferCount)
		{
			device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));

			// Set values and its handles
			values_.reserve(bufferCount_);
			handles_.reserve(bufferCount_);

			// Populate handles
			for (size_t i = 0; i < bufferCount_; ++i) {
				values_.emplace_back();

				handles_.emplace_back();
				auto& handle = handles_.back();

				handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
				if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to create handle");
			}
		}
		SynchronizationObject(const SynchronizationObject& other) = delete;
		SynchronizationObject(SynchronizationObject&& other) noexcept :
			fence_(std::move(other.fence_)),
			values_(other.values_),
			currentValue_(other.currentValue_),
			handles_(other.handles_),
			bufferCount_(other.bufferCount_)
		{}

		~SynchronizationObject() {
			// Iterate over handles and close them
			for (size_t i = 0; i < bufferCount_; ++i) {
				if (handles_[i]) CloseHandle(handles_[i]);
			}
		}

		void wait(size_t index) {
			assert(index < bufferCount_);

			// Wait until the fence has been signaled
			if (fence_->GetCompletedValue() < values_[index]) {
				HANDLE& handle = this->handles_[index];
				fence_->SetEventOnCompletion(values_[index], handle);
				WaitForSingleObject(handle, INFINITE);
			}
		}

		void signal(ID3D12CommandQueue* queue, size_t index) {
			assert(index < bufferCount_);

			// Signal the fence
			currentValue_++;
			values_[index] = currentValue_;
			queue->Signal(fence_.Get(), currentValue_);
		}

		SynchronizationObject(SynchronizationObject&) = delete;
		SynchronizationObject& operator=(SynchronizationObject&& other) noexcept {
			if (this != &other) {
				fence_ = std::move(other.fence_);
				values_		  = other.values_;
				currentValue_ = other.currentValue_;
				handles_	  = other.handles_;
				bufferCount_  = other.bufferCount_;
			}
			return *this;
		}
	};

	struct DescriptorHeap {
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE				 cpuHandle;
		CD3DX12_GPU_DESCRIPTOR_HANDLE				 gpuHandle;

		D3D12_DESCRIPTOR_HEAP_TYPE  descriptorHeapType;
		D3D12_DESCRIPTOR_HEAP_FLAGS descriptorHeapFlags;

		uint_t descriptorHandleIncrementSize;

		size_t size;
		size_t capacity;
	};

	class HeapAllocator {
	private:
		Microsoft::WRL::ComPtr<ID3D12Device> device_;

		ska::flat_hash_map<std::string, std::unique_ptr<DescriptorHeap>> descriptorHeaps_;

		size_t defaultDescriptorHeapSize_;

		void reallocateDescriptorHeap(DescriptorHeap* descriptorHeap, 
									  size_t		  newCapacity) 
		{
			DescriptorHeap newDescriptorHeap				= {};
			newDescriptorHeap.capacity                      = newCapacity;
			newDescriptorHeap.size                          = descriptorHeap->size;
			newDescriptorHeap.descriptorHeapType            = descriptorHeap->descriptorHeapType;
			newDescriptorHeap.descriptorHeapFlags           = descriptorHeap->descriptorHeapFlags;
			newDescriptorHeap.descriptorHandleIncrementSize = device_->GetDescriptorHandleIncrementSize(newDescriptorHeap.descriptorHeapType);

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type						= newDescriptorHeap.descriptorHeapType;
			desc.NumDescriptors				= static_cast<uint_t>(newCapacity);
			desc.Flags						= newDescriptorHeap.descriptorHeapFlags;

			SPIDER_DX12_ERROR_CHECK(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&newDescriptorHeap.heap)));

			const size_t copyCount = std::min(descriptorHeap->size, newCapacity);
			if (copyCount) {
				device_->CopyDescriptorsSimple(
					static_cast<UINT>(copyCount),
					newDescriptorHeap.heap->GetCPUDescriptorHandleForHeapStart(),
					descriptorHeap->heap->GetCPUDescriptorHandleForHeapStart(),
					newDescriptorHeap.descriptorHeapType
				);
			}

			{
				CD3DX12_CPU_DESCRIPTOR_HANDLE cpu(newDescriptorHeap.heap->GetCPUDescriptorHandleForHeapStart());
				cpu.Offset(static_cast<INT>(copyCount), newDescriptorHeap.descriptorHandleIncrementSize);
				newDescriptorHeap.cpuHandle = cpu;

				CD3DX12_GPU_DESCRIPTOR_HANDLE gpu(newDescriptorHeap.heap->GetGPUDescriptorHandleForHeapStart());
				gpu.Offset(static_cast<INT>(copyCount), newDescriptorHeap.descriptorHandleIncrementSize);
				newDescriptorHeap.gpuHandle = gpu;
			}

			newDescriptorHeap.size = copyCount;

			*descriptorHeap = std::move(newDescriptorHeap);
		}

	public:
		HeapAllocator(Microsoft::WRL::ComPtr<ID3D12Device> device) :
			device_(device),
			defaultDescriptorHeapSize_(2048)
		{}
		HeapAllocator(Microsoft::WRL::ComPtr<ID3D12Device> device,
					  const size_t defaultDescriptorHeapSize) :
			device_(device),
			defaultDescriptorHeapSize_(defaultDescriptorHeapSize)
		{}
		HeapAllocator(const HeapAllocator&)     = delete;
		HeapAllocator(HeapAllocator&&) noexcept = default;

		DescriptorHeap* createDescriptorHeap(const std::string&                descriptorHeapName,
											 const D3D12_DESCRIPTOR_HEAP_TYPE  descriptorHeapType,
											 const D3D12_DESCRIPTOR_HEAP_FLAGS descriptorHeapFlags)
		{
			DescriptorHeap descriptorHeap;
			descriptorHeap.capacity					     = defaultDescriptorHeapSize_;
			descriptorHeap.descriptorHeapType			 = descriptorHeapType;
			descriptorHeap.descriptorHeapFlags		     = descriptorHeapFlags;
			descriptorHeap.descriptorHandleIncrementSize = device_->GetDescriptorHandleIncrementSize(descriptorHeapType);
			descriptorHeap.size                          = 0;

			D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
			descriptorHeapDesc.Type						  = descriptorHeapType;
			descriptorHeapDesc.NumDescriptors             = static_cast<uint_t>(defaultDescriptorHeapSize_);
			descriptorHeapDesc.Flags                      = descriptorHeapFlags;
			SPIDER_DX12_ERROR_CHECK(
				device_->CreateDescriptorHeap(
					&descriptorHeapDesc,
					IID_PPV_ARGS(&descriptorHeap.heap)
				)
			);

			descriptorHeap.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap.heap->GetCPUDescriptorHandleForHeapStart());
			descriptorHeap.gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap.heap->GetGPUDescriptorHandleForHeapStart());

			return descriptorHeaps_.emplace(
				descriptorHeapName,
				std::make_unique<DescriptorHeap>(descriptorHeap)
			).first->second.get();
		}
		DescriptorHeap* createDescriptorHeap(const std::string&                descriptorHeapName,
											 const size_t					   descriptorHeapSize,
											 const D3D12_DESCRIPTOR_HEAP_TYPE  descriptorHeapType,
											 const D3D12_DESCRIPTOR_HEAP_FLAGS descriptorHeapFlags)
		{
			DescriptorHeap descriptorHeap;
			descriptorHeap.capacity						 = descriptorHeapSize;
			descriptorHeap.descriptorHeapType			 = descriptorHeapType;
			descriptorHeap.descriptorHeapFlags			 = descriptorHeapFlags;
			descriptorHeap.descriptorHandleIncrementSize = device_->GetDescriptorHandleIncrementSize(descriptorHeapType);
			descriptorHeap.size                          = 0;

			D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
			descriptorHeapDesc.Type						  = descriptorHeapType;
			descriptorHeapDesc.NumDescriptors             = static_cast<uint_t>(descriptorHeapSize);
			descriptorHeapDesc.Flags                      = descriptorHeapFlags;
			SPIDER_DX12_ERROR_CHECK(
				device_->CreateDescriptorHeap(
					&descriptorHeapDesc,
					IID_PPV_ARGS(&descriptorHeap.heap)
				)
			);

			descriptorHeap.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap.heap->GetCPUDescriptorHandleForHeapStart());
			descriptorHeap.gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap.heap->GetGPUDescriptorHandleForHeapStart());

			return descriptorHeaps_.emplace(
				descriptorHeapName,
				std::make_unique<DescriptorHeap>(descriptorHeap)
			).first->second.get();
		}

		template <typename Fn, typename... Args>
		requires (CallableAs<Fn, void, DescriptorHeap*, Args...>)
		void writeOnDescriptorHeap(const std::string& id,
								   size_t			  operationCount,
								   Fn&&				  fn,
								   Args&&...		  args)
		{
			auto it = descriptorHeaps_.find(id);
			if (it == descriptorHeaps_.end()) throw std::runtime_error("Descriptor Heap not found");

			DescriptorHeap* descriptorHeap = it->second.get();

			descriptorHeap->size += operationCount;
			if (descriptorHeap->size > descriptorHeap->capacity) {
				reallocateDescriptorHeap(
					descriptorHeap,
					std::max(descriptorHeap->capacity * 2, descriptorHeap->size)
				);
			}

			for (size_t i = 0; i < operationCount; ++i) {
				fn(descriptorHeap, std::forward<Args>(args)...);
				descriptorHeap->cpuHandle.Offset(1, descriptorHeap->descriptorHandleIncrementSize);
				descriptorHeap->gpuHandle.Offset(1, descriptorHeap->descriptorHandleIncrementSize);
			}
		}
		template <typename Fn, typename... Args>
			requires (CallableAs<Fn, void, DescriptorHeap*, Args...>)
		void writeOnDescriptorHeap(DescriptorHeap* descriptorHeap,
								   size_t		   operationCount,
								   Fn&&			   fn,
								   Args&&...	   args)
		{
			descriptorHeap->size += operationCount;
			if (descriptorHeap->size > descriptorHeap->capacity) {
				reallocateDescriptorHeap(
					descriptorHeap,
					std::max(descriptorHeap->capacity * 2, descriptorHeap->size)
				);
			}

			for (size_t i = 0; i < operationCount; ++i) {
				fn(descriptorHeap, std::forward<Args>(args)...);

				descriptorHeap->cpuHandle.Offset(1, descriptorHeap->descriptorHandleIncrementSize);
				descriptorHeap->gpuHandle.Offset(1, descriptorHeap->descriptorHandleIncrementSize);
			}
		}

		void destroyDescriptorHeap(const std::string& descriptorHeapName) {
			auto it = descriptorHeaps_.find(descriptorHeapName);
			if (it == descriptorHeaps_.end()) return;
			it->second->heap->Release();
		}
		void destroyDescriptorHeap(DescriptorHeap* descriptorHeap) {
			descriptorHeap->heap->Release();
		}

		DescriptorHeap* getDescriptorHeap(const std::string& descriptorHeapName) {
			auto it = descriptorHeaps_.find(descriptorHeapName);
			if (it != descriptorHeaps_.end()) return it->second.get();
			throw std::runtime_error("Descriptor Heap not found.");
			return {};
		}

		HeapAllocator& operator=(const HeapAllocator&)     = delete;
		HeapAllocator& operator=(HeapAllocator&&) noexcept = default;
	};

	struct RenderPipelineRequirements {
		std::vector<std::string_view> constantBufferName;
		std::vector<ShaderStage>      constantBufferStage;
		std::vector<std::size_t>      constantBufferSize;

		std::vector<std::string_view> shaderResourceName;
		std::vector<ShaderStage>      shaderResourceStage;
	};

	class RenderPipeline {
	private:
		DX12Renderer* renderer_;

		ShaderResourceView(DX12Renderer::* createShaderResourceViewForStructuredDataFunction_)(
			const std::string&		    name,
			const std::vector<uint8_t>& data,
			const ShaderStage			stage
		);
		ShaderResourceView(DX12Renderer::* createShaderResourceViewForTexture2DFunction_)(
			const std::string& name,
			Texture2D&		   data,
			const ShaderStage  stage);

		std::vector<Shader> shaders_;

		ska::flat_hash_map<std::pair<std::string, ShaderStage>, ConstantBuffer>     requiredConstantBuffers_;
		ska::flat_hash_map<std::pair<std::string, ShaderStage>, ShaderResourceView> requiredShaderResourceViews_;
		ska::flat_hash_map<std::pair<std::string, ShaderStage>, Sampler>            requiredSamplers_;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		D3D12_RESOURCE_BARRIER                      barrier_;

	public:
		friend class DX12Renderer;
		friend class DX12Compiler;

		RenderPipelineRequirements getRequirements() {
			RenderPipelineRequirements requirements;

			for (auto& [key, constantBuffer] : requiredConstantBuffers_) {
				requirements.constantBufferName.push_back(key.first);
				requirements.constantBufferStage.push_back(key.second);
				requirements.constantBufferSize.push_back(constantBuffer.getSizeInBytes());
			}
			for (auto& [key, shaderResource] : requiredShaderResourceViews_) {
				requirements.shaderResourceName.push_back(key.first);
				requirements.shaderResourceStage.push_back(key.second);
			}

			return requirements;
		}

		template <typename Ty>
		void bindBuffer(const std::string& name,
						const ShaderStage  stage,
						Ty&&			   data)
		{
			auto it = requiredConstantBuffers_.find(std::make_pair(name, stage));
			if (it == requiredConstantBuffers_.end()) {
				throw std::runtime_error("Could not find buffer");
				return;
			}

			it->second.copy(std::forward<Ty>(data));
		}

		template <typename Ty>
		void bindShaderResource(const std::string& name,
								const ShaderStage  stage,
								Ty&&               data)
		{
			auto it = requiredShaderResourceViews_.find(std::make_pair(name, stage));
			if (it == requiredShaderResourceViews_.end()) {
				throw std::runtime_error("Could not find shader resource");
				return;
			}

			it->second = (renderer_->*createShaderResourceViewForStructuredDataFunction_)(name, data, stage);
		}
		void bindShaderResourceForTexture2D(const std::string& name,
								            const ShaderStage  stage,
								            Texture2D&         data)
		{
			auto it = requiredShaderResourceViews_.find(std::make_pair(name, stage));
			if (it == requiredShaderResourceViews_.end()) {
				throw std::runtime_error("Could not find shader resource");
				return;
			}

			it->second = (renderer_->*createShaderResourceViewForTexture2DFunction_)(name, data, stage);
		}

		ConstantBuffer* getBufferPtr(const std::string& name,
									 const ShaderStage  stage) noexcept
		{
			auto it = requiredConstantBuffers_.find(std::make_pair(name, stage));
			if (it == requiredConstantBuffers_.end()) {
				throw std::runtime_error("Could not find buffer");
				return nullptr;
			}
			return &it->second;
		}

		ShaderResourceView* getShaderResourcePtr(const std::string& name,
												 const ShaderStage  stage) noexcept
		{
			auto it = requiredShaderResourceViews_.find(std::make_pair(name, stage));
			if (it == requiredShaderResourceViews_.end()) {
				throw std::runtime_error("Could not find shader resource");
				return nullptr;
			}
			return &it->second;
		}
	};
}