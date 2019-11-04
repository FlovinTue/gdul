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
#include <stdexcept>
#include <memory>
#include <cstddef>

// (not atomic_)shared_ptr uses local refs by default to speed up the copy process,
// which means if the number of local refs kept is > 1, the copyee must modify local
// state (not concurrency safe) when stealing refs. Define GDUL_SP_SAFE_COPY to disable this
// behaviour. The number of local refs may also be adjusted using shared_ptr<T>::set_local_refs(n)

/* #define GDUL_SP_SAFE_COPY */

#undef max

#pragma warning(push, 2)
// Anonymous union
#pragma warning(disable : 4201)
// Non-class enums 
#pragma warning(disable : 26812)

namespace gdul
{
namespace aspdetail
{
typedef std::allocator<std::uint8_t> default_allocator;

static constexpr std::uint64_t Ptr_Mask = (std::numeric_limits<std::uint64_t>::max() >> 16);
static constexpr std::uint64_t Versioned_Ptr_Mask = (std::numeric_limits<std::uint64_t>::max() >> 8);

union compressed_storage
{
	constexpr compressed_storage()  noexcept : myU64(0ULL) {}
	constexpr compressed_storage(std::uint64_t from) noexcept : myU64(from) {}

	std::uint64_t myU64;
	std::uint32_t myU32[2];
	std::uint16_t myU16[4];
	std::uint8_t myU8[8];
};

template <class T>
constexpr void assert_alignment(std::uint8_t* block);

template <class T>
struct disable_deduction;

template <std::uint8_t Align, std::size_t Size>
struct aligned_storage;

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

struct memory_orders { std::memory_order myFirst, mySecond; };

using size_type = std::uint32_t;

static constexpr std::uint8_t Local_Ref_Index(7);
static constexpr std::uint64_t Local_Ref_Step(1ULL << (Local_Ref_Index * 8));
static constexpr std::uint8_t Local_Ref_Fill_Boundary(112);
static constexpr std::uint8_t Default_Local_Refs =
#if defined (GDUL_SP_SAFE_COPY) 
1;
#else 
std::numeric_limits<std::uint8_t>::max();
#endif

enum STORAGE_BYTE : std::uint8_t
{
	STORAGE_BYTE_VERSION = 6,
	STORAGE_BYTE_LOCAL_REF = Local_Ref_Index,
};
enum CAS_FLAG : std::uint8_t
{
	CAS_FLAG_NONE = 0,
	CAS_FLAG_CAPTURE_ON_FAILURE = 1 << 0,
	CAS_FLAG_STEAL_PREVIOUS = 1 << 1,
};
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

	// compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept;

