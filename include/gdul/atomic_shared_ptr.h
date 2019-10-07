// Copyright(c) 2019 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <atomic>
#include <stdint.h>
#include <limits>

#undef max

#pragma warning(push)
#pragma warning(disable : 4201)

namespace gdul {

namespace aspdetail {

typedef std::allocator<uint8_t> default_allocator;

static constexpr uint64_t Ptr_Mask = (std::numeric_limits<uint64_t>::max() >> 16);
static constexpr uint64_t Versioned_Ptr_Mask = (std::numeric_limits<uint64_t>::max() >> 8);

union compressed_storage
{
	compressed_storage() : myU64(0ULL) {}
	compressed_storage(uint64_t from) : myU64(from) {}

	std::uint64_t myU64;
	std::uint32_t myU32[2];
	std::uint16_t myU16[4];
	std::uint8_t myU8[8];
};

template <class T>
struct disable_deduction;

template <class T, class Allocator>
class control_block_base;
template <class T, class Allocator>
class control_block_make_shared;
template <class T, class Allocator>
class control_block_claim;
template <class T, class Allocator, class Deleter>
class control_block_claim_custom_delete;

template <class T, class Allocator>
class ptr_base;

enum STORAGE_BYTE : uint8_t;
enum CAS_FLAG : uint8_t;

static constexpr uint8_t CopyRequestIndex(7);
static constexpr uint64_t Copy_Request_Step(1ULL << (CopyRequestIndex * 8));
}
template <class T, class Allocator = aspdetail::default_allocator>
class shared_ptr;

template <class T, class Allocator = aspdetail::default_allocator>
class versioned_raw_ptr;

template <class T, class ...Args>
inline shared_ptr<T> make_shared(Args&&...);
template <class T, class ...Args>
inline shared_ptr<T, aspdetail::default_allocator> make_shared(Args&&...);
template <class T, class Allocator, class ...Args>
inline shared_ptr<T, Allocator> make_shared(Allocator&, Args&&...);

template <class T, class Allocator = aspdetail::default_allocator>
class atomic_shared_ptr
{
public:
	typedef typename aspdetail::ptr_base<T, Allocator>::size_type size_type;

	inline constexpr atomic_shared_ptr();
	inline constexpr atomic_shared_ptr(std::nullptr_t);

	inline atomic_shared_ptr(const shared_ptr<T, Allocator>& from);
	inline atomic_shared_ptr(shared_ptr<T, Allocator>&& from);

	inline ~atomic_shared_ptr();

	inline bool compare_exchange_strong(versioned_raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired);
	inline bool compare_exchange_strong(versioned_raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired);

	inline bool compare_exchange_strong(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired);
	inline bool compare_exchange_strong(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired);

	inline atomic_shared_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& from);
	inline atomic_shared_ptr<T, Allocator>& operator=(shared_ptr<T, Allocator>&& from);

	inline shared_ptr<T, Allocator> load();

	inline void store(const shared_ptr<T, Allocator>& from);
	inline void store(shared_ptr<T, Allocator>&& from);

	inline shared_ptr<T, Allocator> exchange(const shared_ptr<T, Allocator>& with);
	inline shared_ptr<T, Allocator> exchange(shared_ptr<T, Allocator>&& with);

	inline shared_ptr<T, Allocator> unsafe_load();

	inline shared_ptr<T, Allocator> unsafe_exchange(const shared_ptr<T, Allocator>& with);
	inline shared_ptr<T, Allocator> unsafe_exchange(shared_ptr<T, Allocator>&& with);

	inline void unsafe_store(const shared_ptr<T, Allocator>& from);
	inline void unsafe_store(shared_ptr<T, Allocator>&& from);

	operator bool() const;
	bool operator==(const aspdetail::ptr_base<T, Allocator>& other) const;

	size_type unsafe_use_count() const;

	versioned_raw_ptr<T, Allocator> unsafe_get_versioned_raw_ptr() const;

	T* unsafe_get_owned();
	const T* unsafe_get_owned() const;

	const aspdetail::control_block_base<T, Allocator>* unsafe_get_control_block() const;
	aspdetail::control_block_base<T, Allocator>* unsafe_get_control_block();

private:
	typedef typename aspdetail::compressed_storage compressed_storage;
	typedef typename aspdetail::ptr_base<T, Allocator>::size_type size_type;

	inline compressed_storage copy_internal();
	inline compressed_storage unsafe_copy_internal();
	inline compressed_storage unsafe_exchange_internal(compressed_storage with);

	inline void unsafe_store_internal(compressed_storage from);
	inline void store_internal(compressed_storage from);

