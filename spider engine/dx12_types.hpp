#pragma once
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

#include "d3dx12.h"

#include "dxcapi.h"
#include "d3d12shader.h"

#include "fast_array.hpp"
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
}

namespace std {
	template<>
	struct hash<std::pair<std::string, spider_engine::d3dx12::ShaderStage>> {
		std::size_t operator()(const std::pair<std::string, spider_engine::d3dx12::ShaderStage>& k) const {
			return std::hash<std::string>()(k.first) ^ (std::hash<uint8_t>()(static_cast<uint8_t>(k.second)) << 1);
		}
	};
} 

#include "flat_hash_map.hpp"

namespace spider_engine::d3dx12 {
	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

	const D3D12_INPUT_ELEMENT_DESC psInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	class DescriptorHeap {
	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
		CD3DX12_CPU_DESCRIPTOR_HANDLE				 cpuHandle_;
		CD3DX12_GPU_DESCRIPTOR_HANDLE				 gpuHandle_;
		const size_t								 capacity_;
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

	struct Texture {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t							   width;
		uint32_t							   height;
		uint32_t							   depth;
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

		FastArray<ConstantBufferVariable> variables;
	};

	struct ShaderResourceViewData {
		std::string name;
		uint32_t    size;

		uint32_t bindPoint;
		uint32_t space;

		ShaderStage stage;
	};

	class ShaderDescription {
	private:
		FastArray<FastArray<uint8_t>> shaderResourceViewData_;

	public:
		std::wstring pathOrSource;
		ShaderStage  stage;

		ShaderDescription() = default;
		ShaderDescription(std::wstring pathOrSource, 
						  ShaderStage  stage) : 
			pathOrSource(pathOrSource), stage(stage) 
		{}
		ShaderDescription(FastArray<FastArray<uint8_t>> shaderResourceViewData, 
						  ShaderStage				    stage) : 
			shaderResourceViewData_(shaderResourceViewData), stage(stage) 
		{}
		ShaderDescription(const ShaderDescription&)     = default;
		ShaderDescription(ShaderDescription&&) noexcept = default;

		template <typename Ty>
		void setShaderResourceViewData(FastArray<FastArray<Ty>>& data) {
			shaderResourceViewData_ = data;
		}
		template <typename Ty>
		void addShaderResourceViewData(FastArray<Ty>& data) {
			shaderResourceViewData_.pushBack(data);
		}

		template <typename Ty>
		FastArray<FastArray<Ty>> getShaderResourceViewData() {
			return FastArray<FastArray<Ty>>(shaderResourceViewData_);
		}
		FastArray<FastArray<uint8_t>>& getShaderResourceViewRawData() {
			return shaderResourceViewData_;
		}
		const FastArray<FastArray<uint8_t>>& getConstShaderResourceViewRawData() const {
			return shaderResourceViewData_;
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

		ConstantBuffer()                          = default;
		ConstantBuffer(const ConstantBuffer&)     = default;
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

		std::string getName() {
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
		ConstantBuffer& operator=(ConstantBuffer&& other) = default;
	};
	struct ConstantBuffers {
		ConstantBuffer* begin;
		ConstantBuffer* end;

		const size_t getSize() const {
			return static_cast<size_t>(end - begin);
		}
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

		std::string getName() {
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
		uint32_t getIndex() const {
			return this->index_;
		}

		ShaderResourceView& operator=(const ShaderResourceView& other) = default;
		ShaderResourceView& operator=(ShaderResourceView&& other)      = default;
	};
	struct ShaderResourceViews {
		ShaderResourceView* begin;
		ShaderResourceView* end;

		const size_t getSize() const {
			return static_cast<size_t>(end - begin);
		}
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
		FastArray<ConstantBufferData>				   constantBuffers;
		FastArray<ShaderResourceViewData>		       shaderResourceViews;
		FastArray<ResourceBindingData>                 shaderResourceBindingData;
	};

	struct Mesh {
		std::unique_ptr<VertexArrayBuffer> vertexArrayBuffer;
		std::unique_ptr<IndexArrayBuffer>  indexArrayBuffer;
	};

	struct Renderizable {
		Mesh				 mesh;
		rendering::Transform transform;
	};

	struct Shader {
		std::wstring pathOrSource;

		Microsoft::WRL::ComPtr<IDxcBlob> shader;

		ShaderStage stage;
		ShaderData  data;
	};
	class RenderPipeline {
	private:
		DX12Renderer* renderer_;

		FastArray<Shader> shaders_;

		ska::flat_hash_map<std::pair<std::string, ShaderStage>, ConstantBuffer>     requiredConstantBuffers_;
		ska::flat_hash_map<std::pair<std::string, ShaderStage>, ShaderResourceView> requiredShaderResourceViews_;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		D3D12_RESOURCE_BARRIER                      barrier_;

	public:
		friend class DX12Renderer;
		friend class DX12Compiler;

		FastArray<std::tuple<std::string, ShaderStage, size_t>> getRequirements() {
			FastArray<std::tuple<std::string, ShaderStage, size_t>> result;

			for (auto& [key, constantBuffer] : requiredConstantBuffers_) {
				result.pushBack(std::make_tuple(
					constantBuffer.getName(),
					constantBuffer.getStage(),
					constantBuffer.getSizeInBytes()
				));
			}

			return result;
		}
		template <typename Ty, typename Policy = DefaultPolicy>
		requires (SameAs<Policy, DefaultPolicy>)
		void bindBuffer(const std::string& name, const ShaderStage stage, Ty&& data) {
			auto it = requiredConstantBuffers_.find(std::make_pair(name, stage));
			if (it == requiredConstantBuffers_.end()) {
				throw std::runtime_error("Could not find buffer");
				return;
			}

			it->second.copy(std::forward<Ty>(data));
		}
		template <typename Ty, typename Policy = DefaultPolicy>
		requires (SameAs<Policy, NoThrowPolicy>)
		void bindBuffer(const std::string& name, const ShaderStage stage, Ty&& data) noexcept {
			auto it = requiredConstantBuffers_.find(std::make_pair(name, stage));
			if (it == requiredConstantBuffers_.end()) return;

			it->second.copy(std::forward<Ty>(data));
		}
		template <typename Policy = DefaultPolicy>
		requires (SameAs<Policy, DefaultPolicy>)
		ConstantBuffer* getBufferPtr(const std::string& name, const ShaderStage stage) noexcept {
			auto it = requiredConstantBuffers_.find(std::make_pair(name, stage));
			if (it == requiredConstantBuffers_.end()) {
				throw std::runtime_error("Could not find buffer");
				return nullptr;
			}
			return &it->second;
		}
		template <typename Policy = DefaultPolicy>
		requires (SameAs<Policy, NoThrowPolicy>)
		ConstantBuffer* getBufferPtr(const std::string& name, const ShaderStage stage) noexcept {
			auto it = requiredConstantBuffers_.find(std::make_pair(name, stage));
			if (it == requiredConstantBuffers_.end()) return nullptr;

			return &it->second;
		}

		template <typename Ty, typename Policy = DefaultPolicy>
		requires (SameAs<Policy, DefaultPolicy>)
		void bindResourceView(const std::string& name, const ShaderStage stage, Ty&& data) {
			auto it = requiredShaderResourceViews_.find(std::make_pair(name, stage));
			if (it == requiredShaderResourceViews_.end()) {
				throw std::runtime_error("Could not find resource view");
				return;
			}


		}
	};
}