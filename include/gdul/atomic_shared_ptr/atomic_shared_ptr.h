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
#include <cstdint>
#include <limits>
#include <cassert>

#undef max

#pragma warning(push, 2)
// Anonymous union
#pragma warning(disable : 4201)
// Non-class enums 
#pragma warning(disable : 26812)

namespace gdul {
namespace aspdetail {

typedef std::allocator<std::uint8_t> default_allocator;

static constexpr std::uint64_t Ptr_Mask = (std::numeric_limits<std::uint64_t>::max() >> 16);
static constexpr std::uint64_t Versioned_Ptr_Mask = (std::numeric_limits<std::uint64_t>::max() >> 8);

union compressed_storage
{
	compressed_storage()  noexcept : myU64(0ULL) {}
	compressed_storage(std::uint64_t from) noexcept : myU64(from) {}

	std::uint64_t myU64;
	std::uint32_t myU32[2];
	std::uint16_t myU16[4];
	std::uint8_t myU8[8];
};

template <class Dummy = void>
void assert_alignment(std::uint8_t* block, std::size_t alignment);

template <class T>
struct disable_deduction;

template <class T>
class control_block_base_interface;
template <class T, class Allocator>
class control_block_base_members;
template <class T, class Allocator>
class control_block_make_shared;
template <class T, class Allocator>
class control_block_claim;
template <class T, class Allocator, class Deleter>
class control_block_claim_custom_delete;

template <class T>
class ptr_base;

struct memory_orders{std::memory_order myFirst, mySecond;};

using size_type = std::uint32_t;

enum STORAGE_BYTE : std::uint8_t;
enum CAS_FLAG : std::uint8_t;

static constexpr std::uint8_t Local_Ref_Index(7);
static constexpr std::uint64_t Local_Ref_Step(1ULL << (Local_Ref_Index * 8));
static constexpr std::uint8_t Local_Ref_Fill_Boundary(112);

}
template <class T>
class shared_ptr;

template <class T>
class raw_ptr;

template <class T, class ...Args>
inline shared_ptr<T> make_shared(Args&&...);
template <class T, class ...Args>
inline shared_ptr<T> make_shared(Args&&...);
template <class T, class Allocator, class ...Args>
inline shared_ptr<T> make_shared(Allocator&, Args&&...);

template <class T>
class atomic_shared_ptr
{
public:
	using size_type = aspdetail::size_type;

	inline constexpr atomic_shared_ptr() noexcept;
	inline constexpr atomic_shared_ptr(std::nullptr_t) noexcept;
	inline constexpr atomic_shared_ptr(std::nullptr_t, std::uint8_t version) noexcept;

	inline atomic_shared_ptr(const shared_ptr<T>& from) noexcept;
	inline atomic_shared_ptr(shared_ptr<T>&& from) noexcept;

	inline ~atomic_shared_ptr() noexcept;

	inline atomic_shared_ptr<T>& operator=(const shared_ptr<T>& from) noexcept;
	inline atomic_shared_ptr<T>& operator=(shared_ptr<T>&& from) noexcept;