	inline compressed_storage exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags);

	template <class PtrType>
	inline bool compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T, Allocator>&& desired);

	inline bool try_help_increment_and_try_swap(compressed_storage& expected, compressed_storage desired, uint64_t compareMask);

	inline bool cas_internal(compressed_storage& expected, compressed_storage desired, aspdetail::CAS_FLAG flags);

	inline void try_help_increment(compressed_storage expected);

	inline constexpr aspdetail::control_block_base<T, Allocator>* get_control_block() const;
	inline constexpr aspdetail::control_block_base<T, Allocator>* to_control_block(compressed_storage from) const;

	friend class shared_ptr<T, Allocator>;
	friend class versioned_raw_ptr<T, Allocator>;
	friend class aspdetail::ptr_base<T, Allocator>;

	union
	{
		std::atomic<uint64_t> myStorage;
		const compressed_storage myDebugView;
	};
};
template <class T, class Allocator>
inline constexpr atomic_shared_ptr<T, Allocator>::atomic_shared_ptr()
	: myStorage{ 0ULL }
{
	static_assert(std::is_same<Allocator::value_type, uint8_t>(), "value_type for allocator must be uint8_t");
}
template<class T, class Allocator>
inline constexpr atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(std::nullptr_t)
	: atomic_shared_ptr<T, Allocator>()
{
}
template <class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(shared_ptr<T, Allocator>&& other)
	: atomic_shared_ptr<T, Allocator>()
{
	unsafe_store(std::move(other));
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(const shared_ptr<T, Allocator>& from)
	: atomic_shared_ptr<T, Allocator>()
{
	unsafe_store(from);
}
template <class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::~atomic_shared_ptr()
{
	unsafe_store_internal(compressed_storage());
}

template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(versioned_raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired)
{
	return compare_exchange_strong(expected, shared_ptr<T, Allocator>(desired));
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(versioned_raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired)
{
	return compare_exchange_strong<decltype(expected)>(expected, std::move(desired));
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired)
{
	return compare_exchange_strong(expected, shared_ptr<T, Allocator>(desired));
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired)
{
	return compare_exchange_strong<decltype(expected)>(expected, std::move(desired));
}
template<class T, class Allocator>
template<class PtrType>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type & expected, shared_ptr<T, Allocator>&& desired)
{
	const compressed_storage desired_(desired.myControlBlockStorage.myU64);
	compressed_storage expected_(expected.myControlBlockStorage.myU64);

	typedef typename std::remove_reference<PtrType>::type raw_type;

	const uint64_t compareBlock(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);

	const bool sharedPtrVersion(static_cast<bool>(std::is_same<raw_type, shared_ptr<T, Allocator>>()));
	const uint8_t flags(aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE * static_cast<uint8_t>(sharedPtrVersion));

	do {
		if (cas_internal(expected_, desired_, static_cast<aspdetail::CAS_FLAG>(flags))) {

			desired.reset();

			return true;
		}

	} while (compareBlock == (expected_.myU64 & aspdetail::Versioned_Ptr_Mask));

	expected = raw_type(expected_);

	return false;
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>& atomic_shared_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& from)
{
	store(from);
	return *this;
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>& atomic_shared_ptr<T, Allocator>::operator=(shared_ptr<T, Allocator>&& from)
{
	store(std::move(from));
	return *this;
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::load()
{
	compressed_storage copy(copy_internal());
	return shared_ptr<T, Allocator>(copy);
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store(const shared_ptr<T, Allocator>& from)
{
	store(shared_ptr<T, Allocator>(from));
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store(shared_ptr<T, Allocator>&& from)
{
	store_internal(from.myControlBlockStorage.myU64);
	from.reset();
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::exchange(const shared_ptr<T, Allocator>& with)
{
	return exchange(shared_ptr<T, Allocator>(with));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::exchange(shared_ptr<T, Allocator>&& with)
{
	compressed_storage previous(exchange_internal(with.myControlBlockStorage, aspdetail::CAS_FLAG_STEAL_PREVIOUS));
	with.reset();
	return shared_ptr<T, Allocator>(previous);
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_load()
{
	return shared_ptr<T, Allocator>(unsafe_copy_internal());
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_exchange(const shared_ptr<T, Allocator>& with)
{
	return unsafe_exchange(shared_ptr<T, Allocator>(with));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_exchange(shared_ptr<T, Allocator>&& with)
{
	const compressed_storage previous(unsafe_exchange_internal(with.myControlBlockStorage));
	with.reset();
	return shared_ptr<T, Allocator>(previous);
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store(const shared_ptr<T, Allocator>& from)
{
	unsafe_store(shared_ptr<T, Allocator>(from));
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store(shared_ptr<T, Allocator>&& from)
{
	unsafe_store_internal(from.myControlBlockStorage);
	from.reset();
}
template<class T, class Allocator>
inline typename atomic_shared_ptr<T, Allocator>::size_type atomic_shared_ptr<T, Allocator>::unsafe_use_count() const
{
	aspdetail::control_block_base<T, Allocator>* const cb(get_control_block());
	if (cb)
	{
		return cb->use_count();
	}
	return 0;
}
template<class T, class Allocator>
inline versioned_raw_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_get_versioned_raw_ptr() const
{
	compressed_storage storage(myStorage.load(std::memory_order_acquire));
	return versioned_raw_ptr<T, Allocator>(storage);
}
template<class T, class Allocator>
inline T * atomic_shared_ptr<T, Allocator>::unsafe_get_owned()
{
	aspdetail::control_block_base<T, Allocator>* const cb(get_control_block());
	if (cb) {
		return cb->get_owned();
	}
	return nullptr;
}
template<class T, class Allocator>
inline const T * atomic_shared_ptr<T, Allocator>::unsafe_get_owned() const
{
	aspdetail::control_block_base<T, Allocator>* const cb(get_control_block());
	if (cb)
	{
		return cb->get_owned();
	}
	return nullptr;
}
template<class T, class Allocator>
inline const aspdetail::control_block_base<T, Allocator>* atomic_shared_ptr<T, Allocator>::unsafe_get_control_block() const
{
	return get_control_block();
}
template<class T, class Allocator>
inline aspdetail::control_block_base<T, Allocator>* atomic_shared_ptr<T, Allocator>::unsafe_get_control_block()
{
	return get_control_block();
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::operator bool() const
{
	return static_cast<bool>(myStorage.load(std::memory_order_acquire) & aspdetail::Ptr_Mask);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::operator==(const aspdetail::ptr_base<T, Allocator>& other) const
{
	return !((myStorage.load(std::memory_order_acquire) ^ other.myControlBlockStorage.myU64) & aspdetail::Versioned_Ptr_Mask);
}
// ------------------------------------------------------------------------------------

// -------------------------Internals--------------------------------------------------

// ------------------------------------------------------------------------------------
template <class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store_internal(compressed_storage from)
{
	exchange_internal(from, aspdetail::CAS_FLAG_NONE);
}
template <class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::try_help_increment_and_try_swap(compressed_storage & expected, compressed_storage desired, uint64_t compareMask)
{
	const compressed_storage initialPtrBlock(expected.myU64 & compareMask);

	aspdetail::control_block_base<T, Allocator>* const controlBlock(to_control_block(expected));

	compressed_storage desired_(desired);
	do {
		const uint16_t copyRequests(expected.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST]);

		if (controlBlock)
			controlBlock->incref(copyRequests);

		if (myStorage.compare_exchange_strong(expected.myU64, desired_.myU64, std::memory_order_acq_rel)) {
			return true;
		}

		if (controlBlock)
			controlBlock->decref(copyRequests);

	} while ((expected.myU64 & compareMask) == initialPtrBlock.myU64);

	return false;
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::try_help_increment(compressed_storage expected)
{
	const compressed_storage initialPtrBlock(expected.myU64 & aspdetail::Versioned_Ptr_Mask);

	aspdetail::control_block_base<T, Allocator>* const controlBlock(to_control_block(expected));

	if (!controlBlock) {
		return;
	}

	compressed_storage desired(expected);
	desired.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] = 0;
	compressed_storage expected_(expected);
	do {
		const uint8_t copyRequests(expected_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST]);

		controlBlock->incref(copyRequests);

		if (myStorage.compare_exchange_strong(expected_.myU64, desired.myU64)) {
			return;
		}
		controlBlock->decref(copyRequests);

	} while (
		(expected_.myU64 & aspdetail::Versioned_Ptr_Mask) == initialPtrBlock.myU64 &&
		expected_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST]);
}
template<class T, class Allocator>
inline constexpr aspdetail::control_block_base<T, Allocator>* atomic_shared_ptr<T, Allocator>::get_control_block() const
{
	return to_control_block(myStorage.load(std::memory_order_acquire));
}
template<class T, class Allocator>
inline constexpr aspdetail::control_block_base<T, Allocator>* atomic_shared_ptr<T, Allocator>::to_control_block(compressed_storage from) const
{
	return reinterpret_cast<aspdetail::control_block_base<T, Allocator>*>(from.myU64 & aspdetail::Ptr_Mask);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::cas_internal(compressed_storage & expected, compressed_storage desired, aspdetail::CAS_FLAG flags)
{
	bool success(false);

	const bool captureOnFail(flags & aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE);
	const bool stealPrevious(flags & aspdetail::CAS_FLAG_STEAL_PREVIOUS);

	compressed_storage desired_(desired);
	desired_.myU8[aspdetail::STORAGE_BYTE_VERSION] = expected.myU8[aspdetail::STORAGE_BYTE_VERSION] + 1;
	desired_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] = 0;

	compressed_storage expected_(expected);

	if (!expected_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST]) {
		success = myStorage.compare_exchange_strong(expected_.myU64, desired_.myU64);

		const bool decrementPrevious(success & !stealPrevious);

		if (decrementPrevious) {
			aspdetail::control_block_base<T, Allocator>* const controlBlock(to_control_block(expected_));
			if (controlBlock) {
				controlBlock->decref();
			}
		}
	}
	else {
		expected_ = myStorage.fetch_add(aspdetail::Copy_Request_Step, std::memory_order_acq_rel);
		expected_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] += 1;

		aspdetail::control_block_base<T, Allocator>* const controlBlock(to_control_block(expected_));

		const bool otherInterjection((expected_.myU64 ^ expected.myU64) & aspdetail::Versioned_Ptr_Mask);

		if (!otherInterjection) {
			success = try_help_increment_and_try_swap(expected_, desired_, aspdetail::Versioned_Ptr_Mask);
		}
		else {
			try_help_increment(expected_);
		}

		const bool decrementPrevious(success & !stealPrevious);

		if (controlBlock) {
			controlBlock->decref(1 + decrementPrevious);
		}
	}

	const bool otherInterjection((expected_.myU64 ^ expected.myU64) & aspdetail::Versioned_Ptr_Mask);

	expected = expected_;

	if (otherInterjection & captureOnFail) {
		expected = copy_internal();
	}

	return success;
}
template <class T, class Allocator>
inline typename  aspdetail::compressed_storage atomic_shared_ptr<T, Allocator>::copy_internal()
{
	compressed_storage initial(myStorage.fetch_add(aspdetail::Copy_Request_Step, std::memory_order_acq_rel));
	initial.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] += 1;

	if (to_control_block(initial)) {
		compressed_storage expected(initial);
		try_help_increment(expected);
	}

	initial.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] = 0;

	return initial;
}
template <class T, class Allocator>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T, Allocator>::unsafe_copy_internal()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	aspdetail::control_block_base<T, Allocator>* const cb(to_control_block(compressed_storage(myStorage._My_val)));
	if (cb) {
		cb->incref();
	}

	return compressed_storage(myStorage._My_val);
}
template<class T, class Allocator>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T, Allocator>::unsafe_exchange_internal(compressed_storage with)
{
	std::atomic_thread_fence(std::memory_order_acquire);

	const compressed_storage old(myStorage._My_val);

	compressed_storage replacement(with.myU64);
	replacement.myU8[aspdetail::STORAGE_BYTE_VERSION] = old.myU8[aspdetail::STORAGE_BYTE_VERSION] + 1;

	myStorage._My_val = replacement.myU64;

	std::atomic_thread_fence(std::memory_order_release);

	return old;
}
template <class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store_internal(compressed_storage from)
{
	std::atomic_thread_fence(std::memory_order_acquire);

	const compressed_storage previous(myStorage._My_val);
	myStorage._My_val = from.myU64;

	aspdetail::control_block_base<T, Allocator>* const prevCb(to_control_block(previous));
	if (prevCb) {
		prevCb->decref();
	}
	else {
		std::atomic_thread_fence(std::memory_order_release);
	}
}
template<class T, class Allocator>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T, Allocator>::exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags)
{
	compressed_storage expected(myStorage._My_val);
	while (!cas_internal(expected, to, flags));
	return expected;
}
namespace aspdetail {
template <class T, class Allocator>
class control_block_base
{
public:
	typedef typename atomic_shared_ptr<T>::size_type size_type;

	control_block_base(T* object, Allocator& allocator);

	T* get_owned();
	const T* get_owned() const;

	size_type use_count() const;

	void incref(size_type count = 1);
	void decref(size_type count = 1);

protected:
	virtual ~control_block_base() = default;
	virtual void destroy() = 0;

	friend class atomic_shared_ptr<T>;

	T* const myPtr;
	std::atomic<size_type> myUseCount;
	Allocator myAllocator;
};

template<class T, class Allocator>
inline control_block_base<T, Allocator>::control_block_base(T* object, Allocator& allocator)
	: myUseCount(1)
	, myPtr(object)
	, myAllocator(allocator)
{
}
template<class T, class Allocator>
inline void control_block_base<T, Allocator>::incref(size_type count)
{
	myUseCount.fetch_add(count, std::memory_order_relaxed);
}
template<class T, class Allocator>
inline void control_block_base<T, Allocator>::decref(size_type count)
{
	if (!(myUseCount.fetch_sub(count, std::memory_order_acq_rel) - count))
	{
		destroy();
	}
}
template <class T, class Allocator>
inline T* control_block_base<T, Allocator>::get_owned()
{
	return myPtr;
}
template<class T, class Allocator>
inline const T* control_block_base<T, Allocator>::get_owned() const
{
	return myPtr;
}
template <class T, class Allocator>
inline typename control_block_base<T, Allocator>::size_type control_block_base<T, Allocator>::use_count() const
{
	return myUseCount.load(std::memory_order_acquire);
}
template <class T, class Allocator>
class control_block_make_shared : public control_block_base<T, Allocator>
{
public:
	template <class ...Args>
	control_block_make_shared(Allocator& alloc, Args&& ...args);

	void destroy() override;
private:
	T myOwned;
};
template<class T, class Allocator>
template <class ...Args>
inline control_block_make_shared<T, Allocator>::control_block_make_shared(Allocator& alloc, Args&& ...args)
	: control_block_base<T, Allocator>(&myOwned, alloc)
	, myOwned(std::forward<Args&&>(args)...)
{
}
template<class T, class Allocator>
inline void control_block_make_shared<T, Allocator>::destroy()
{
	Allocator alloc(this->myAllocator);
	this->myPtr->~T();
	(*this).~control_block_make_shared<T, Allocator>();
	alloc.deallocate(reinterpret_cast<uint8_t*>(this), shared_ptr<T, Allocator>::alloc_size_make_shared());
}
template <class T, class Allocator>
class control_block_claim : public control_block_base<T, Allocator>
{
public:
	control_block_claim(T* obj, Allocator& alloc);
	void destroy() override;
};
template<class T, class Allocator>
inline control_block_claim<T, Allocator>::control_block_claim(T * obj, Allocator & alloc)
	:control_block_base<T, Allocator>(obj, alloc)
{
}
template<class T, class Allocator>
inline void control_block_claim<T, Allocator>::destroy()
{
	Allocator alloc(this->myAllocator);

	T* const ptrAddr(this->myPtr);
	this->myPtr->~T();
	(*this).~control_block_claim<T, Allocator>();

	alloc.deallocate(reinterpret_cast<uint8_t*>(this), sizeof(decltype(*this)));
	alloc.deallocate(reinterpret_cast<uint8_t*>(ptrAddr), sizeof(T));
}
template <class T, class Allocator, class Deleter>
class control_block_claim_custom_delete : public control_block_base<T, Allocator>
{
public:
	control_block_claim_custom_delete(T* obj, Allocator& alloc, Deleter&& deleter);
	void destroy() override;

private:
	Deleter myDeleter;
};
template<class T, class Allocator, class Deleter>
inline control_block_claim_custom_delete<T, Allocator, Deleter>::control_block_claim_custom_delete(T* obj, Allocator& alloc, Deleter && deleter)
	: control_block_base<T, Allocator>(obj, alloc)
	, myDeleter(std::forward<Deleter&&>(deleter))
{
}
template<class T, class Allocator, class Deleter>
inline void control_block_claim_custom_delete<T, Allocator, Deleter>::destroy()
{
	Allocator alloc(this->myAllocator);

	T* const ptrAddr(this->myPtr);
	(*this).~control_block_claim_custom_delete<T, Allocator, Deleter>();

	alloc.deallocate(reinterpret_cast<uint8_t*>(this), sizeof(decltype(*this)));

	myDeleter(ptrAddr, alloc);
}
template <class T>
struct disable_deduction
{
	using type = T;
};
enum STORAGE_BYTE : uint8_t
{
	STORAGE_BYTE_VERSION = 6,
	STORAGE_BYTE_COPYREQUEST = CopyRequestIndex,
};
enum CAS_FLAG : uint8_t
{
	CAS_FLAG_NONE = 0,
	CAS_FLAG_CAPTURE_ON_FAILURE,
	CAS_FLAG_STEAL_PREVIOUS,
};
template <class T, class Allocator>
class ptr_base
{
public:
	typedef std::uint32_t size_type;

	inline constexpr operator bool() const;

	inline constexpr bool operator==(const ptr_base<T, Allocator>& other) const;
	inline constexpr bool operator!=(const ptr_base<T, Allocator>& other) const;

	inline constexpr explicit operator T*();
	inline constexpr explicit operator const T*() const;

	inline constexpr const control_block_base<T, Allocator>* get_control_block() const;
	inline constexpr const T* get_owned() const;

	inline constexpr control_block_base<T, Allocator>* get_control_block();
	inline constexpr T* get_owned();

	inline constexpr versioned_raw_ptr<T, Allocator> get_versioned_raw_ptr() const;

	inline constexpr uint8_t get_version() const;

	inline size_type use_count() const;

	inline constexpr T* operator->();
	inline constexpr T& operator*();

	inline constexpr const T* operator->() const;
	inline constexpr const T& operator*() const;

	inline const T& operator[](size_type index) const;
	inline T& operator[](size_type index);

protected:
	inline constexpr ptr_base();
	inline constexpr ptr_base(compressed_storage from);

	inline void reset();

	constexpr control_block_base<T, Allocator>* to_control_block(compressed_storage from);
	constexpr T* to_object(compressed_storage from);

	constexpr const control_block_base<T, Allocator>* to_control_block(compressed_storage from) const;
	constexpr const T* to_object(compressed_storage from) const;

	friend class atomic_shared_ptr<T, Allocator>;

	compressed_storage myControlBlockStorage;
	T* myPtr;
};
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::ptr_base()
	: myPtr(nullptr)
{
}
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::ptr_base(compressed_storage from)
	: ptr_base<T, Allocator>()
{
	myControlBlockStorage = from;
	myControlBlockStorage.myU8[STORAGE_BYTE_COPYREQUEST] = 0;
	myPtr = to_object(from);
}
template<class T, class Allocator>
inline void ptr_base<T, Allocator>::reset()
{
	myControlBlockStorage = compressed_storage();
	myPtr = nullptr;
}
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::operator bool() const
{
	return get_owned();
}
template <class T, class Allocator>
inline constexpr bool ptr_base<T, Allocator>::operator==(const ptr_base<T, Allocator>& other) const
{
	return !((myControlBlockStorage.myU64 ^ other.myControlBlockStorage.myU64) & aspdetail::Versioned_Ptr_Mask);
}
template <class T, class Allocator>
inline constexpr bool ptr_base<T, Allocator>::operator!=(const ptr_base<T, Allocator>& other) const
{
	return !operator==(other);
}
template <class T, class Allocator>
inline typename ptr_base<T, Allocator>::size_type ptr_base<T, Allocator>::use_count() const
{
	if (!operator bool()) {
		return 0;
	}

	return get_control_block()->use_count();
}
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::operator T*()
{
	return get_owned();
}
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::operator const T*() const
{
	return get_owned();
}
template <class T, class Allocator>
inline constexpr T* ptr_base<T, Allocator>::operator->()
{
	return get_owned();
}
template <class T, class Allocator>
inline constexpr T & ptr_base<T, Allocator>::operator*()
{
	return *get_owned();
}
template <class T, class Allocator>
inline constexpr const T* ptr_base<T, Allocator>::operator->() const
{
	return get_owned();
}
template <class T, class Allocator>
inline constexpr const T & ptr_base<T, Allocator>::operator*() const
{
	return *get_owned();
}
template <class T, class Allocator>
inline const T & ptr_base<T, Allocator>::operator[](size_type index) const
{
	return(*get_owned())[index];
}
template <class T, class Allocator>
inline T & ptr_base<T, Allocator>::operator[](size_type index)
{
	return(*get_owned())[index];
}
template <class T, class Allocator>
inline constexpr bool operator==(std::nullptr_t /*aNullptr*/, const ptr_base<T, Allocator>& ptr)
{
	return !ptr;
}
template <class T, class Allocator>
inline constexpr bool operator!=(std::nullptr_t /*aNullptr*/, const ptr_base<T, Allocator>& ptr)
{
	return ptr;
}
template <class T, class Allocator>
inline constexpr bool operator==(const ptr_base<T, Allocator>& ptr, std::nullptr_t /*aNullptr*/)
{
	return !ptr;
}
template <class T, class Allocator>
inline constexpr bool operator!=(const ptr_base<T, Allocator>& ptr, std::nullptr_t /*aNullptr*/)
{
	return ptr;
}
template <class T, class Allocator>
inline constexpr control_block_base<T, Allocator>* ptr_base<T, Allocator>::to_control_block(compressed_storage from)
{
	return reinterpret_cast<control_block_base<T, Allocator>*>(from.myU64 & Ptr_Mask);
}
template <class T, class Allocator>
inline constexpr T* ptr_base<T, Allocator>::to_object(compressed_storage from)
{
	control_block_base<T, Allocator>* const cb(to_control_block(from));
	if (cb) {
		return cb->get_owned();
	}
	return nullptr;
}
template <class T, class Allocator>
inline constexpr const control_block_base<T, Allocator>* ptr_base<T, Allocator>::to_control_block(compressed_storage from) const
{
	return reinterpret_cast<const control_block_base<T, Allocator>*>(from.myU64 & Ptr_Mask);
}
template <class T, class Allocator>
inline constexpr const T* ptr_base<T, Allocator>::to_object(compressed_storage from) const
{
	control_block_base<T, Allocator>* const cb(to_control_block(from));
	if (cb)
	{
		return cb->get_owned();
	}
	return nullptr;
}
template <class T, class Allocator>
inline constexpr const control_block_base<T, Allocator>* ptr_base<T, Allocator>::get_control_block() const
{
	return to_control_block(myControlBlockStorage);
}
template <class T, class Allocator>
inline constexpr const T* ptr_base<T, Allocator>::get_owned() const
{
	return myPtr;
}
template <class T, class Allocator>
inline constexpr control_block_base<T, Allocator>* ptr_base<T, Allocator>::get_control_block()
{
	return to_control_block(myControlBlockStorage);
}
template <class T, class Allocator>
inline constexpr T* ptr_base<T, Allocator>::get_owned()
{
	return myPtr;
}
template<class T, class Allocator>
inline constexpr uint8_t ptr_base<T, Allocator>::get_version() const
{
	return myControlBlockStorage.myU8[STORAGE_BYTE_VERSION];
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator> ptr_base<T, Allocator>::get_versioned_raw_ptr() const
{
	return versioned_raw_ptr<T, Allocator>(myControlBlockStorage);
}
};
template <class T, class Allocator>
class shared_ptr : public aspdetail::ptr_base<T, Allocator>
{
public:
	inline constexpr shared_ptr();

	inline shared_ptr(const shared_ptr<T, Allocator>& other);
	inline shared_ptr(shared_ptr<T, Allocator>&& other);
	inline shared_ptr(std::nullptr_t);

	inline explicit shared_ptr(T* object);
	template <class Deleter>
	inline explicit shared_ptr(T* object, Deleter&& deleter, Allocator& allocator);

	~shared_ptr();

	shared_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& other);
	shared_ptr<T, Allocator>& operator=(shared_ptr<T, Allocator>&& other);

	// The amount of memory requested from the allocator when calling
	// make_shared
	static constexpr std::size_t alloc_size_make_shared();

	// The amount of memory requested from the allocator when taking 
	// ownership of an object using default deleter
	static constexpr std::size_t alloc_size_claim();

	// The amount of memory requested from the allocator when taking 
	// ownership of an object using a custom deleter

	template <class Deleter>
	static constexpr std::size_t alloc_size_claim_custom_delete();

private:
	typedef aspdetail::compressed_storage compressed_storage;

	shared_ptr(compressed_storage from);

	template<class Deleter>
	inline compressed_storage create_control_block(T* object, Deleter&& deleter, Allocator& allocator);
	inline compressed_storage create_control_block(T* object, Allocator& allocator);

	friend class versioned_raw_ptr<T, Allocator>;
	friend class atomic_shared_ptr<T, Allocator>;

	template <class T, class Allocator, class ...Args>
	friend shared_ptr<T, Allocator> make_shared<T, Allocator>(Allocator&, Args&&...);
};
template<class T, class Allocator>
inline constexpr shared_ptr<T, Allocator>::shared_ptr()
	: aspdetail::ptr_base<T, Allocator>()
{
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::~shared_ptr()
{
	aspdetail::control_block_base<T, Allocator>* const cb(this->get_control_block());
	if (cb) {
		cb->decref();
	}
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(shared_ptr<T, Allocator>&& other)
	: shared_ptr<T, Allocator>()
{
	operator=(std::move(other));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(std::nullptr_t)
	: shared_ptr<T, Allocator>()
{
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(const shared_ptr<T, Allocator>& other)
	: shared_ptr<T, Allocator>()
{
	operator=(other);
}
template <class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(T * object)
	: shared_ptr<T, Allocator>()
{
	Allocator alloc;
	this->myControlBlockStorage = create_control_block(object, alloc);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
// The Deleter callable has signature void(T* obj, Allocator& alloc)
template<class T, class Allocator>
template<class Deleter>
inline shared_ptr<T, Allocator>::shared_ptr(T* object, Deleter && deleter, Allocator & allocator)
	: shared_ptr<T, Allocator>()
{
	this->myControlBlockStorage = create_control_block(object, std::forward<Deleter&&>(deleter), allocator);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(compressed_storage from)
	: aspdetail::ptr_base<T, Allocator>(from)
{
}

template <class T, class Allocator>
template<class Deleter>
inline typename aspdetail::compressed_storage shared_ptr<T, Allocator>::create_control_block(T* object, Deleter&& deleter, Allocator& allocator)
{
	aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>* controlBlock(nullptr);

	const std::size_t blockSize(alloc_size_claim_custom_delete<Deleter>());

	void* block(nullptr);

	try {
		block = allocator.allocate(blockSize);
		controlBlock = new (block) aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>(object, allocator, std::forward<Deleter&&>(deleter));
	}
	catch (...) {
		allocator.deallocate(static_cast<uint8_t*>(block), blockSize);
		deleter(object, allocator);
		throw;
	}

	return compressed_storage(reinterpret_cast<uint64_t>(controlBlock));
}
template<class T, class Allocator>
inline typename aspdetail::compressed_storage shared_ptr<T, Allocator>::create_control_block(T * object, Allocator& allocator)
{
	aspdetail::control_block_claim<T, Allocator>* controlBlock(nullptr);

	const std::size_t blockSize(alloc_size_claim());

	void* block(nullptr);

	try {
		block = allocator.allocate(blockSize);
		controlBlock = new (block) aspdetail::control_block_claim<T, Allocator>(object, allocator);
	}
	catch (...) {
		allocator.deallocate(static_cast<uint8_t*>(block), blockSize);
		object->~T();
		allocator.deallocate(reinterpret_cast<uint8_t*>(object), sizeof(T));
		throw;
	}

	return compressed_storage(reinterpret_cast<uint64_t>(controlBlock));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>& shared_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& other)
{
	const compressed_storage otherValue(other.myControlBlockStorage);
	aspdetail::control_block_base<T, Allocator>* const otherCb(this->to_control_block(otherValue));
	if (otherCb) {
		otherCb->incref();
	}
	aspdetail::control_block_base<T, Allocator>* const cb(this->get_control_block());
	if (cb) {
		cb->decref();
	}

	this->myControlBlockStorage = otherValue;
	this->myPtr = other.myPtr;

	return *this;
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>& shared_ptr<T, Allocator>::operator=(shared_ptr<T, Allocator>&& other)
{
	std::swap(this->myControlBlockStorage, other.myControlBlockStorage);
	std::swap(this->myPtr, other.myPtr);
	return *this;
}
template<class T, class Allocator>
inline constexpr std::size_t shared_ptr<T, Allocator>::alloc_size_make_shared()
{
	return sizeof(aspdetail::control_block_make_shared<T, Allocator>);
}
template<class T, class Allocator>
inline constexpr std::size_t shared_ptr<T, Allocator>::alloc_size_claim()
{
	return sizeof(aspdetail::control_block_claim<T, Allocator>);
}
template<class T, class Allocator>
template<class Deleter>
inline constexpr std::size_t shared_ptr<T, Allocator>::alloc_size_claim_custom_delete()
{
	return sizeof(aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>);
}
// versioned_raw_ptr does not share in ownership of the object
template <class T, class Allocator>
class versioned_raw_ptr : public aspdetail::ptr_base<T, Allocator>
{
public:
	constexpr versioned_raw_ptr();
	constexpr versioned_raw_ptr(std::nullptr_t);

	constexpr versioned_raw_ptr(versioned_raw_ptr<T, Allocator>&& other);
	constexpr versioned_raw_ptr(const versioned_raw_ptr<T, Allocator>& other);

	explicit constexpr versioned_raw_ptr(const shared_ptr<T, Allocator>& from);

	explicit versioned_raw_ptr(const atomic_shared_ptr<T, Allocator>& from);

	constexpr versioned_raw_ptr<T, Allocator>& operator=(const versioned_raw_ptr<T, Allocator>& other);
	constexpr versioned_raw_ptr<T, Allocator>& operator=(versioned_raw_ptr<T, Allocator>&& other);
	constexpr versioned_raw_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& from);
	constexpr versioned_raw_ptr<T, Allocator>& operator=(const atomic_shared_ptr<T, Allocator>& from);

private:
	typedef aspdetail::compressed_storage compressed_storage;

	explicit constexpr versioned_raw_ptr(compressed_storage from);

	friend class aspdetail::ptr_base<T, Allocator>;
	friend class atomic_shared_ptr<T, Allocator>;
};
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr()
	: aspdetail::ptr_base<T, Allocator>()
{
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(std::nullptr_t)
	: versioned_raw_ptr<T, Allocator>()
{
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(versioned_raw_ptr<T, Allocator>&& other)
	: versioned_raw_ptr()
{
	operator=(std::move(other));
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(const versioned_raw_ptr<T, Allocator>& other)
	: versioned_raw_ptr()
{
	operator=(other);
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(const shared_ptr<T, Allocator>& from)
	: aspdetail::ptr_base<T, Allocator>(from.myControlBlockStorage)
{
}
template<class T, class Allocator>
inline versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(const atomic_shared_ptr<T, Allocator>& from)
	: aspdetail::ptr_base<T, Allocator>(from.myStorage.load())
{
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>& versioned_raw_ptr<T, Allocator>::operator=(const versioned_raw_ptr<T, Allocator>& other) {
	this->myControlBlockStorage = other.myControlBlockStorage;
	this->myPtr = other.myPtr;

	return *this;
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>& versioned_raw_ptr<T, Allocator>::operator=(versioned_raw_ptr<T, Allocator>&& other) {
	std::swap(this->myControlBlockStorage, other.myControlBlockStorage);
	std::swap(this->myPtr, other.myPtr);
	return *this;
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>& versioned_raw_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& from)
{
	*this = versioned_raw_ptr<T, Allocator>(from);
	return *this;
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>& versioned_raw_ptr<T, Allocator>::operator=(const atomic_shared_ptr<T, Allocator>& from)
{
	*this = versioned_raw_ptr<T, Allocator>(from);
	return *this;
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(compressed_storage from)
	: aspdetail::ptr_base<T, Allocator>(from)
{
}
template<class T, class ...Args>
inline shared_ptr<T, aspdetail::default_allocator> make_shared(Args&& ...args)
{
	aspdetail::default_allocator alloc;
	return make_shared<T>(alloc, std::forward<Args&&>(args)...);
}
template<class T, class Allocator, class ...Args>
inline shared_ptr<T, Allocator> make_shared(Args&& ...args)
{
	Allocator alloc;
	return make_shared<T, Allocator>(alloc, std::forward<Args&&>(args)...);
}
template<class T, class Allocator, class ...Args>
inline shared_ptr<T, Allocator> make_shared(Allocator& allocator, Args&& ...args)
{
	uint8_t* const block(allocator.allocate(shared_ptr<T, Allocator>::alloc_size_make_shared()));

	aspdetail::control_block_make_shared<T, Allocator>* controlBlock(nullptr);
	try {
		controlBlock = new (reinterpret_cast<uint8_t*>(block)) aspdetail::control_block_make_shared<T, Allocator>(allocator, std::forward<Args&&>(args)...);
	}
	catch (...) {
		if (controlBlock) {
			(*controlBlock).~control_block_make_shared<T, Allocator>();
		}
		allocator.deallocate(block, shared_ptr<T, Allocator>::alloc_size_make_shared());
		throw;
	}
	return shared_ptr<T, Allocator>(aspdetail::compressed_storage(reinterpret_cast<uint64_t>(controlBlock)));
}
};
#pragma warning(pop)