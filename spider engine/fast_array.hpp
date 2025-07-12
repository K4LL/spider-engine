#pragma once
#include <type_traits>
#include <memory>
#include <iterator>
#include <cassert>

#include "concepts.hpp"

namespace spider_engine {
	template <typename Ty, typename Allocator = std::allocator<Ty>>
	class FastArray {
	private:
		Ty* storage_;

		size_t size_;
		size_t capacity_;

		Allocator allocator_;

		void moveOrCopyElements(Ty* dest) requires (NothrowMoveConstructible<Ty>) {
			std::uninitialized_move_n(this->storage_, this->size_, dest);
		}
		void moveOrCopyElements(Ty* dest) requires (!MoveConstructible<Ty>&& TriviallyCopyable<Ty>) {
			std::memcpy(dest, this->storage_, this->size_ * sizeof(Ty));
		}
		void moveOrCopyElements(Ty* dest) requires (!NothrowMoveConstructible<Ty>&& CopyConstructible<Ty>) {
			std::uninitialized_copy_n(this->storage_, this->size_, dest);
		}

		void copyElements(Ty* dest) requires (TriviallyCopyable<Ty>) {
			std::memcpy(dest, this->storage_, this->size_ * sizeof(Ty));
		}
		void copyElements(Ty* dest) requires (!TriviallyCopyable<Ty>&& CopyConstructible<Ty>) {
			std::uninitialized_copy_n(this->storage_, this->size_, dest);
		}

		void resizeImpl(const size_t newCapacity) {
			Ty* buff = this->allocator_.allocate(newCapacity);

			assert(buff);
			if (this->storage_) {
				this->moveOrCopyElements(buff);

				if constexpr (!std::is_trivially_destructible_v<Ty>) std::destroy_n(this->storage_, this->size_);
				this->allocator_.deallocate(this->storage_, this->capacity_);
			}

			this->storage_ = buff;
			this->capacity_ = newCapacity;
		}

	public:
		using Iterator = Ty*;
		using ConstIterator = const Ty*;

		FastArray() noexcept : storage_(nullptr), size_(0), capacity_(0) {
			this->resizeImpl(1);
		}
		FastArray(const size_t initialCapacity) : storage_(nullptr), size_(0), capacity_(initialCapacity) {
			this->resizeImpl(initialCapacity);
		}
		FastArray(const std::initializer_list<Ty>& initList) : storage_(nullptr), size_(initList.size()), capacity_(initList.size() * 2) {
			this->storage_ = this->allocator_.allocate(this->capacity_);
			if constexpr (std::is_trivially_copyable_v<Ty>) {
				std::memcpy(this->storage_, initList.begin(), this->size_ * sizeof(Ty));
			}
			else {
				std::uninitialized_copy(initList.begin(), initList.end(), this->storage_);
			}
		}
		FastArray(Ty* storage, size_t size, size_t capacity = size) : storage_(storage), size_(size), capacity_(capacity) {}
		FastArray(const FastArray& other) : storage_(nullptr), size_(other.size_), capacity_(other.capacity_) {
			if (this->size_ > 0) {
				this->storage_ = this->allocator_.allocate(this->capacity_);
				assert(this->storage_);
				if constexpr (std::is_trivially_copyable_v<Ty>) memcpy(this->storage_, other.storage_, other.size_ * sizeof(Ty));
				else std::uninitialized_copy_n(other.storage_, other.size_, this->storage_);
			}
		}
		FastArray(FastArray&& other) noexcept :
			storage_(other.storage_), size_(other.size_), capacity_(other.capacity_)
		{
			other.storage_  = nullptr;
			other.size_     = 0;
			other.capacity_ = 0;
		}

		~FastArray() {
			if (this->storage_) {
				if constexpr (!std::is_trivially_destructible_v<Ty>) std::destroy_n(this->storage_, this->size_);
				this->allocator_.deallocate(this->storage_, this->capacity_);
			}
		}

		void resize(const size_t newSize) {
			this->resizeImpl(newSize);
		}

		template <typename Ty>
		void pushBack(Ty&& value) {
			if (this->size_ + 1 > this->capacity_) this->resizeImpl(this->capacity_ << 1);

			new (this->storage_ + this->size_) Ty(std::forward<Ty>(value));
			++this->size_;
		}