	inline void store(const shared_ptr<T>& from, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline void store(shared_ptr<T>&& from, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline shared_ptr<T> exchange(const shared_ptr<T>& with, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline shared_ptr<T> exchange(shared_ptr<T>&& with, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline std::uint8_t get_version() const noexcept;

	inline shared_ptr<T> unsafe_load(std::memory_order order = std::memory_order_seq_cst) const;

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

	raw_ptr<T> get_raw_ptr() const noexcept;

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

	inline compressed_storage unsafe_copy_internal(std::memory_order order) const;
	inline compressed_storage copy_internal(std::memory_order order) const noexcept;

	inline void unsafe_store_internal(compressed_storage from, std::memory_order order);
	inline void store_internal(compressed_storage from, std::memory_order order) noexcept;

	inline compressed_storage unsafe_exchange_internal(compressed_storage with, std::memory_order order);
	inline compressed_storage exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags, std::memory_order order) noexcept;

	inline bool compare_exchange_weak_internal(compressed_storage& expected, compressed_storage desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept;

	inline void unsafe_fill_local_refs() const noexcept;
	inline void try_fill_local_refs(compressed_storage& expected) const noexcept;

	inline constexpr aspdetail::control_block_base_interface<T>* to_control_block(compressed_storage from) const noexcept;

	friend class shared_ptr<T>;
	friend class raw_ptr<T>;
	friend class aspdetail::ptr_base<T>;

	union
	{
		// mutable for altering local refs
		mutable std::atomic<std::uint64_t> myStorage;
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
inline bool atomic_shared_ptr<T>::compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	if (desired.get_local_refs() < aspdetail::Local_Ref_Fill_Boundary)
		desired.set_local_refs(std::numeric_limits<std::uint8_t>::max());

	const compressed_storage desired_(desired.myControlBlockStorage.myU64);

	compressed_storage expected_(myStorage.load(std::memory_order_relaxed));
	expected_.myU64 &= ~aspdetail::Versioned_Ptr_Mask;
	expected_.myU64 |= expected.myControlBlockStorage.myU64 & aspdetail::Versioned_Ptr_Mask;

	const aspdetail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	constexpr bool needsCapture(std::is_same<raw_type, shared_ptr<T>>::value);
	const std::uint8_t flags(aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE * needsCapture);

	const std::uint64_t preCompare(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);
	do {
		if (compare_exchange_weak_internal(expected_, desired_, static_cast<aspdetail::CAS_FLAG>(flags), orders)) {

			desired.clear();

			return true;
		}

	} while (preCompare == (expected_.myU64 & aspdetail::Versioned_Ptr_Mask));

	expected = raw_type(expected_);

	return false;
}
template<class T>
template<class PtrType>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	if (desired.get_local_refs() < aspdetail::Local_Ref_Fill_Boundary)
		desired.set_local_refs(std::numeric_limits<std::uint8_t>::max());

	const compressed_storage desired_(desired.myControlBlockStorage.myU64);
	compressed_storage expected_(myStorage.load(std::memory_order_relaxed));
	expected_.myU64 &= ~aspdetail::Versioned_Ptr_Mask;
	expected_.myU64 |= expected.myControlBlockStorage.myU64 & aspdetail::Versioned_Ptr_Mask;

	const aspdetail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	constexpr bool needsCapture(std::is_same<raw_type, shared_ptr<T>>::value);
	const std::uint8_t flags(aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE * needsCapture);

	const std::uint64_t preCompare(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);

	if (compare_exchange_weak_internal(expected_, desired_, static_cast<aspdetail::CAS_FLAG>(flags), orders)) {

		desired.clear();

		return true;
	}

	const std::uint64_t postCompare(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);

	// Failed, but not spuriously (other thread loaded, or compare_exchange_weak spurious fail)
	if (preCompare != postCompare) {
		expected = raw_type(expected_);
	}

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
inline shared_ptr<T> atomic_shared_ptr<T>::load(std::memory_order order) const noexcept
{
	return shared_ptr<T>(copy_internal(order));
}
template<class T>
inline void atomic_shared_ptr<T>::store(const shared_ptr<T>& from, std::memory_order order) noexcept
{
	store(shared_ptr<T>(from), order);
}
template<class T>
inline void atomic_shared_ptr<T>::store(shared_ptr<T>&& from, std::memory_order order) noexcept
{
	if (from.get_local_refs() < aspdetail::Local_Ref_Fill_Boundary)
		from.set_local_refs(std::numeric_limits<std::uint8_t>::max());

	store_internal(from.myControlBlockStorage.myU64, order);
	from.clear();
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::exchange(const shared_ptr<T>& with, std::memory_order order) noexcept
{
	return exchange(shared_ptr<T>(with), order);
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::exchange(shared_ptr<T>&& with, std::memory_order order) noexcept
{
	if (with.get_local_refs() < aspdetail::Local_Ref_Fill_Boundary)
		with.set_local_refs(std::numeric_limits<std::uint8_t>::max());

	compressed_storage previous(exchange_internal(with.myControlBlockStorage, aspdetail::CAS_FLAG_STEAL_PREVIOUS, order));
	with.clear();
	return shared_ptr<T>(previous);
}
template<class T>
inline std::uint8_t atomic_shared_ptr<T>::get_version() const noexcept
{
	const compressed_storage storage(myStorage.load(std::memory_order_relaxed));
	return storage.myU8[aspdetail::STORAGE_BYTE_VERSION];
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::unsafe_load(std::memory_order order) const
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
	with.clear();
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
	from.clear();
}
template<class T>
inline raw_ptr<T> atomic_shared_ptr<T>::get_raw_ptr() const noexcept
{
	const compressed_storage storage(myStorage.load(std::memory_order_relaxed));
	return raw_ptr<T>(storage);
}
template<class T>
inline T* atomic_shared_ptr<T>::unsafe_get_owned()
{
	aspdetail::control_block_base_interface<T>* const cb(get_control_block());
	if (cb) {
		return cb->get_owned();
	}
	return nullptr;
}
template<class T>
inline const T* atomic_shared_ptr<T>::unsafe_get_owned() const
{
	const aspdetail::control_block_base_interface<T>* const cb(get_control_block());
	if (cb) {
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
inline bool atomic_shared_ptr<T>::compare_exchange_weak_internal(compressed_storage& expected, compressed_storage desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept
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
inline void atomic_shared_ptr<T>::try_fill_local_refs(compressed_storage& expected) const noexcept
{
	if (!(expected.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary)) {
		return;
	}

	aspdetail::control_block_base_interface<T>* const cb(to_control_block(expected));

	if (!cb) {
		return;
	}

	const compressed_storage initialPtrBlock(expected.myU64 & aspdetail::Versioned_Ptr_Mask);

	do {
		const std::uint8_t localRefs(expected.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
		const std::uint8_t newRefs(std::numeric_limits<std::uint8_t>::max() - localRefs);

		compressed_storage desired(expected);
		desired.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();

		cb->incref(newRefs);

		if (myStorage.compare_exchange_weak(expected.myU64, desired.myU64, std::memory_order_relaxed)) {
			expected.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();
			return;
		}

		cb->decref(newRefs);

	} while (
		(expected.myU64 & aspdetail::Versioned_Ptr_Mask) == initialPtrBlock.myU64 &&
		expected.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary);
}
template <class T>
inline union aspdetail::compressed_storage atomic_shared_ptr<T>::copy_internal(std::memory_order order) const noexcept
{
	compressed_storage initial(myStorage.fetch_sub(aspdetail::Local_Ref_Step, std::memory_order_relaxed));
	initial.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] -= 1;

	compressed_storage expected(initial);
	try_fill_local_refs(expected);

	initial.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 1;

	return initial;
}
template <class T>
inline union aspdetail::compressed_storage atomic_shared_ptr<T>::unsafe_copy_internal(std::memory_order order) const
{
	compressed_storage storage(myStorage.load(std::memory_order_relaxed));

	compressed_storage newStorage(storage);
	newStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] -= 1;
	myStorage.store(newStorage.myU64, std::memory_order_relaxed);

	storage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 1;

	unsafe_fill_local_refs();

	std::atomic_thread_fence(order);

	return storage;
}
template<class T>
inline union aspdetail::compressed_storage atomic_shared_ptr<T>::unsafe_exchange_internal(compressed_storage with, std::memory_order order)
{
	const compressed_storage old(myStorage.load(std::memory_order_relaxed));

	compressed_storage replacement(with.myU64);
	replacement.myU8[aspdetail::STORAGE_BYTE_VERSION] = old.myU8[aspdetail::STORAGE_BYTE_VERSION] + 1;

	myStorage.store(replacement.myU64, std::memory_order_relaxed);

	unsafe_fill_local_refs();

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

	unsafe_fill_local_refs();

	std::atomic_thread_fence(order);
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_fill_local_refs() const noexcept
{
	const compressed_storage current(myStorage.load(std::memory_order_relaxed));
	aspdetail::control_block_base_interface<T>* const cb(to_control_block(current));

	if (cb && current.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary) {
		const std::uint8_t newRefs(std::numeric_limits<std::uint8_t>::max() - current.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);

		cb->incref(newRefs);

		compressed_storage newStorage(current);
		newStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();

		myStorage.store(newStorage.myU64, std::memory_order_relaxed);
	}
}
template<class T>
inline union aspdetail::compressed_storage atomic_shared_ptr<T>::exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags, std::memory_order order) noexcept
{
	compressed_storage expected(myStorage.load(std::memory_order_relaxed));
	while (!compare_exchange_weak_internal(expected, to, flags, { order, std::memory_order_relaxed }));
	return expected;
}
namespace aspdetail
{
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
	: myUseCount(Default_Local_Refs)
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
	if (!(myUseCount.fetch_sub(count, std::memory_order_acq_rel) - count)) {
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
	template <class ...Args, class U = T, std::enable_if_t<std::is_array<U>::value> * = nullptr>
	control_block_make_shared(Allocator& alloc, std::uint8_t blockOffset, Args&& ...args);
	template <class ...Args, class U = T, std::enable_if_t<!std::is_array<U>::value> * = nullptr>
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

	(*this).~control_block_make_shared<T, Allocator>();

	constexpr std::size_t blockSize(shared_ptr<T>::template alloc_size_make_shared<Allocator>());
	constexpr std::size_t blockAlign(alignof(T));

	using block_type = aspdetail::aligned_storage<blockAlign, blockSize>;
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<block_type>;

	rebound_alloc rebound(alloc);

	uint8_t* beginPtr((std::uint8_t*)this - this->myBlockOffset);
	block_type* const typedBlock((block_type*)beginPtr);

	rebound.deallocate(typedBlock, 1);
}
template <class T, class Allocator>
class control_block_claim : public control_block_base_members<T, Allocator>
{
public:
	control_block_claim(T* obj, Allocator& alloc) noexcept;
	void destroy() noexcept override;
};
template<class T, class Allocator>
inline control_block_claim<T, Allocator>::control_block_claim(T* obj, Allocator& alloc) noexcept
	:control_block_base_members<T, Allocator>(obj, alloc)
{
}
template<class T, class Allocator>
inline void control_block_claim<T, Allocator>::destroy() noexcept
{
	Allocator alloc(this->myAllocator);

	delete this->myPtr;
	(*this).~control_block_claim<T, Allocator>();

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(alloc);

	rebound.deallocate(reinterpret_cast<std::uint8_t*>(this), shared_ptr<T>::template alloc_size_claim<Allocator>());
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
inline control_block_claim_custom_delete<T, Allocator, Deleter>::control_block_claim_custom_delete(T* obj, Allocator& alloc, Deleter&& deleter) noexcept
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

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(alloc);

	rebound.deallocate(reinterpret_cast<std::uint8_t*>(this), shared_ptr<T>::template alloc_size_claim_custom_delete<Allocator, Deleter>());
}
template <class T>
struct disable_deduction
{
	using type = T;
};
template <std::uint8_t Align, std::size_t Size>
struct alignas(Align) aligned_storage
{
	std::uint8_t dummy[Size]{};
};
template<class T>
constexpr void assert_alignment(std::uint8_t* block)
{

#if 201700 < __cplusplus || _HAS_CXX17
	if ((std::uintptr_t)block % alignof(T) != 0) {
		throw std::runtime_error("conforming with C++17 make_shared expects alignof(T) allocates");
	}
#else
	static_assert(!(std::numeric_limits<std::uint8_t>::max() < alignof(T)), "make_shared supports only supports up to std::numeric_limits<std::uint8_t>::max() byte aligned types");

	if ((std::uintptr_t)block % alignof(std::max_align_t) != 0) {
		throw std::runtime_error("make_shared expects at least alignof(max_align_t) allocates");
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

	inline constexpr std::uint8_t get_version() const noexcept;

	inline size_type use_count() const noexcept;

protected:
	inline constexpr const control_block_base_interface<T>* get_control_block() const noexcept;
	inline constexpr control_block_base_interface<T>* get_control_block() noexcept;

	inline constexpr ptr_base() noexcept;
	inline constexpr ptr_base(compressed_storage from) noexcept;

	inline void clear() noexcept;

	constexpr control_block_base_interface<T>* to_control_block(compressed_storage from) noexcept;
	constexpr T* to_object(compressed_storage from) noexcept;

	constexpr const control_block_base_interface<T>* to_control_block(compressed_storage from) const noexcept;
	constexpr const T* to_object(compressed_storage from) const noexcept;

	friend class atomic_shared_ptr<T>;

	// mutable for altering local refs
	mutable compressed_storage myControlBlockStorage;
};
template <class T>
inline constexpr ptr_base<T>::ptr_base() noexcept
	: myControlBlockStorage()
{
}
template<class T>
inline constexpr ptr_base<T>::ptr_base(std::nullptr_t) noexcept
	: ptr_base<T>()
{
}
template<class T>
inline constexpr ptr_base<T>::ptr_base(std::nullptr_t, std::uint8_t version) noexcept
	: myControlBlockStorage(0ULL | (std::uint64_t(version) << (STORAGE_BYTE_VERSION * 8)))
{
}
template <class T>
inline constexpr ptr_base<T>::ptr_base(compressed_storage from) noexcept
	: ptr_base<T>()
{
	myControlBlockStorage = from;
	myControlBlockStorage.myU8[STORAGE_BYTE_LOCAL_REF] *= operator bool();
}
template<class T>
inline void ptr_base<T>::clear() noexcept
{
	myControlBlockStorage = compressed_storage();
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
inline constexpr bool operator==(std::nullptr_t /*null*/, const ptr_base<T>& ptr) noexcept
{
	return !ptr;
}
template <class T>
inline constexpr bool operator!=(std::nullptr_t /*null*/, const ptr_base<T>& ptr) noexcept
{
	return ptr;
}
template <class T>
inline constexpr bool operator==(const ptr_base<T>& ptr, std::nullptr_t /*null*/) noexcept
{
	return !ptr;
}
template <class T>
inline constexpr bool operator!=(const ptr_base<T>& ptr, std::nullptr_t /*null*/) noexcept
{
	return ptr;
}
template <class T>
inline constexpr control_block_base_interface<T>* ptr_base<T>::to_control_block(compressed_storage from) noexcept
{
	return reinterpret_cast<control_block_base_interface<T>*>(from.myU64 & Ptr_Mask);
}
template <class T>
inline constexpr const control_block_base_interface<T>* ptr_base<T>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<const control_block_base_interface<T>*>(from.myU64 & Ptr_Mask);
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
inline constexpr const T* ptr_base<T>::to_object(compressed_storage from) const noexcept
{
	const control_block_base_interface<T>* const cb(to_control_block(from));
	if (cb) {
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
inline constexpr control_block_base_interface<T>* ptr_base<T>::get_control_block() noexcept
{
	return to_control_block(myControlBlockStorage);
}
template<class T>
inline constexpr std::uint8_t ptr_base<T>::get_version() const noexcept
{
	return myControlBlockStorage.myU8[STORAGE_BYTE_VERSION];
}
};
template <class T>
class shared_ptr : public aspdetail::ptr_base<T>
{
public:
	inline constexpr shared_ptr() noexcept;

	inline constexpr shared_ptr(std::nullptr_t) noexcept;
	inline constexpr shared_ptr(std::nullptr_t, std::uint8_t version) noexcept;

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

	inline constexpr explicit operator T* () noexcept; 
	inline constexpr explicit operator const T* () const noexcept; 

	inline constexpr const T* get_owned() const noexcept; 
	inline constexpr T* get_owned() noexcept; 

	inline constexpr T* operator->();
	inline constexpr T& operator*();

	inline constexpr const T* operator->() const;
	inline constexpr const T& operator*() const; 

	inline const T& operator[](aspdetail::size_type index) const; 
	inline T& operator[](aspdetail::size_type index); 

	inline constexpr raw_ptr<T> get_raw_ptr() const noexcept;

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

	// Adjusts the amount of local refs kept for fast copies. Setting this to 1
	// means a copy operation will not attempt to modify local state, and thus is
	// concurrency safe(so long as the object remains unaltered). 
	// Use of local refs may be completely disabled via define GDUL_SP_SAFE_COPY
	inline void set_local_refs(std::uint8_t target) noexcept;

	inline constexpr std::uint8_t get_local_refs() const noexcept;
private:

	constexpr void clear() noexcept;

	using compressed_storage = aspdetail::compressed_storage;
	inline constexpr shared_ptr(union aspdetail::compressed_storage from) noexcept;

	template<class Deleter, class Allocator>
	inline compressed_storage create_control_block(T* object, Deleter&& deleter, Allocator& allocator);
	template <class Allocator>
	inline compressed_storage create_control_block(T* object, Allocator& allocator);

	friend class raw_ptr<T>;
	friend class atomic_shared_ptr<T>;

	template <class U, class Allocator, class ...Args>
	friend shared_ptr<U> make_shared(Allocator&, Args&&...);

	T* myPtr;
};
template <class T>
inline constexpr shared_ptr<T>::shared_ptr() noexcept
	: aspdetail::ptr_base<T>()
	, myPtr(nullptr)
{

}
template <class T>
inline constexpr shared_ptr<T>::shared_ptr(std::nullptr_t) noexcept
	: aspdetail::ptr_base<T>(nullptr)
	, myPtr(nullptr)
{

}
template <class T>
inline constexpr shared_ptr<T>::shared_ptr(std::nullptr_t, std::uint8_t version) noexcept
	: aspdetail::ptr_base<T>(nullptr, version)
	, myPtr(nullptr)
{

}
template<class T>
inline shared_ptr<T>::~shared_ptr() noexcept
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
inline shared_ptr<T>::shared_ptr(T* object)
	: shared_ptr<T>()
{
	aspdetail::default_allocator alloc;
	this->myControlBlockStorage = create_control_block(object, alloc);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
template <class T>
template <class Allocator>
inline shared_ptr<T>::shared_ptr(T* object, Allocator& allocator)
	: shared_ptr<T>()
{
	this->myControlBlockStorage = create_control_block(object, allocator);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
// The Deleter callable has signature void(T* obj, Allocator& alloc)
template<class T>
template<class Deleter>
inline shared_ptr<T>::shared_ptr(T* object, Deleter&& deleter)
	: shared_ptr<T>()
{
	aspdetail::default_allocator alloc;
	this->myControlBlockStorage = create_control_block(object, std::forward<Deleter&&>(deleter), alloc);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
// The Deleter callable has signature void(T* obj, Allocator& alloc)
template<class T>
template<class Deleter, class Allocator>
inline shared_ptr<T>::shared_ptr(T* object, Deleter&& deleter, Allocator& allocator)
	: shared_ptr<T>()
{
	this->myControlBlockStorage = create_control_block(object, std::forward<Deleter&&>(deleter), allocator);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
template<class T>
inline constexpr void shared_ptr<T>::clear() noexcept
{
	myPtr = nullptr;
	aspdetail::ptr_base<T>::clear();
}
template<class T>
inline void shared_ptr<T>::set_local_refs(std::uint8_t target) noexcept
{
	if (aspdetail::control_block_base_interface<T>* const cb = this->get_control_block()) {
		const uint8_t localRefs(this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);

		if (localRefs < target) {
			cb->incref(target - localRefs);
		}
		else if (target < localRefs) {
			cb->decref(localRefs - target);
		}

		this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = target;
		this->myControlBlockStorage.myU64 *= (bool)target;
		myPtr = (T*)((uint64_t)myPtr * (bool)target);
	}
}
template <class T>
inline constexpr std::uint8_t shared_ptr<T>::get_local_refs() const noexcept
{
	return this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF];
}
template<class T>
inline constexpr shared_ptr<T>::shared_ptr(union aspdetail::compressed_storage from) noexcept
	: aspdetail::ptr_base<T>(from)
	, myPtr(this->to_object(from))
{
}
template<class T>
inline constexpr raw_ptr<T> shared_ptr<T>::get_raw_ptr() const noexcept
{
	return raw_ptr<T>(this->myControlBlockStorage);
}
template <class T>
inline constexpr shared_ptr<T>::operator T* () noexcept
{
	return get_owned();
}
template <class T>
inline constexpr shared_ptr<T>::operator const T* () const noexcept
{
	return get_owned();
}
template <class T>
inline constexpr T* shared_ptr<T>::operator->()
{
	return get_owned();
}
template <class T>
inline constexpr T& shared_ptr<T>::operator*()
{
	return *get_owned();
}
template <class T>
inline constexpr const T* shared_ptr<T>::operator->() const
{
	return get_owned();
}
template <class T>
inline constexpr const T& shared_ptr<T>::operator*() const
{
	return *get_owned();
}
template <class T>
inline const T& shared_ptr<T>::operator[](aspdetail::size_type index) const
{
	return get_owned()[index];
}
template <class T>
inline T& shared_ptr<T>::operator[](aspdetail::size_type index)
{
	return get_owned()[index];
}
template <class T>
inline constexpr const T* shared_ptr<T>::get_owned() const noexcept
{
	return myPtr;
}
template <class T>
inline constexpr T* shared_ptr<T>::get_owned() noexcept
{
	return myPtr;
}
template <class T>
template<class Deleter, class Allocator>
inline union aspdetail::compressed_storage shared_ptr<T>::create_control_block(T* object, Deleter&& deleter, Allocator& allocator)
{
	static_assert(!(8 < alignof(Deleter)), "No support for custom aligned deleter objects");

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(allocator);

	aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>* controlBlock(nullptr);

	constexpr std::size_t blockSize(alloc_size_claim_custom_delete<Allocator, Deleter>());

	void* block(nullptr);

	try {
		block = rebound.allocate(blockSize);

		if ((std::uintptr_t)block % alignof(std::max_align_t) != 0) {
			throw std::runtime_error("make_shared expects at least alignof(max_align_t) allocates");
		}

		controlBlock = new (block) aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>(object, allocator, std::forward<Deleter&&>(deleter));
	}
	catch (...) {
		rebound.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		deleter(object, allocator);
		throw;
	}
	compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;

	return storage;
}
template<class T>
template <class Allocator>
inline union aspdetail::compressed_storage shared_ptr<T>::create_control_block(T* object, Allocator& allocator)
{
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(allocator);

	aspdetail::control_block_claim<T, Allocator>* controlBlock(nullptr);

	constexpr std::size_t blockSize(alloc_size_claim<Allocator>());

	void* block(nullptr);

	try {
		block = rebound.allocate(blockSize);

		if ((std::uintptr_t)block % alignof(std::max_align_t) != 0) {
			throw std::runtime_error("make_shared expects at least alignof(max_align_t) allocates");
		}

		controlBlock = new (block) aspdetail::control_block_claim<T, Allocator>(object, rebound);
	}
	catch (...) {
		rebound.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		delete object;
		throw;
	}

	compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;

	return storage;
}
template<class T>
inline shared_ptr<T>& shared_ptr<T>::operator=(const shared_ptr<T>& other) noexcept
{
	compressed_storage copy(other.myControlBlockStorage);

	if (aspdetail::control_block_base_interface<T>* const copyCb = this->to_control_block(copy)) {
#ifndef GDUL_SP_SAFE_COPY
		copy.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = copy.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] / 2;
		if (copy.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]) {
			other.myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] -= copy.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF];
		}
		else {
#endif
			copy.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;
			copyCb->incref(copy.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
#ifndef GDUL_SP_SAFE_COPY
		}
#endif
	}

	if (aspdetail::control_block_base_interface<T>* const cb = this->get_control_block()) {
		cb->decref(this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
	}

	this->myControlBlockStorage = copy;
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
#if 201700 < __cplusplus || _HAS_CXX17
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

	inline constexpr explicit operator T* () noexcept;
	inline constexpr explicit operator const T* () const noexcept;

	inline constexpr const T* get_owned() const noexcept;
	inline constexpr T* get_owned() noexcept;

	inline constexpr T* operator->();
	inline constexpr T& operator*();

	inline constexpr const T* operator->() const;
	inline constexpr const T& operator*() const;

	inline const T& operator[](aspdetail::size_type index) const;
	inline T& operator[](aspdetail::size_type index);

	constexpr raw_ptr<T>& operator=(const raw_ptr<T>& other) noexcept;
	constexpr raw_ptr<T>& operator=(raw_ptr<T>&& other) noexcept;
	constexpr raw_ptr<T>& operator=(const shared_ptr<T>& from) noexcept;
	constexpr raw_ptr<T>& operator=(const atomic_shared_ptr<T>& from) noexcept;

private:
	typedef aspdetail::compressed_storage compressed_storage;

	explicit constexpr raw_ptr(compressed_storage from) noexcept;

	friend class aspdetail::ptr_base<T>;
	friend class atomic_shared_ptr<T>;
	friend class shared_ptr<T>;
};
template<class T>
inline constexpr raw_ptr<T>::raw_ptr() noexcept
	: aspdetail::ptr_base<T>()
{
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
	this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 0;
}
template<class T>
inline raw_ptr<T>::raw_ptr(const atomic_shared_ptr<T>& from) noexcept
	: aspdetail::ptr_base<T>(from.myStorage.load(std::memory_order_relaxed))
{
	this->myControlBlockStorage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 0;
}
template<class T>
inline constexpr raw_ptr<T>& raw_ptr<T>::operator=(const raw_ptr<T>& other)  noexcept
{
	this->myControlBlockStorage = other.myControlBlockStorage;

	return *this;
}
template<class T>
inline constexpr raw_ptr<T>& raw_ptr<T>::operator=(raw_ptr<T>&& other) noexcept
{
	std::swap(this->myControlBlockStorage, other.myControlBlockStorage);
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
template <class T>
inline constexpr raw_ptr<T>::operator T* () noexcept
{
	return get_owned();
}
template <class T>
inline constexpr raw_ptr<T>::operator const T* () const noexcept
{
	return get_owned();
}
template <class T>
inline constexpr const T* raw_ptr<T>::get_owned() const noexcept
{
	return this->to_object(this->myControlBlockStorage);
}
template <class T>
inline constexpr T* raw_ptr<T>::get_owned() noexcept
{
	return this->to_object(this->myControlBlockStorage);
}
template <class T>
inline constexpr T* raw_ptr<T>::operator->()
{
	return get_owned();
}
template <class T>
inline constexpr T& raw_ptr<T>::operator*()
{
	return *get_owned();
}
template <class T>
inline constexpr const T* raw_ptr<T>::operator->() const
{
	return get_owned();
}
template <class T>
inline constexpr const T& raw_ptr<T>::operator*() const
{
	return *get_owned();
}
template <class T>
inline const T& raw_ptr<T>::operator[](aspdetail::size_type index) const
{
	return get_owned()[index];
}
template <class T>
inline T& raw_ptr<T>::operator[](aspdetail::size_type index)
{
	return get_owned()[index];
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
	return make_shared<T, aspdetail::default_allocator>(alloc, std::forward<Args&&>(args)...);
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
	constexpr std::size_t blockSize(shared_ptr<T>::template alloc_size_make_shared<Allocator>());
	constexpr std::size_t blockAlign(alignof(T));

	using block_type = aspdetail::aligned_storage<blockAlign, blockSize>;
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<block_type>;

	rebound_alloc rebound(allocator);
	block_type* typedBlock(nullptr);
	aspdetail::control_block_make_shared<T, Allocator>* controlBlock(nullptr);
	try {
		typedBlock = rebound.allocate(1);
		std::uint8_t* const block((std::uint8_t*)typedBlock);

		const std::uintptr_t mod((std::uintptr_t)block % blockAlign);
		const std::uintptr_t offset(mod ? blockAlign - mod : 0);

		aspdetail::assert_alignment<T>(block);

		controlBlock = new (reinterpret_cast<std::uint8_t*>(block + offset)) aspdetail::control_block_make_shared<T, Allocator>(allocator, (std::uint8_t)offset, std::forward<Args&&>(args)...);
	}
	catch (...) {
		if (controlBlock) {
			(*controlBlock).~control_block_make_shared();
		}
		throw;
		rebound.deallocate(typedBlock, 1);
	}

	aspdetail::compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.myU8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;

	return shared_ptr<T>(storage);
}
};
#pragma warning(pop)