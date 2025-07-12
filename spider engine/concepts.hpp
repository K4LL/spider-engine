#pragma once
#include <type_traits>
#include <concepts>

#include "policies.hpp"

namespace spider_engine {
	template <typename Ty>
	concept NothrowMoveConstructible = std::is_nothrow_move_constructible_v<Ty>;

	template <typename Ty>
	concept MoveConstructible = std::is_move_constructible_v<Ty>;

	template <typename Ty>
	concept CopyConstructible = std::is_copy_constructible_v<Ty>;

	template <typename Ty>
	concept TriviallyCopyable = std::is_trivially_copyable_v<Ty>;

	template <typename FirstTy, typename SecondTy>
	concept SameAs = std::is_same_v<FirstTy, SecondTy>;

	template <typename FirstTy, typename SecondTy>
	concept DifferentFrom = std::is_same_v<FirstTy, SecondTy> == false;
}