		template <typename... Args>
		void emplaceBack(Args&&... args) {
			if (this->size_ + 1 > this->capacity_) this->resizeImpl(this->capacity_ << 1);

			new (this->storage_ + this->size_) Ty(std::forward<Args>(args)...);
			++this->size_;
		}

		void pop() {
			assert(this->size_ > 0);
			std::destroy_at(this->storage_ + this->size_ - 1);
			--this->size_;
		}

		void erase(Ty* where) {
			assert(where >= this->begin() && where < this->end());
			for (auto it = where; it != this->end() - 1; ++it) {
				*it = std::move(*(it + 1));
			}
			std::destroy_at(this->storage_ + this->size_ - 1);
			--this->size_;
		}

		void clear() {
			if constexpr (std::is_trivially_destructible_v<Ty>) {
				memset(this->storage_, 0, this->capacity_ * sizeof(Ty));
				this->size_ = 0;
				return;
			}
			for (int i = 0; i < this->size_; ++i) {
				std::destroy_at(this->storage_ + i);
			}
			this->size_ = 0;
		}

		Iterator begin() {
			return this->storage_;
		}
		Iterator end() {
			return this->storage_ + this->size_;
		}
		ConstIterator cbegin() const {
			return this->storage_;
		}
		ConstIterator cend() const {
			return this->storage_ + this->size_;
		}

		Ty* data() { 
			return this->storage_; 
		}
		const Ty* data() const { 
			return this->storage_; 
		}

		size_t size() const {
			return this->size_;
		}
		size_t capacity() const {
			return this->capacity_;
		}

		Ty& at(const size_t index) {
			if (index >= this->size_) throw std::out_of_range("Index out of range");
			return this->storage_[index];
		}

		Ty& back() {
			assert(this->size_ > 0);
			return this->storage_[this->size_ - 1];
		}
		Ty& front() {
			assert(this->size_ > 0);
			return this->storage_[0];
		}

		Ty& operator[](const size_t index) {
			return this->storage_[index];
		}
		const Ty& operator[](const size_t index) const {
			return this->storage_[index];
		}

		FastArray& operator=(std::initializer_list<Ty> initList) {
			// Se já tem dados, destrói e desaloca
			if (this->storage_) {
				if constexpr (!std::is_trivially_destructible_v<Ty>)
					std::destroy_n(this->storage_, this->size_);
				this->allocator_.deallocate(this->storage_, this->capacity_);
				this->storage_ = nullptr;
			}

			this->size_ = initList.size();
			this->capacity_ = this->size_ * 2; // sua regra de capacidade

			if (this->capacity_ > 0) {
				this->storage_ = this->allocator_.allocate(this->capacity_);
				assert(this->storage_);

				if constexpr (std::is_trivially_copyable_v<Ty>) {
					std::memcpy(this->storage_, initList.begin(), this->size_ * sizeof(Ty));
				}
				else {
					std::uninitialized_copy(initList.begin(), initList.end(), this->storage_);
				}
			}

			return *this;
		}
		FastArray& operator=(const FastArray& other) {
			if (this != &other) {
				if (this->storage_) {
					if constexpr (!std::is_trivially_destructible_v<Ty>) std::destroy_n(this->storage_, this->size_);
					this->allocator_.deallocate(this->storage_, this->capacity_);
				}

				this->size_ = other.size_;
				this->capacity_ = other.capacity_;

				if (this->size_ > 0) {
					this->storage_ = this->allocator_.allocate(this->capacity_);
					assert(this->storage_);
					if constexpr (std::is_trivially_copyable_v<Ty>) memcpy(this->storage_, other.storage_, other.size_ * sizeof(Ty));
					else std::uninitialized_copy_n(other.storage_, other.size_, this->storage_);
				}
			}
			return *this;
		}
		FastArray& operator=(FastArray&& other) noexcept {
			if (this != &other) {
				if (this->storage_) {
					if constexpr (!std::is_trivially_destructible_v<Ty>) std::destroy_n(this->storage_, this->size_);
					this->allocator_.deallocate(this->storage_, this->capacity_);
				}

				this->storage_ = other.storage_;
				this->size_ = other.size_;
				this->capacity_ = other.capacity_;

				other.storage_ = nullptr;
				other.size_ = 0;
				other.capacity_ = 0;
			}
			return *this;
		}
	};
}