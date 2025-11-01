#pragma once
#include <utility>
#include <concepts>
#include <wrl/client.h>

namespace spider_engine::d3dx12::concept_impl {
	template <typename Ty>
	concept HasResourceImpl = requires(Ty ty) {
		ty.resource;
	};
	template <typename Ty>
	concept HasResourceAsDX12ResourceImpl = requires(Ty ty) {
		{ ty.resource } -> std::same_as<ID3D12Resource*>;
	};
	template <typename Ty>
	concept HasResourceAsEngineStandartDX12ResourceImpl = requires (Ty ty) {
		{ ty.resource } -> std::same_as<Microsoft::WRL::ComPtr<ID3D12Resource>>;
	};
	template <typename Ty>
	consteval void hasResourceCheck() {
		if constexpr (!HasResourceImpl<Ty>) {
			static_assert(false, "Type is missing resource data memeber.");
		}
	}
	template <typename Ty>
	consteval void hasResourceAsDX12ResourceCheck() {
		if constexpr (!HasResourceImpl<Ty>) {
			static_assert(false, "Type is missing resource data member.");
			break;
		}
		else if constexpr (!HasResourceAsDX12ResourceImpl<Ty>) {
			static_assert(false, "Type resource data member must be ID3D12Resource*.");
		}
	}
	template <typename Ty>
	consteval void hasResourceAsEngineStandartDX12ResourceCheck() {
		if constexpr (!HasResourceImpl<Ty>) {
			static_assert(false, "Type is missing resource data member.");
			break;
		}
		else if constexpr (!HasResourceAsEngineStandartDX12ResourceImpl<Ty>) {
			static_assert(false, "Type resource data member must be ComPtr<ID3D12Resource>.");
		}
	}
}

namespace spider_engine::d3dx12 {
	using namespace spider_engine::d3dx12::concept_impl;

	template <typename Ty>
	concept HasDX12Resource = (hasResourceCheck<Ty>(), true);
	template <typename Ty>
	concept HasResourceAsDX12Resource = (hasResourceAsDX12ResourceCheck<Ty>(), true);
	template <typename Ty>
	concept hasResourceAsEngineStandartDX12Resource = (hasResourceAsEngineStandartDX12ResourceCheck<Ty>(), true);
}