	inline bool compare_exchange_strong(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline bool compare_exchange_strong(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline bool compare_exchange_strong(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	inline bool compare_exchange_strong(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline bool compare_exchange_strong(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline bool compare_exchange_strong(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline bool compare_exchange_strong(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	inline bool compare_exchange_strong(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline void store(const shared_ptr<T>& from, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline void store(shared_ptr<T>&& from, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline shared_ptr<T> exchange(const shared_ptr<T>& with, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline shared_ptr<T> exchange(shared_ptr<T>&& with, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline std::uint8_t get_version() const noexcept;

	inline shared_ptr<T> unsafe_load(std::memory_order order = std::memory_order_seq_cst);

	inline shared_ptr<T> unsafe_exchange(const shared_ptr<T>& with, std::memory_order order = std::memory_order_seq_cst);
	inline shared_ptr<T> unsafe_exchange(shared_ptr<T>&& with, std::memory_order order = std::memory_order_seq_cst);

	inline void unsafe_store(const shared_ptr<T>& from, std::memory_order order = std::memory_order_seq_cst);
	inline void unsafe_store(shared_ptr<T>&& from, std::memory_order order = std::memory_order_seq_cst);

	// cheap hint to see if this object holds a value
	operator bool() const noexcept;

	// cheap hint comparison to ptr_base derivatives
	bool operator==(const aspdetail::ptr_base<T>& other) const noexcept;

	// cheap hint comparison to ptr_base derivatives
	bool operator!=(const aspdetail::ptr_base<T>& other) const noexcept;

	raw_ptr<T> unsafe_get_raw_ptr() const;

	T* unsafe_get_owned();
	const T* unsafe_get_owned() const;

private:
	template <class PtrType>
	inline bool compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	template <class PtrType>
	inline bool compare_exchange_weak(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline constexpr aspdetail::control_block_base_interface<T>* get_control_block() noexcept;
	inline constexpr const aspdetail::control_block_base_interface<T>* get_control_block() const noexcept;

	using compressed_storage = aspdetail::compressed_storage;
	using size_type = aspdetail::size_type;

	inline compressed_storage unsafe_copy_internal(std::memory_order order);
	inline compressed_storage copy_internal(std::memory_order order) noexcept;

	inline void unsafe_store_internal(compressed_storage from, std::memory_order order);
	inline void store_internal(compressed_storage from, std::memory_order order) noexcept;

	inline compressed_storage unsafe_exchange_internal(compressed_storage with, std::memory_order order);
	inline compressed_storage exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags, std::memory_order order) noexcept;

	inline bool compare_exchange_weak_internal(compressed_storage& expected, compressed_storage desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept;

	inline void unsafe_fill_local_ref();
	inline void try_fill_local_ref(compressed_storage& expected) noexcept;

	inline constexpr aspdetail::control_block_base_interface<T>* to_control_block(compressed_storage from) const noexcept;

	friend class shared_ptr<T>;
	friend class raw_ptr<T>;
	friend class aspdetail::ptr_base<T>;

	union
	{
		std::atomic<std::uint64_t> myStorage;
		const compressed_storage myDebugView;
	};
};
template <class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr() noexcept
	: myStorage(0ULL)
{
}
template<class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr(std::nullptr_t) noexcept
	: atomic_shared_ptr<T>()
{
}
template<class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr(std::nullptr_t, std::uint8_t version) noexcept
	: myStorage(0ULL | (std::uint64_t(version) << (aspdetail::STORAGE_BYTE_VERSION * 8)))
{
}
template <class T>
inline atomic_shared_ptr<T>::atomic_shared_ptr(shared_ptr<T>&& other) noexcept
	: atomic_shared_ptr<T>()
{
	unsafe_store(std::move(other));
}
template<class T>
inline atomic_shared_ptr<T>::atomic_shared_ptr(const shared_ptr<T>& from) noexcept
	: atomic_shared_ptr<T>()
{
	unsafe_store(from);
}
template <class T>
inline atomic_shared_ptr<T>::~atomic_shared_ptr() noexcept
{
	unsafe_store_internal(compressed_storage(), std::memory_order_relaxed);
}

template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order) noexcept
{
	return compare_exchange_strong(expected, shared_ptr<T>(desired), order);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order) noexcept
{
	return compare_exchange_strong(expected, std::move(desired), order, order);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_strong(expected, shared_ptr<T>(desired), successOrder, failOrder);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_strong<decltype(expected)>(expected, std::move(desired), successOrder, failOrder);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order) noexcept
{
	return compare_exchange_strong(expected, shared_ptr<T>(desired), order);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order) noexcept
{
	return compare_exchange_strong(expected, std::move(desired), order, order);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_strong(expected, shared_ptr<T>(desired), successOrder, failOrder);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_strong<decltype(expected)>(expected, std::move(desired), successOrder, failOrder);
}

template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order) noexcept
{
	return compare_exchange_weak(expected, shared_ptr<T>(desired), order);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order) noexcept
{
	return compare_exchange_weak(expected, std::move(desired), order, order);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_weak(expected, shared_ptr<T>(desired), successOrder, failOrder);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_weak<decltype(expected)>(expected, std::move(desired), successOrder, failOrder);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order) noexcept
{
	return compare_exchange_weak(expected, shared_ptr<T>(desired), order);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order) noexcept
{
	return compare_exchange_weak(expected, std::move(desired), order, order);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_weak(expected, shared_ptr<T>(desired), successOrder, failOrder);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_weak<decltype(expected)>(expected, std::move(desired), successOrder, failOrder);
}
template<class T>
template<class PtrType>
inline bool atomic_shared_ptr<T>::compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type & expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	desired.fill_local_refs();

	const compressed_storage desired_(desired.myControlBlockStorage.myU64);

	compressed_storage expected_(myStorage.load(std::memory_order_relaxed));
	expected_.myU64 &= ~aspdetail::Versioned_Ptr_Mask;
	expected_.myU64 |= expected.myControlBlockStorage.myU64 & aspdetail::Versioned_Ptr_Mask;

	const aspdetail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	const bool needsCapture(std::is_same<raw_type, shared_ptr<T>>::value);
	const std::uint8_t flags(aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE * static_cast<std::uint8_t>(needsCapture));

	const std::uint64_t preCompare(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);
	do {
		if (compare_exchange_weak_internal(expected_, desired_, static_cast<aspdetail::CAS_FLAG>(flags), orders)) {

			desired.reset();

			return true;
		}

	} while (preCompare == (expected_.myU64 & aspdetail::Versioned_Ptr_Mask));

	expected = raw_type(expected_);

	return false;
}
template<class T>
template<class PtrType>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(typename aspdetail::disable_deduction<PtrType>::type & expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	desired.fill_local_refs();

	const compressed_storage desired_(desired.myControlBlockStorage.myU64);
	compressed_storage expected_(myStorage.load(std::memory_order_relaxed));
	expected_.myU64 &= ~aspdetail::Versioned_Ptr_Mask;
	expected_.myU64 |= expected.myControlBlockStorage.myU64 & aspdetail::Versioned_Ptr_Mask;

	const aspdetail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	const bool needsCapture(std::is_same<raw_type, shared_ptr<T>>::value);
	const std::uint8_t flags(aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE * static_cast<std::uint8_t>(needsCapture));

	const std::uint64_t preCompare(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);

	if (compare_exchange_weak_internal(expected_, desired_, static_cast<aspdetail::CAS_FLAG>(flags), orders)) {

		desired.reset();

		return true;
	}

	const std::uint64_t postCompare(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);

	if (preCompare == postCompare) {
		std::atomic_thread_fence(orders.mySecond);
		return false;
	}

	expected = raw_type(expected_);

	return false;
}
template<class T>
inline atomic_shared_ptr<T>& atomic_shared_ptr<T>::operator=(const shared_ptr<T>& from) noexcept
{
	store(from);
	return *this;
}
template<class T>
inline atomic_shared_ptr<T>& atomic_shared_ptr<T>::operator=(shared_ptr<T>&& from) noexcept
{
	store(std::move(from));
	return *this;
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::load(std::memory_order order) noexcept
{
	compressed_storage copy(copy_internal(order));
	return shared_ptr<T>(copy);
}
template<class T>
inline void atomic_shared_ptr<T>::store(const shared_ptr<T>& from, std::memory_order order) noexcept
{
	store(shared_ptr<T>(from), order);
}
template<class T>
inline void atomic_shared_ptr<T>::store(shared_ptr<T>&& from, std::memory_order order) noexcept
{
	from.fill_local_refs();
	store_internal(from.myControlBlockStorage.myU64, order);
	from.reset();
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::exchange(const shared_ptr<T>& with, std::memory_order order) noexcept
{
	return exchange(shared_ptr<T>(with), order);
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::exchange(shared_ptr<T>&& with, std::memory_order order) noexcept
{
	with.fill_local_refs();
	compressed_storage previous(exchange_internal(with.myControlBlockStorage, aspdetail::CAS_FLAG_STEAL_PREVIOUS, order));
	with.reset();
	return shared_ptr<T>(previous);
}
template<class T>
inline std::uint8_t atomic_shared_ptr<T>::get_version() const noexcept
{
	const compressed_storage storage(myStorage.load(std::memory_order_relaxed));
	return storage.myU8[aspdetail::STORAGE_BYTE_VERSION];
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::unsafe_load(std::memory_order order)
{
	return shared_ptr<T>(unsafe_copy_internal(order));
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::unsafe_exchange(const shared_ptr<T>& with, std::memory_order order)
{
	return unsafe_exchange(shared_ptr<T>(with), order);
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::unsafe_exchange(shared_ptr<T>&& with, std::memory_order order)
{
	const compressed_storage previous(unsafe_exchange_internal(with.myControlBlockStorage, order));
	with.reset();
	return shared_ptr<T>(previous);
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_store(const shared_ptr<T>& from, std::memory_order order)
{
	unsafe_store(shared_ptr<T>(from), order);
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_store(shared_ptr<T>&& from, std::memory_order order)
{
	unsafe_store_internal(from.myControlBlockStorage, order);
	from.reset();
}
template<class T>
inline raw_ptr<T> atomic_shared_ptr<T>::unsafe_get_raw_ptr() const
{
	compressed_storage storage(myStorage.load(std::memory_order_acquire));
	return raw_ptr<T>(storage);
}
template<class T>
inline T * atomic_shared_ptr<T>::unsafe_get_owned()
{
	aspdetail::control_block_base_interface<T>* const cb(get_control_block());
	if (cb) {
		return cb->get_owned();
	}
	return nullptr;
}
template<class T>
inline const T * atomic_shared_ptr<T>::unsafe_get_owned() const
{
	aspdetail::control_block_base_interface<T>* const cb(get_control_block());
	if (cb)
	{
		return cb->get_owned();
	}
	return nullptr;
}
template<class T>
inline constexpr const aspdetail::control_block_base_interface<T>* atomic_shared_ptr<T>::get_control_block() const noexcept
{
	return to_control_block(myStorage.load(std::memory_order_acquire));
}
template<class T>
inline constexpr aspdetail::control_block_base_interface<T>* atomic_shared_ptr<T>::get_control_block() noexcept
{
	return to_control_block(myStorage.load(std::memory_order_acquire));
}
// cheap hint to see if this object holds a value
template<class T>
inline atomic_shared_ptr<T>::operator bool() const noexcept
{
	return static_cast<bool>(myStorage.load(std::memory_order_relaxed) & aspdetail::Ptr_Mask);
}
// cheap hint to see if this object holds a value
template <class T>
inline constexpr bool operator==(std::nullptr_t /*aNullptr*/, const atomic_shared_ptr<T>& ptr) noexcept
{
	return !ptr;
}
// cheap hint to see if this object holds a value
template <class T>
inline constexpr bool operator!=(std::nullptr_t /*aNullptr*/, const atomic_shared_ptr<T>& ptr) noexcept
{
	return ptr;
}
// cheap hint to see if this object holds a value
template <class T>
inline constexpr bool operator==(const atomic_shared_ptr<T>& ptr, std::nullptr_t /*aNullptr*/) noexcept
{
	return !ptr;
}
// cheap hint to see if this object holds a value
template <class T>
inline constexpr bool operator!=(const atomic_shared_ptr<T>& ptr, std::nullptr_t /*aNullptr*/) noexcept
{
	return ptr;
}
// cheap hint comparison to ptr_base derivatives
template<class T>
inline bool atomic_shared_ptr<T>::operator==(const aspdetail::ptr_base<T>& other) const noexcept
{
	return !((myStorage.load(std::memory_order_relaxed) ^ other.myControlBlockStorage.myU64) & aspdetail::Versioned_Ptr_Mask);
}
// cheap hint comparison to ptr_base derivatives
template<class T>
inline bool atomic_shared_ptr<T>::operator!=(const aspdetail::ptr_base<T>& other) const noexcept
{
	return !operator==(other);
}
// ------------------------------------------------------------------------------------

// -------------------------Internals--------------------------------------------------

// ------------------------------------------------------------------------------------
template <class T>
inline void atomic_shared_ptr<T>::store_internal(compressed_storage from, std::memory_order order) noexcept
{
	exchange_internal(from, aspdetail::CAS_FLAG_NONE, order);
}
template<class T>
inline constexpr aspdetail::control_block_base_interface<T>* atomic_shared_ptr<T>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<aspdetail::control_block_base_interface<T>*>(from.myU64 & aspdetail::Ptr_Mask);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak_internal(compressed_storage & expected, compressed_storage desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept
{
	bool result(false);

	compressed_storage desired_(desired);
	desired_.myU8[aspdetail::STORAGE_BYTE_VERSION] = expected.myU8[aspdetail::STORAGE_BYTE_VERSION] + 1;

	compressed_storage expected_(expected);

	result = myStorage.compare_exchange_weak(expected_.myU64, desired_.myU64, orders.myFirst, orders.mySecond);

	if (result & !(flags & aspdetail::CAS_FLAG_STEAL_PREVIOUS)) {
		if (aspdetail::control_block_base_interface<T>* const cb = to_control_block(expected)) {
			cb->decref(expected.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
		}
	}

	const bool otherInterjection((expected_.myU64 ^ expected.myU64) & aspdetail::Versioned_Ptr_Mask);

	expected = expected_;

	if (otherInterjection & (flags & aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE)) {
		expected = copy_internal(std::memory_order_relaxed);
	}
	
	return result;
}
template<class T>
inline void atomic_shared_ptr<T>::try_fill_local_ref(compressed_storage& expected) noexcept
{
	const compressed_storage initialPtrBlock(expected.myU64 & aspdetail::Versioned_Ptr_Mask);

	aspdetail::control_block_base_interface<T>* const cb(to_control_block(expected));

	if (!cb) {
		return;
	}

	do {
		const std::uint8_t localRefs(expected.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
		const std::uint8_t desiredRefs(std::numeric_limits<std::uint8_t>::max() - localRefs);

		compressed_storage desired(expected);
		desired.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = desiredRefs;

		cb->incref(desiredRefs);

		if (myStorage.compare_exchange_weak(expected.myU64, desired.myU64, std::memory_order_relaxed)) {
			expected.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();
			return;
		}

		cb->decref(desiredRefs);

	} while (
		(expected.myU64 & aspdetail::Versioned_Ptr_Mask) == initialPtrBlock.myU64 &&
		expected.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary);
}
template <class T>
inline typename  aspdetail::compressed_storage atomic_shared_ptr<T>::copy_internal(std::memory_order order) noexcept
{
	compressed_storage initial(myStorage.fetch_sub(aspdetail::Local_Ref_Step, std::memory_order_relaxed));
	initial.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] -= 1;

	if (to_control_block(initial) && initial.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary) {
		compressed_storage expected(initial);
		try_fill_local_ref(expected);
	}

	initial.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 1;

	return initial;
}
template <class T>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T>::unsafe_copy_internal(std::memory_order order)
{
	compressed_storage storage(myStorage.load(std::memory_order_relaxed));

	compressed_storage newStorage(storage);
	newStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] -= 1;
	myStorage.store(newStorage.myU64, std::memory_order_relaxed);

	storage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 1;

	unsafe_fill_local_ref();

	std::atomic_thread_fence(order);

	return storage;
}
template<class T>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T>::unsafe_exchange_internal(compressed_storage with, std::memory_order order)
{
	const compressed_storage old(myStorage.load(std::memory_order_relaxed));

	compressed_storage replacement(with.myU64);
	replacement.myU8[aspdetail::STORAGE_BYTE_VERSION] = old.myU8[aspdetail::STORAGE_BYTE_VERSION] + 1;

	myStorage.store(replacement.myU64, std::memory_order_relaxed);

	unsafe_fill_local_ref();

	std::atomic_thread_fence(order);

	return old;
}
template <class T>
inline void atomic_shared_ptr<T>::unsafe_store_internal(compressed_storage from, std::memory_order order)
{
	const compressed_storage previous(myStorage.load(std::memory_order_relaxed));
	compressed_storage next(from);
	next.myU8[aspdetail::STORAGE_BYTE_VERSION] = previous.myU8[aspdetail::STORAGE_BYTE_VERSION] + 1;

	myStorage.store(next.myU64, std::memory_order_relaxed);

	aspdetail::control_block_base_interface<T>* const prevCb(to_control_block(previous));
	if (prevCb) {
		prevCb->decref(previous.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
	}

	unsafe_fill_local_ref();
	
	std::atomic_thread_fence(order);
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_fill_local_ref()
{
	const compressed_storage current(myStorage.load(std::memory_order_relaxed));
	aspdetail::control_block_base_interface<T>* const cb(to_control_block(current));

	if (cb && current.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary) {
		const std::uint8_t desiredRefs(std::numeric_limits<std::uint8_t>::max() - current.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);

		cb->incref(desiredRefs);

		compressed_storage newStorage(current);
		newStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();

		myStorage.store(newStorage.myU64, std::memory_order_relaxed);
	}
}
template<class T>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T>::exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags, std::memory_order order) noexcept
{
	compressed_storage expected(myStorage.load(std::memory_order_relaxed));
	while (!compare_exchange_weak_internal(expected, to, flags, { order, std::memory_order_relaxed }));
	return expected;
}
namespace aspdetail {
template <class T>
class control_block_base_interface
{
public:
	using size_type = aspdetail::size_type;

	virtual T* get_owned() noexcept = 0;
	virtual const T* get_owned() const noexcept = 0;

	virtual size_type use_count() const noexcept = 0;

	virtual void incref(size_type count) noexcept = 0;
	virtual void decref(size_type count) noexcept = 0;

protected:
	virtual ~control_block_base_interface() = default;
	virtual void destroy() = 0;
};
template <class T, class Allocator>
class control_block_base_members : public control_block_base_interface<T>
{
public:
	using size_type = aspdetail::size_type;

	control_block_base_members(T* object, Allocator& allocator, std::uint8_t blockOffset = 0);

	T* get_owned() noexcept;
	const T* get_owned() const noexcept;

	size_type use_count() const noexcept;

	void incref(size_type count) noexcept;
	void decref(size_type count) noexcept;

protected:
	virtual ~control_block_base_members() = default;
	virtual void destroy() = 0;

	T* const myPtr;
	std::atomic<size_type> myUseCount;
	Allocator myAllocator;
	const std::uint8_t myBlockOffset;
};

template<class T, class Allocator>
inline control_block_base_members<T, Allocator>::control_block_base_members(T* object, Allocator& allocator, std::uint8_t blockOffset)
	: myUseCount(std::numeric_limits<std::uint8_t>::max())
	, myPtr(object)
	, myAllocator(allocator)
	, myBlockOffset(blockOffset)
{
}
template<class T, class Allocator>
inline void control_block_base_members<T, Allocator>::incref(size_type count) noexcept
{
	myUseCount.fetch_add(count, std::memory_order_relaxed);
}
template<class T, class Allocator>
inline void control_block_base_members<T, Allocator>::decref(size_type count) noexcept
{
	if (!(myUseCount.fetch_sub(count, std::memory_order_acq_rel) - count))
	{
		destroy();
	}
}
template <class T, class Allocator>
inline T* control_block_base_members<T, Allocator>::get_owned() noexcept
{
	return myPtr;
}
template<class T, class Allocator>
inline const T* control_block_base_members<T, Allocator>::get_owned() const noexcept
{
	return myPtr;
}
template <class T, class Allocator>
inline typename control_block_base_members<T, Allocator>::size_type control_block_base_members<T, Allocator>::use_count() const noexcept
{
	return myUseCount.load(std::memory_order_relaxed);
}
template <class T, class Allocator>
class control_block_make_shared : public control_block_base_members<T, Allocator>
{
public:
	template <class ...Args, class U = T, std::enable_if_t<std::is_array<U>::value>* = nullptr>
	control_block_make_shared(Allocator& alloc, std::uint8_t blockOffset, Args&& ...args);
	template <class ...Args, class U = T, std::enable_if_t<!std::is_array<U>::value>* = nullptr>
	control_block_make_shared(Allocator& alloc, std::uint8_t blockOffset, Args&& ...args);

	void destroy() noexcept override;
private:
	T myOwned;
};
template<class T, class Allocator>
template <class ...Args, class U, std::enable_if_t<std::is_array<U>::value>*>
inline control_block_make_shared<T, Allocator>::control_block_make_shared(Allocator& alloc, std::uint8_t blockOffset, Args&& ...args)
	: control_block_base_members<T, Allocator>(&myOwned, alloc, blockOffset)
	, myOwned{ std::forward<Args&&>(args)... }
{
}
template<class T, class Allocator>
template <class ...Args, class U, std::enable_if_t<!std::is_array<U>::value>*>
inline control_block_make_shared<T, Allocator>::control_block_make_shared(Allocator& alloc, std::uint8_t blockOffset, Args&& ...args)
	: control_block_base_members<T, Allocator>(&myOwned, alloc, blockOffset)
	, myOwned(std::forward<Args&&>(args)...)
{
}
template<class T, class Allocator>
inline void control_block_make_shared<T, Allocator>::destroy() noexcept
{
	Allocator alloc(this->myAllocator);
	uint8_t* beginPtr(reinterpret_cast<std::uint8_t*>(this) - this->myBlockOffset);

	(*this).~control_block_make_shared<T, Allocator>();

	alloc.deallocate(beginPtr, shared_ptr<T>::template alloc_size_make_shared<Allocator>());
}
template <class T, class Allocator>
class control_block_claim : public control_block_base_members<T, Allocator>
{
public:
	control_block_claim(T* obj, Allocator& alloc) noexcept;
	void destroy() noexcept override;
};
template<class T, class Allocator>
inline control_block_claim<T, Allocator>::control_block_claim(T * obj, Allocator & alloc) noexcept
	:control_block_base_members<T, Allocator>(obj, alloc)
{
}
template<class T, class Allocator>
inline void control_block_claim<T, Allocator>::destroy() noexcept
{
	Allocator alloc(this->myAllocator);

	T* const ptrAddr(this->myPtr);
	ptrAddr->~T();
	(*this).~control_block_claim<T, Allocator>();

	alloc.deallocate(reinterpret_cast<std::uint8_t*>(this), sizeof(decltype(*this)));
	alloc.deallocate(reinterpret_cast<std::uint8_t*>(ptrAddr), sizeof(T));
}
template <class T, class Allocator, class Deleter>
class control_block_claim_custom_delete : public control_block_base_members<T, Allocator>
{
public:
	control_block_claim_custom_delete(T* obj, Allocator& alloc, Deleter&& deleter) noexcept;
	void destroy() noexcept override;

private:
	Deleter myDeleter;
};
template<class T, class Allocator, class Deleter>
inline control_block_claim_custom_delete<T, Allocator, Deleter>::control_block_claim_custom_delete(T* obj, Allocator& alloc, Deleter && deleter) noexcept
	: control_block_base_members<T, Allocator>(obj, alloc)
	, myDeleter(std::forward<Deleter&&>(deleter))
{
}
template<class T, class Allocator, class Deleter>
inline void control_block_claim_custom_delete<T, Allocator, Deleter>::destroy() noexcept
{
	Allocator alloc(this->myAllocator);

	T* const ptrAddr(this->myPtr);
	myDeleter(ptrAddr, alloc);
	(*this).~control_block_claim_custom_delete<T, Allocator, Deleter>();

	alloc.deallocate(reinterpret_cast<std::uint8_t*>(this), sizeof(decltype(*this)));
}
template <class T>
struct disable_deduction
{
	using type = T;
};
enum STORAGE_BYTE : std::uint8_t
{
	STORAGE_BYTE_VERSION = 6,
	STORAGE_BYTE_LOCAL_REF = Local_Ref_Index,
};
enum CAS_FLAG : std::uint8_t
{
	CAS_FLAG_NONE = 0,
	CAS_FLAG_CAPTURE_ON_FAILURE,
	CAS_FLAG_STEAL_PREVIOUS,
};
template<class Dummy>
void assert_alignment(std::uint8_t* block, std::size_t alignment)
{
#ifndef _HAS_CXX17
	static_assert(!(std::numeric_limits<std::uint8_t>::max() < align), "make_shared supports only supports up to std::numeric_limits<std::uint8_t>::max() byte aligned types");

	if ((std::uintptr_t)block % alignof(std::max_align_t) != 0) {
		throw std::exception("make_shared expects at least alignof(max_align_t) allocates");
	}
#else
	if ((std::uintptr_t)block % alignment != 0) {
		throw std::exception("conforming with C++17 make_shared expects alignof(T) allocates");
	}
#endif
}
template <class T>
class ptr_base
{
public:
	using size_type = aspdetail::size_type;
	using value_type = T;

	inline constexpr ptr_base(std::nullptr_t) noexcept;
	inline constexpr ptr_base(std::nullptr_t, std::uint8_t version) noexcept;

	inline constexpr operator bool() const noexcept;

	inline constexpr bool operator==(const ptr_base<T>& other) const noexcept;
	inline constexpr bool operator!=(const ptr_base<T>& other) const noexcept;

	inline bool operator==(const atomic_shared_ptr<T>& other) const noexcept;
	inline bool operator!=(const atomic_shared_ptr<T>& other) const noexcept;

	inline constexpr explicit operator T*() noexcept;
	inline constexpr explicit operator const T*() const noexcept;

	inline constexpr const T* get_owned() const noexcept;
	inline constexpr T* get_owned() noexcept;

	inline constexpr raw_ptr<T> get_raw_ptr() const noexcept;

	inline constexpr std::uint8_t get_version() const noexcept;

	inline size_type use_count() const noexcept;

	inline constexpr T* operator->();
	inline constexpr T& operator*();

	inline constexpr const T* operator->() const;
	inline constexpr const T& operator*() const;

	inline const T& operator[](size_type index) const;
	inline T& operator[](size_type index);

protected:
	inline constexpr const control_block_base_interface<T>* get_control_block() const noexcept;
	inline constexpr control_block_base_interface<T>* get_control_block() noexcept;

	inline constexpr ptr_base() noexcept;
	inline constexpr ptr_base(compressed_storage from) noexcept;

	inline void reset() noexcept;

	constexpr control_block_base_interface<T>* to_control_block(compressed_storage from) noexcept;
	constexpr T* to_object(compressed_storage from) noexcept;

	constexpr const control_block_base_interface<T>* to_control_block(compressed_storage from) const noexcept;
	constexpr const T* to_object(compressed_storage from) const noexcept;

	friend class atomic_shared_ptr<T>;

	// mutable for altering local refs
	mutable compressed_storage myControlBlockStorage;
	T* myPtr;
};
template <class T>
inline constexpr ptr_base<T>::ptr_base() noexcept
	: myPtr(nullptr)
{
}
template<class T>
inline constexpr ptr_base<T>::ptr_base(std::nullptr_t) noexcept
	: ptr_base<T>()
{
}
template<class T>
inline constexpr ptr_base<T>::ptr_base(std::nullptr_t, std::uint8_t version) noexcept
	: myPtr(nullptr)
	, myControlBlockStorage(0ULL | (std::uint64_t(version) << (STORAGE_BYTE_VERSION * 8)))
{
}
template <class T>
inline constexpr ptr_base<T>::ptr_base(compressed_storage from) noexcept
	: ptr_base<T>()
{
	myControlBlockStorage = from;
	myPtr = to_object(from);
}
template<class T>
inline void ptr_base<T>::reset() noexcept
{
	myControlBlockStorage = compressed_storage();
	myPtr = nullptr;
}
template <class T>
inline constexpr ptr_base<T>::operator bool() const noexcept
{
	return myControlBlockStorage.myU64 & aspdetail::Ptr_Mask;
}
template <class T>
inline constexpr bool ptr_base<T>::operator==(const ptr_base<T>& other) const noexcept
{
	return !((myControlBlockStorage.myU64 ^ other.myControlBlockStorage.myU64) & aspdetail::Versioned_Ptr_Mask);
}
template <class T>
inline constexpr bool ptr_base<T>::operator!=(const ptr_base<T>& other) const noexcept
{
	return !operator==(other);
}
template<class T>
inline bool ptr_base<T>::operator==(const atomic_shared_ptr<T>& other) const noexcept
{
	return other == *this;
}
template<class T>
inline bool ptr_base<T>::operator!=(const atomic_shared_ptr<T>& other) const noexcept
{
	return !operator==(other);
}
template <class T>
inline typename ptr_base<T>::size_type ptr_base<T>::use_count() const noexcept
{
	if (!operator bool()) {
		return 0;
	}

	return get_control_block()->use_count();
}
template <class T>
inline constexpr ptr_base<T>::operator T*() noexcept
{
	return get_owned();
}
template <class T>
inline constexpr ptr_base<T>::operator const T*() const noexcept
{
	return get_owned();
}
template <class T>
inline constexpr T* ptr_base<T>::operator->()
{
	return get_owned();
}
template <class T>
inline constexpr T & ptr_base<T>::operator*()
{
	return *get_owned();
}
template <class T>
inline constexpr const T* ptr_base<T>::operator->() const
{
	return get_owned();
}
template <class T>
inline constexpr const T & ptr_base<T>::operator*() const
{
	return *get_owned();
}
template <class T>
inline const T & ptr_base<T>::operator[](size_type index) const
{
	return get_owned()[index];
}
template <class T>
inline T & ptr_base<T>::operator[](size_type index)
{
	return get_owned()[index];
}
template <class T>
inline constexpr bool operator==(std::nullptr_t /*aNullptr*/, const ptr_base<T>& ptr) noexcept
{
	return !ptr;
}
template <class T>
inline constexpr bool operator!=(std::nullptr_t /*aNullptr*/, const ptr_base<T>& ptr) noexcept
{
	return ptr;
}
template <class T>
inline constexpr bool operator==(const ptr_base<T>& ptr, std::nullptr_t /*aNullptr*/) noexcept
{
	return !ptr;
}
template <class T>
inline constexpr bool operator!=(const ptr_base<T>& ptr, std::nullptr_t /*aNullptr*/) noexcept
{
	return ptr;
}
template <class T>
inline constexpr control_block_base_interface<T>* ptr_base<T>::to_control_block(compressed_storage from) noexcept
{
	return reinterpret_cast<control_block_base_interface<T>*>(from.myU64 & Ptr_Mask);
}
template <class T>
inline constexpr T* ptr_base<T>::to_object(compressed_storage from) noexcept
{
	control_block_base_interface<T>* const cb(to_control_block(from));
	if (cb) {
		return cb->get_owned();
	}
	return nullptr;
}
template <class T>
inline constexpr const control_block_base_interface<T>* ptr_base<T>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<const control_block_base_interface<T>*>(from.myU64 & Ptr_Mask);
}
template <class T>
inline constexpr const T* ptr_base<T>::to_object(compressed_storage from) const noexcept
{
	control_block_base_interface<T>* const cb(to_control_block(from));
	if (cb)
	{
		return cb->get_owned();
	}
	return nullptr;
}
template <class T>
inline constexpr const control_block_base_interface<T>* ptr_base<T>::get_control_block() const noexcept
{
	return to_control_block(myControlBlockStorage);
}
template <class T>
inline constexpr const T* ptr_base<T>::get_owned() const noexcept
{
	return myPtr;
}
template <class T>
inline constexpr control_block_base_interface<T>* ptr_base<T>::get_control_block() noexcept
{
	return to_control_block(myControlBlockStorage);
}
template <class T>
inline constexpr T* ptr_base<T>::get_owned() noexcept
{
	return myPtr;
}
template<class T>
inline constexpr std::uint8_t ptr_base<T>::get_version() const noexcept
{
	return myControlBlockStorage.myU8[STORAGE_BYTE_VERSION];
}
template<class T>
inline constexpr raw_ptr<T> ptr_base<T>::get_raw_ptr() const noexcept
{
	return raw_ptr<T>(myControlBlockStorage);
}
};
template <class T>
class shared_ptr : public aspdetail::ptr_base<T>
{
public:
	inline constexpr shared_ptr() = default;

	using aspdetail::ptr_base<T>::ptr_base;

	inline shared_ptr(const shared_ptr<T>& other) noexcept;
	inline shared_ptr(shared_ptr<T>&& other) noexcept;

	inline explicit shared_ptr(T* object);
	template <class Allocator>
	inline explicit shared_ptr(T* object, Allocator& allocator);
	template <class Deleter>
	inline explicit shared_ptr(T* object, Deleter&& deleter);
	template <class Deleter, class Allocator>
	inline explicit shared_ptr(T* object, Deleter&& deleter, Allocator& allocator);

	~shared_ptr() noexcept;

	shared_ptr<T>& operator=(const shared_ptr<T>& other) noexcept;
	shared_ptr<T>& operator=(shared_ptr<T>&& other) noexcept;

	// The amount of memory requested from the allocator when calling
	// make_shared
	

	template <class Allocator>
	static constexpr std::size_t alloc_size_make_shared() noexcept;

	// The amount of memory requested from the allocator when taking 
	// ownership of an object using default deleter
	template <class Allocator>
	static constexpr std::size_t alloc_size_claim() noexcept;

	// The amount of memory requested from the allocator when taking 
	// ownership of an object using a custom deleter
	template <class Allocator, class Deleter>
	static constexpr std::size_t alloc_size_claim_custom_delete() noexcept;

private:
	inline void fill_local_refs() noexcept;

	using compressed_storage = aspdetail::compressed_storage;

	shared_ptr(compressed_storage from) noexcept;

	template<class Deleter, class Allocator>
	inline compressed_storage create_control_block(T* object, Deleter&& deleter, Allocator& allocator);
	template <class Allocator>
	inline compressed_storage create_control_block(T* object, Allocator& allocator);

	friend class raw_ptr<T>;
	friend class atomic_shared_ptr<T>;

	template <class T, class Allocator, class ...Args>
	friend shared_ptr<T> make_shared<T, Allocator>(Allocator&, Args&&...);
};
template<class T>
inline shared_ptr<T>::~shared_ptr()
{
	aspdetail::control_block_base_interface<T>* const cb(this->get_control_block());
	if (cb) {
		cb->decref(this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
	}
}
template<class T>
inline shared_ptr<T>::shared_ptr(shared_ptr<T>&& other) noexcept
	: shared_ptr<T>()
{
	operator=(std::move(other));
}
template<class T>
inline shared_ptr<T>::shared_ptr(const shared_ptr<T>& other) noexcept
	: shared_ptr<T>()
{
	operator=(other);
}
template <class T>
inline shared_ptr<T>::shared_ptr(T * object)
	: shared_ptr<T>()
{
	aspdetail::default_allocator alloc;
	this->myControlBlockStorage = create_control_block(object, alloc);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
template <class T>
template <class Allocator>
inline shared_ptr<T>::shared_ptr(T * object, Allocator& allocator)
	: shared_ptr<T>()
{
	this->myControlBlockStorage = create_control_block(object, allocator);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
// The Deleter callable has signature void(T* obj, Allocator& alloc)
template<class T>
template<class Deleter>
inline shared_ptr<T>::shared_ptr(T* object, Deleter && deleter)
	: shared_ptr<T>()
{
	aspdetail::default_allocator alloc;
	this->myControlBlockStorage = create_control_block(object, std::forward<Deleter&&>(deleter), alloc);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
// The Deleter callable has signature void(T* obj, Allocator& alloc)
template<class T>
template<class Deleter, class Allocator>
inline shared_ptr<T>::shared_ptr(T* object, Deleter && deleter, Allocator & allocator)
	: shared_ptr<T>()
{
	this->myControlBlockStorage = create_control_block(object, std::forward<Deleter&&>(deleter), allocator);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
template<class T>
inline void shared_ptr<T>::fill_local_refs() noexcept
{
	aspdetail::control_block_base_interface<T>* const cb(this->get_control_block());
	const uint8_t localRefs(this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);

	if (cb && localRefs < aspdetail::Local_Ref_Fill_Boundary) {
		const uint8_t newRefs(std::numeric_limits<std::uint8_t>::max() - localRefs);
		cb->incref(newRefs);
		this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();
	}
}
template<class T>
inline shared_ptr<T>::shared_ptr(compressed_storage from) noexcept
	: aspdetail::ptr_base<T>(from)
{
}

template <class T>
template<class Deleter, class Allocator>
inline typename aspdetail::compressed_storage shared_ptr<T>::create_control_block(T* object, Deleter&& deleter, Allocator& allocator)
{
	aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>* controlBlock(nullptr);

	constexpr std::size_t blockSize(alloc_size_claim_custom_delete<Allocator, Deleter>());

	void* block(nullptr);

	try {
		block = allocator.allocate(blockSize);

		if ((std::uintptr_t)block % alignof(std::max_align_t) != 0) {
			throw std::exception("make_shared expects at least alignof(max_align_t) allocates");
		}

		controlBlock = new (block) aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>(object, allocator, std::forward<Deleter&&>(deleter));
	}
	catch (...) {
		allocator.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		deleter(object, allocator);
		throw;
	}
	compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();

	return storage;
}
template<class T>
template <class Allocator>
inline typename aspdetail::compressed_storage shared_ptr<T>::create_control_block(T * object, Allocator& allocator)
{
	aspdetail::control_block_claim<T, Allocator>* controlBlock(nullptr);

	constexpr std::size_t blockSize(alloc_size_claim<Allocator>());

	void* block(nullptr);

	try {
		block = allocator.allocate(blockSize);

		if ((std::uintptr_t)block % alignof(std::max_align_t) != 0){
			throw std::exception("make_shared expects at least alignof(max_align_t) allocates");
		}

		controlBlock = new (block) aspdetail::control_block_claim<T, Allocator>(object, allocator);
	}
	catch (...) {
		allocator.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		object->~T();
		allocator.deallocate(reinterpret_cast<std::uint8_t*>(object), sizeof(T));
		throw;
	}

	compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();

	return storage;
}
template<class T>
inline shared_ptr<T>& shared_ptr<T>::operator=(const shared_ptr<T>& other) noexcept
{
	const compressed_storage otherValue(other.myControlBlockStorage);
	const std::uint8_t refsToSteal(otherValue.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] / 2);

	other.myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] -= refsToSteal;

	const std::uint8_t newRefs(refsToSteal ? 0 : std::numeric_limits<std::uint8_t>::max());

	aspdetail::control_block_base_interface<T>* const otherCb(this->to_control_block(otherValue));
	if (otherCb && newRefs) {
		otherCb->incref(newRefs);
	}

	aspdetail::control_block_base_interface<T>* const cb(this->get_control_block());
	if (cb) {
		cb->decref(this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
	}

	compressed_storage newCb(otherValue);
	newCb.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = refsToSteal + newRefs;

	this->myControlBlockStorage = newCb;
	this->myPtr = other.myPtr;

	return *this;
}
template<class T>
inline shared_ptr<T>& shared_ptr<T>::operator=(shared_ptr<T>&& other) noexcept
{
	std::swap(this->myControlBlockStorage, other.myControlBlockStorage);
	std::swap(this->myPtr, other.myPtr);
	return *this;
}
#if defined(_HAS_CXX17)
template<class T>
template <class Allocator>
inline constexpr std::size_t shared_ptr<T>::alloc_size_make_shared() noexcept
{
	return sizeof(aspdetail::control_block_make_shared<T, Allocator>);
}
#else
template<class T>
template <class Allocator>
inline constexpr std::size_t shared_ptr<T>::alloc_size_make_shared() noexcept
{
	constexpr std::size_t align(alignof(aspdetail::control_block_make_shared<T, Allocator>));
	constexpr std::size_t maxOffset(align - alignof(std::max_align_t));
	return sizeof(aspdetail::control_block_make_shared<T, Allocator>) + (alignof(std::max_align_t) < align ? maxOffset : 0);
}
#endif
template<class T>
template <class Allocator>
inline constexpr std::size_t shared_ptr<T>::alloc_size_claim() noexcept
{
	return sizeof(aspdetail::control_block_claim<T, Allocator>);
}
template<class T>
template<class Allocator, class Deleter>
inline constexpr std::size_t shared_ptr<T>::alloc_size_claim_custom_delete() noexcept
{
	return sizeof(aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>);
}
// raw_ptr does not share in ownership of the object
template <class T>
class raw_ptr : public aspdetail::ptr_base<T>
{
public:
	inline constexpr raw_ptr() noexcept;

	using aspdetail::ptr_base<T>::ptr_base;

	constexpr raw_ptr(raw_ptr<T>&& other) noexcept;
	constexpr raw_ptr(const raw_ptr<T>& other) noexcept;

	explicit constexpr raw_ptr(const shared_ptr<T>& from) noexcept;

	explicit raw_ptr(const atomic_shared_ptr<T>& from) noexcept;

	constexpr raw_ptr<T>& operator=(const raw_ptr<T>& other) noexcept;
	constexpr raw_ptr<T>& operator=(raw_ptr<T>&& other) noexcept;
	constexpr raw_ptr<T>& operator=(const shared_ptr<T>& from) noexcept;
	constexpr raw_ptr<T>& operator=(const atomic_shared_ptr<T>& from) noexcept;

private:
	typedef aspdetail::compressed_storage compressed_storage;

	explicit constexpr raw_ptr(compressed_storage from) noexcept;

	friend class aspdetail::ptr_base<T>;
	friend class atomic_shared_ptr<T>;
};
template<class T>
inline constexpr raw_ptr<T>::raw_ptr() noexcept
	: aspdetail::ptr_base<T>()
{
	this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 0;
}
template<class T>
inline constexpr raw_ptr<T>::raw_ptr(raw_ptr<T>&& other) noexcept
	: raw_ptr()
{
	operator=(std::move(other));
}
template<class T>
inline constexpr raw_ptr<T>::raw_ptr(const raw_ptr<T>& other) noexcept
	: raw_ptr()
{
	operator=(other);
}
template<class T>
inline constexpr raw_ptr<T>::raw_ptr(const shared_ptr<T>& from) noexcept
	: aspdetail::ptr_base<T>(from.myControlBlockStorage)
{
}
template<class T>
inline raw_ptr<T>::raw_ptr(const atomic_shared_ptr<T>& from) noexcept
	: aspdetail::ptr_base<T>(from.myStorage.load())
{
}
template<class T>
inline constexpr raw_ptr<T>& raw_ptr<T>::operator=(const raw_ptr<T>& other)  noexcept{
	this->myControlBlockStorage = other.myControlBlockStorage;
	this->myPtr = other.myPtr;

	return *this;
}
template<class T>
inline constexpr raw_ptr<T>& raw_ptr<T>::operator=(raw_ptr<T>&& other) noexcept {
	std::swap(this->myControlBlockStorage, other.myControlBlockStorage);
	std::swap(this->myPtr, other.myPtr);
	return *this;
}
template<class T>
inline constexpr raw_ptr<T>& raw_ptr<T>::operator=(const shared_ptr<T>& from) noexcept
{
	*this = raw_ptr<T>(from);
	return *this;
}
template<class T>
inline constexpr raw_ptr<T>& raw_ptr<T>::operator=(const atomic_shared_ptr<T>& from) noexcept
{
	*this = raw_ptr<T>(from);
	return *this;
}
template<class T>
inline constexpr raw_ptr<T>::raw_ptr(compressed_storage from) noexcept
	: aspdetail::ptr_base<T>(from)
{
}
template<class T, class ...Args>
inline shared_ptr<T> make_shared(Args&& ...args)
{
	aspdetail::default_allocator alloc;
	return make_shared<T>(alloc, std::forward<Args&&>(args)...);
}
template<class T, class Allocator, class ...Args>
inline shared_ptr<T> make_shared(Args&& ...args)
{
	Allocator alloc;
	return make_shared<T, Allocator>(alloc, std::forward<Args&&>(args)...);
}
template<class T, class Allocator, class ...Args>
inline shared_ptr<T> make_shared(Allocator& allocator, Args&& ...args)
{
	using block_type = struct alignas(alignof(T)) { std::uint8_t dummy[shared_ptr<T>::template alloc_size_make_shared<Allocator>()]{}; };
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<block_type>;

	rebound_alloc rebound(allocator);

	std::uint8_t* const block((std::uint8_t*)rebound.allocate(1));

	constexpr std::uintptr_t align(alignof(aspdetail::control_block_make_shared<T, Allocator>));

	const std::uintptr_t mod((std::uintptr_t)block % align);
	const std::uintptr_t offset(mod ? align - mod : 0);

	aspdetail::control_block_make_shared<T, Allocator>* controlBlock(nullptr);
	try {
		aspdetail::assert_alignment(block, align);

		controlBlock = new (reinterpret_cast<std::uint8_t*>(block + offset)) aspdetail::control_block_make_shared<T, Allocator>(allocator, (std::uint8_t)offset, std::forward<Args&&>(args)...);
	}
	catch (...) {
		if (controlBlock) {
			(*controlBlock).~control_block_make_shared<T, Allocator>();
		}
		throw;
		allocator.deallocate(block, shared_ptr<T>::template alloc_size_make_shared<Allocator>());
	}

	aspdetail::compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();

	return shared_ptr<T>(storage);
}
};
#pragma warning(pop)