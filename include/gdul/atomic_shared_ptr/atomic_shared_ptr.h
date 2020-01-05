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


#undef max

#pragma warning(push, 3)
// Anonymous union
#pragma warning(disable : 4201)
// Non-class enums 
#pragma warning(disable : 26812)
// Deleter with internal linkage
#pragma warning(disable : 5046)

namespace gdul
{

#pragma region declares
namespace aspdetail
{
typedef std::allocator<std::uint8_t> default_allocator;

constexpr std::uint8_t get_num_bottom_bits() { std::uint8_t i = 0, align(alignof(std::max_align_t)); for (; align; ++i, align >>= 1); return i - 1; }

static constexpr std::uint8_t Cb_Ptr_Bottom_Bits = get_num_bottom_bits();
static constexpr std::uint64_t Owned_Mask = (std::numeric_limits<std::uint64_t>::max() >> 16);
static constexpr std::uint64_t Cb_Mask = (std::numeric_limits<std::uint64_t>::max() >> 16) & ~std::uint64_t((std::uint16_t(1) << Cb_Ptr_Bottom_Bits) - 1);
static constexpr std::uint64_t Versioned_Cb_Mask = (std::numeric_limits<std::uint64_t>::max() >> 8);
static constexpr std::uint64_t Local_Ref_Mask = ~Versioned_Cb_Mask;
static constexpr std::uint16_t Max_Version = (std::uint16_t(std::numeric_limits<std::uint8_t>::max()) << Cb_Ptr_Bottom_Bits | ((std::uint16_t(1) << Cb_Ptr_Bottom_Bits) - 1));

union compressed_storage
{
	constexpr compressed_storage()  noexcept : m_u64(0ULL) {}
	constexpr compressed_storage(std::uint64_t from) noexcept : m_u64(from) {}

	std::uint64_t m_u64;
	std::uint32_t m_u32[2];
	std::uint16_t m_u16[4];
	std::uint8_t m_u8[8];
};

template <class T>
constexpr void assert_alignment(std::uint8_t* block);

constexpr std::uint16_t to_version(compressed_storage);
constexpr compressed_storage set_version(compressed_storage, std::uint16_t);
constexpr compressed_storage inc_version(compressed_storage);

template <class T>
struct disable_deduction;

template <std::uint8_t Align, std::size_t Size>
struct aligned_storage;

template <class T>
constexpr bool is_unbounded_array_v = std::is_array_v<T> & (std::extent_v<T> == 0);

template <class T>
using decay_unbounded_t = std::conditional_t<is_unbounded_array_v<T>, std::remove_pointer_t<std::decay_t<T>>, T>;

template <class T>
class control_block_free_type_base;
template <class T, class U>
class control_block_base;
template <class T>
class control_block_base_count;
template <class T, class Allocator>
class control_block_make_shared;
template <class T, class Allocator>
class control_block_make_unbounded_array;
template <class T, class Allocator>
class control_block_claim;
template <class T, class Allocator, class Deleter>
class control_block_claim_custom_delete;

template <class T>
class ptr_base;

struct memory_orders { std::memory_order m_first, m_second; };

using size_type = std::size_t;

static constexpr std::uint8_t Local_Ref_Index(7);
static constexpr std::uint64_t Local_Ref_Step(1ULL << (Local_Ref_Index * 8));

// Also, the maximum number of concurrently accessing threads
static constexpr std::uint8_t Local_Ref_Fill_Boundary(112);
static constexpr std::uint8_t Default_Local_Refs = std::numeric_limits<std::uint8_t>::max();

enum STORAGE_BYTE : std::uint8_t
{
	STORAGE_BYTE_VERSION_LOWER = 0,
	STORAGE_BYTE_VERSION_UPPER = 6,
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

template <class T, class Allocator, std::enable_if_t<!aspdetail::is_unbounded_array_v<T>> * = nullptr>
constexpr std::size_t alloc_shared_size() noexcept;

template <class T, class Allocator, std::enable_if_t<aspdetail::is_unbounded_array_v<T>> * = nullptr>
constexpr std::size_t alloc_shared_size(std::size_t count) noexcept;

template <class T, class Allocator>
constexpr std::size_t alloc_size_sp_claim() noexcept;

template <class T, class Allocator, class Deleter>
constexpr std::size_t alloc_size_sp_claim_custom_delete() noexcept;

template
<
	class T,
	std::enable_if_t<!aspdetail::is_unbounded_array_v<T>>*,
	class Allocator,
	class ...Args
>
inline shared_ptr<T> allocate_shared(Allocator, Args&& ...);

template
<
	class T,
	std::enable_if_t<aspdetail::is_unbounded_array_v<T>>*,
	class Allocator,
	class ...Args
>
inline shared_ptr<T> allocate_shared(std::size_t, Allocator, Args&& ...);
#pragma endregion

template <class T>
class atomic_shared_ptr
{
public:
	using size_type = aspdetail::size_type;
	using value_type = T;
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	inline constexpr atomic_shared_ptr() noexcept;
	inline constexpr atomic_shared_ptr(std::nullptr_t) noexcept;
	inline constexpr atomic_shared_ptr(std::nullptr_t, std::uint16_t version) noexcept;

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

	inline std::uint16_t get_version() const noexcept;

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

	decayed_type* unsafe_get();
	const decayed_type* unsafe_get() const;

	const std::size_t unsafe_item_count() const;

private:
	template <class PtrType>
	inline bool compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	template <class PtrType>
	inline bool compare_exchange_weak(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline constexpr aspdetail::control_block_free_type_base<T>* get_control_block() noexcept;
	inline constexpr const aspdetail::control_block_free_type_base<T>* get_control_block() const noexcept;

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

	inline constexpr aspdetail::control_block_free_type_base<T>* to_control_block(compressed_storage from) const noexcept;

	using cb_free_type = aspdetail::control_block_free_type_base<T>;
	using cb_count_type = aspdetail::control_block_base_count<T>;

	friend class shared_ptr<T>;
	friend class raw_ptr<T>;
	friend class aspdetail::ptr_base<T>;

	union
	{
		// mutable for altering local refs
		mutable std::atomic<std::uint64_t> m_storage;
		const compressed_storage m_debugView;
	};
};
template <class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr() noexcept
	: m_storage((std::uint64_t(std::numeric_limits<std::uint8_t>::max()) << (aspdetail::STORAGE_BYTE_LOCAL_REF * 8)))
{
}
template<class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr(std::nullptr_t) noexcept
	: atomic_shared_ptr<T>()
{
}
template<class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr(std::nullptr_t, std::uint16_t version) noexcept
	: m_storage(aspdetail::set_version(compressed_storage(), version).m_u64 | (std::uint64_t(std::numeric_limits<std::uint8_t>::max()) << (aspdetail::STORAGE_BYTE_LOCAL_REF * 8)))
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

	const compressed_storage desired_(desired.m_controlBlockStorage.m_u64);

	compressed_storage expected_(m_storage.load(std::memory_order_relaxed));
	expected_.m_u64 &= ~aspdetail::Versioned_Cb_Mask;
	expected_.m_u64 |= expected.m_controlBlockStorage.m_u64 & aspdetail::Versioned_Cb_Mask;

	const aspdetail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	constexpr bool needsCapture(std::is_same<raw_type, shared_ptr<T>>::value);
	const std::uint8_t flags(aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE * needsCapture);

	const std::uint64_t preCompare(expected_.m_u64 & aspdetail::Versioned_Cb_Mask);
	do
	{
		if (compare_exchange_weak_internal(expected_, desired_, static_cast<aspdetail::CAS_FLAG>(flags), orders))
		{

			desired.clear();

			return true;
		}

	} while (preCompare == (expected_.m_u64 & aspdetail::Versioned_Cb_Mask));

	expected = raw_type(expected_);

	return false;
}
template<class T>
template<class PtrType>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	if (desired.get_local_refs() < aspdetail::Local_Ref_Fill_Boundary)
		desired.set_local_refs(std::numeric_limits<std::uint8_t>::max());

	const compressed_storage desired_(desired.m_controlBlockStorage.m_u64);
	compressed_storage expected_(m_storage.load(std::memory_order_relaxed));
	expected_.m_u64 &= ~aspdetail::Versioned_Cb_Mask;
	expected_.m_u64 |= expected.m_controlBlockStorage.m_u64 & aspdetail::Versioned_Cb_Mask;

	const aspdetail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	constexpr bool needsCapture(std::is_same<raw_type, shared_ptr<T>>::value);
	const std::uint8_t flags(aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE * needsCapture);

	const std::uint64_t preCompare(expected_.m_u64 & aspdetail::Versioned_Cb_Mask);

	if (compare_exchange_weak_internal(expected_, desired_, static_cast<aspdetail::CAS_FLAG>(flags), orders))
	{

		desired.clear();

		return true;
	}

	const std::uint64_t postCompare(expected_.m_u64 & aspdetail::Versioned_Cb_Mask);

	// Failed, but not spuriously (other thread loaded, or compare_exchange_weak spurious fail)
	if (preCompare != postCompare)
	{
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

	store_internal(from.m_controlBlockStorage.m_u64, order);
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

	compressed_storage previous(exchange_internal(with.m_controlBlockStorage, aspdetail::CAS_FLAG_STEAL_PREVIOUS, order));
	with.clear();
	return shared_ptr<T>(previous);
}
template<class T>
inline std::uint16_t atomic_shared_ptr<T>::get_version() const noexcept
{
	const compressed_storage storage(m_storage.load(std::memory_order_relaxed));
	return aspdetail::to_version(storage);
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
	const compressed_storage previous(unsafe_exchange_internal(with.m_controlBlockStorage, order));
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
	unsafe_store_internal(from.m_controlBlockStorage, order);
	from.clear();
}
template<class T>
inline raw_ptr<T> atomic_shared_ptr<T>::get_raw_ptr() const noexcept
{
	const compressed_storage storage(m_storage.load(std::memory_order_relaxed));
	return raw_ptr<T>(storage);
}
template<class T>
inline typename atomic_shared_ptr<T>::decayed_type* atomic_shared_ptr<T>::unsafe_get()
{
	aspdetail::control_block_free_type_base<T>* const cb(get_control_block());
	if (cb) {
		return cb->get();
	}
	return nullptr;
}
template<class T>
inline const typename atomic_shared_ptr<T>::decayed_type* atomic_shared_ptr<T>::unsafe_get() const
{
	const aspdetail::control_block_free_type_base<T>* const cb(get_control_block());
	if (cb) {
		return cb->get();
	}
	return nullptr;
}
template<class T>
inline const std::size_t atomic_shared_ptr<T>::unsafe_item_count() const
{
	return raw_ptr<T>(*this)->item_count();
}
template<class T>
inline constexpr const aspdetail::control_block_free_type_base<T>* atomic_shared_ptr<T>::get_control_block() const noexcept
{
	return to_control_block(m_storage.load(std::memory_order_acquire));
}
template<class T>
inline constexpr aspdetail::control_block_free_type_base<T>* atomic_shared_ptr<T>::get_control_block() noexcept
{
	return to_control_block(m_storage.load(std::memory_order_acquire));
}
// cheap hint to see if this object holds a value
template<class T>
inline atomic_shared_ptr<T>::operator bool() const noexcept
{
	return static_cast<bool>(m_storage.load(std::memory_order_relaxed) & aspdetail::Cb_Mask);
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
	return !((m_storage.load(std::memory_order_relaxed) ^ other.m_controlBlockStorage.m_u64) & aspdetail::Versioned_Cb_Mask);
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
inline constexpr aspdetail::control_block_free_type_base<T>* atomic_shared_ptr<T>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<aspdetail::control_block_free_type_base<T>*>(from.m_u64 & aspdetail::Cb_Mask);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak_internal(compressed_storage& expected, compressed_storage desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept
{
	bool result(false);

	const compressed_storage desired_(aspdetail::set_version(desired, aspdetail::to_version(expected) + 1));
	compressed_storage expected_(expected);

	result = m_storage.compare_exchange_weak(expected_.m_u64, desired_.m_u64, orders.m_first, orders.m_second);

	if (result & !(flags & aspdetail::CAS_FLAG_STEAL_PREVIOUS))
	{
		if (aspdetail::control_block_free_type_base<T>* const cb = to_control_block(expected))
		{
			cb->decref(expected.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
		}
	}

	const bool otherInterjection((expected_.m_u64 ^ expected.m_u64) & aspdetail::Versioned_Cb_Mask);

	expected = expected_;

	if (otherInterjection & (flags & aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE))
	{
		expected = copy_internal(std::memory_order_relaxed);
	}

	return result;
}
template<class T>
inline void atomic_shared_ptr<T>::try_fill_local_refs(compressed_storage& expected) const noexcept
{
	if (!(expected.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary))
	{
		return;
	}

	aspdetail::control_block_free_type_base<T>* const cb(to_control_block(expected));

	const compressed_storage initialPtrBlock(expected.m_u64 & aspdetail::Versioned_Cb_Mask);

	do
	{
		const std::uint8_t localRefs(expected.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
		const std::uint8_t newRefs(std::numeric_limits<std::uint8_t>::max() - localRefs);

		compressed_storage desired(expected);
		desired.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();

		// Accept 'null' refs, to be able to avoid using compare_exchange_strong during copy_internal
		if (cb)
		{
			cb->incref(newRefs);
		}

		if (m_storage.compare_exchange_weak(expected.m_u64, desired.m_u64, std::memory_order_relaxed))
		{
			return;
		}

		if (cb)
		{
			cb->decref(newRefs);
		}

	} while (
		(expected.m_u64 & aspdetail::Versioned_Cb_Mask) == initialPtrBlock.m_u64 &&
		expected.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary);
}
template <class T>
inline union aspdetail::compressed_storage atomic_shared_ptr<T>::copy_internal(std::memory_order order) const noexcept
{
	compressed_storage initial(m_storage.fetch_sub(aspdetail::Local_Ref_Step, std::memory_order_relaxed));
	initial.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] -= 1;

	compressed_storage expected(initial);
	try_fill_local_refs(expected);

	initial.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 1;

	return initial;
}
template <class T>
inline union aspdetail::compressed_storage atomic_shared_ptr<T>::unsafe_copy_internal(std::memory_order order) const
{
	compressed_storage storage(m_storage.load(std::memory_order_relaxed));

	compressed_storage newStorage(storage);
	newStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] -= 1;
	m_storage.store(newStorage.m_u64, std::memory_order_relaxed);

	storage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 1;

	unsafe_fill_local_refs();

	std::atomic_thread_fence(order);

	return storage;
}
template<class T>
inline union aspdetail::compressed_storage atomic_shared_ptr<T>::unsafe_exchange_internal(compressed_storage with, std::memory_order order)
{
	const compressed_storage old(m_storage.load(std::memory_order_relaxed));
	const compressed_storage replacement(aspdetail::set_version(with, aspdetail::to_version(old) + 1));

	m_storage.store(replacement.m_u64, std::memory_order_relaxed);

	unsafe_fill_local_refs();

	std::atomic_thread_fence(order);

	return old;
}
template <class T>
inline void atomic_shared_ptr<T>::unsafe_store_internal(compressed_storage from, std::memory_order order)
{
	const compressed_storage previous(m_storage.load(std::memory_order_relaxed));
	const compressed_storage next(aspdetail::set_version(from, aspdetail::to_version(previous) + 1));

	m_storage.store(next.m_u64, std::memory_order_relaxed);

	aspdetail::control_block_free_type_base<T>* const prevCb(to_control_block(previous));
	if (prevCb)
	{
		prevCb->decref(previous.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
	}

	unsafe_fill_local_refs();

	std::atomic_thread_fence(order);
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_fill_local_refs() const noexcept
{
	const compressed_storage current(m_storage.load(std::memory_order_relaxed));
	aspdetail::control_block_free_type_base<T>* const cb(to_control_block(current));

	if (current.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] < aspdetail::Local_Ref_Fill_Boundary)
	{
		const std::uint8_t newRefs(std::numeric_limits<std::uint8_t>::max() - current.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF]);

		// Accept 'null' refs, to be able to avoid using compare_exchange_strong during copy_internal
		if (cb)
		{
			cb->incref(newRefs);
		}

		compressed_storage newStorage(current);
		newStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = std::numeric_limits<std::uint8_t>::max();

		m_storage.store(newStorage.m_u64, std::memory_order_relaxed);
	}
}
template<class T>
inline union aspdetail::compressed_storage atomic_shared_ptr<T>::exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags, std::memory_order order) noexcept
{
	compressed_storage expected(m_storage.load(std::memory_order_relaxed));
	while (!compare_exchange_weak_internal(expected, to, flags, { order, std::memory_order_relaxed }));
	return expected;
}
namespace aspdetail
{
template <class T>
class control_block_free_type_base
{
public:
	control_block_free_type_base(compressed_storage storage) noexcept;

	using size_type = aspdetail::size_type;
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	virtual decayed_type* get() noexcept = 0;
	virtual const decayed_type* get() const noexcept = 0;

	size_type use_count() const noexcept;

	std::size_t item_count() const noexcept;

	void incref(size_type count) noexcept;
	void decref(size_type count) noexcept;

protected:
	virtual ~control_block_free_type_base() = default;
	virtual void destroy() = 0;

	const compressed_storage m_ptrStorage;
private:

	std::atomic<size_type> m_useCount;
};
template<class T>
inline control_block_free_type_base<T>::control_block_free_type_base(compressed_storage storage) noexcept
	: m_ptrStorage(storage)
	, m_useCount(Default_Local_Refs)
{}
template<class T>
inline void control_block_free_type_base<T>::incref(size_type count) noexcept
{
	m_useCount.fetch_add(count, std::memory_order_relaxed);
}
template<class T>
inline void control_block_free_type_base<T>::decref(size_type count) noexcept
{
	if (!(m_useCount.fetch_sub(count, std::memory_order_acq_rel) - count)) {
		destroy();
	}
}
template <class T>
inline typename control_block_free_type_base<T>::size_type control_block_free_type_base<T>::use_count() const noexcept
{
	return m_useCount.load(std::memory_order_relaxed);
}
template<class T>
inline std::size_t control_block_free_type_base<T>::item_count() const noexcept
{
	return !is_unbounded_array_v<T> ? sizeof(decayed_type) / sizeof(std::remove_all_extents_t<T>) : ((control_block_base_count<T>*)this)->item_count();
}
template <class T, class U = T>
class control_block_base : public control_block_free_type_base<U>
{
public:
	using conv_type = aspdetail::decay_unbounded_t<U>;
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	inline conv_type* get() noexcept override final;
	inline const conv_type* get() const noexcept override final;

protected:
	control_block_base(decayed_type* object, std::uint8_t blockOffset = 0) noexcept;
	std::uint8_t block_offset() const;
};
template <class T, class U>
inline typename control_block_base<T, U>::conv_type* control_block_base<T, U>::get() noexcept
{
	return static_cast<conv_type*>((decayed_type*)(this->m_ptrStorage.m_u64 & Owned_Mask));
}
template<class T, class U>
inline const typename control_block_base<T, U>::conv_type* control_block_base<T, U>::get() const noexcept
{
	return static_cast<const conv_type*>((decayed_type*)(this->m_ptrStorage.m_u64 & Owned_Mask));
}
template<class T, class U>
inline std::uint8_t control_block_base<T, U>::block_offset() const
{
	return std::uint8_t(this->m_ptrStorage.m_u8[6]);
}
template<class T, class U>
control_block_base<T, U>::control_block_base(decayed_type* object, std::uint8_t blockOffset) noexcept
	: control_block_free_type_base<U>(compressed_storage(std::uint64_t(object) | (std::uint64_t(blockOffset) << 48) | (std::uint64_t(is_unbounded_array_v<T>) << 56)))
{
}
template <class T>
class control_block_base_count : public control_block_base<T>
{
public:
	using size_type = aspdetail::size_type;
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	control_block_base_count(decayed_type* object, std::size_t count)  noexcept;

	std::size_t item_count() const noexcept;

private:
	const std::size_t m_count;
};

template<class T>
inline control_block_base_count<T>::control_block_base_count(decayed_type* object, std::size_t count) noexcept
	: control_block_base<T>(object)
	, m_count(count)
{}
template <class T>
inline std::size_t control_block_base_count<T>::item_count() const  noexcept
{
	return m_count;
}
template <class T, class Allocator>
class control_block_make_shared : public control_block_base<T>
{
public:
	template <class ...Args, class U = T, std::enable_if_t<std::is_array<U>::value> * = nullptr>
	control_block_make_shared(Allocator alloc, std::uint8_t blockOffset, Args&& ...args);
	template <class ...Args, class U = T, std::enable_if_t<!std::is_array<U>::value> * = nullptr>
	control_block_make_shared(Allocator alloc, std::uint8_t blockOffset, Args&& ...args);

	void destroy() noexcept override;
private:
	T m_owned;
	Allocator m_allocator;
};
template<class T, class Allocator>
template <class ...Args, class U, std::enable_if_t<std::is_array<U>::value>*>
inline control_block_make_shared<T, Allocator>::control_block_make_shared(Allocator alloc, std::uint8_t blockOffset, Args&& ...args)
	: control_block_base<T>(&m_owned, blockOffset)
	, m_owned{ std::forward<Args>(args)... }
	, m_allocator(alloc)
{}
template<class T, class Allocator>
template <class ...Args, class U, std::enable_if_t<!std::is_array<U>::value>*>
inline control_block_make_shared<T, Allocator>::control_block_make_shared(Allocator alloc, std::uint8_t blockOffset, Args&& ...args)
	: control_block_base<T>(&m_owned, blockOffset)
	, m_owned(std::forward<Args>(args)...)
	, m_allocator(alloc)
{}
template<class T, class Allocator>
inline void control_block_make_shared<T, Allocator>::destroy() noexcept
{
	Allocator alloc(this->m_allocator);

	(*this).~control_block_make_shared<T, Allocator>();

	constexpr std::size_t blockSize(gdul::alloc_shared_size<T, Allocator>());
	constexpr std::size_t blockAlign(alignof(T));

	using block_type = aspdetail::aligned_storage<blockAlign, blockSize>;
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<block_type>;

	rebound_alloc rebound(alloc);

	uint8_t* beginPtr((std::uint8_t*)this - this->block_offset());
	block_type* const typedBlock((block_type*)beginPtr);

	rebound.deallocate(typedBlock, 1);
}
template <class T, class Allocator>
class control_block_make_unbounded_array : public control_block_base_count<T>
{
public:
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	template <class ...Args>
	control_block_make_unbounded_array(decayed_type* arr, std::size_t count, Allocator alloc) noexcept;

	void destroy() noexcept override;

private:
	Allocator m_allocator;
};
template<class T, class Allocator>
template <class ...Args>
inline control_block_make_unbounded_array<T, Allocator>::control_block_make_unbounded_array(decayed_type* arr, std::size_t count, Allocator alloc) noexcept
	: control_block_base_count<T>(arr, count)
	, m_allocator(alloc)
{
}
template<class T, class Allocator>
inline void control_block_make_unbounded_array<T, Allocator>::destroy() noexcept
{
	Allocator alloc(this->m_allocator);

	decayed_type* const ptrBase(this->get());

	const std::size_t nItems(this->item_count());

	for (std::size_t i = 0; i < nItems; ++i)
	{
		ptrBase[i].~decayed_type();
	}

	(*this).~control_block_make_unbounded_array<T, Allocator>();

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(alloc);

	uint8_t* const block((uint8_t*)this);

	rebound.deallocate(block, alloc_shared_size<T, Allocator>(nItems));
}
template <class T, class Allocator>
class control_block_claim : public control_block_base<T>
{
public:
	control_block_claim(T* obj, Allocator alloc) noexcept;
	void destroy() noexcept override;

private:
	Allocator m_allocator;
};
template<class T, class Allocator>
inline control_block_claim<T, Allocator>::control_block_claim(T* obj, Allocator alloc) noexcept
	: control_block_base<T>(obj)
{
}
template<class T, class Allocator>
inline void control_block_claim<T, Allocator>::destroy() noexcept
{
	Allocator alloc(this->m_allocator);

	delete this->get();
	(*this).~control_block_claim<T, Allocator>();

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(alloc);

	rebound.deallocate(reinterpret_cast<std::uint8_t*>(this), gdul::alloc_size_sp_claim<T, Allocator>());
}
template <class T, class Allocator, class Deleter>
class control_block_claim_custom_delete : public control_block_base<T>
{
public:
	control_block_claim_custom_delete(T* obj, Allocator alloc, Deleter&& deleter) noexcept;
	void destroy() noexcept override;

private:
	Deleter m_deleter;
	Allocator m_allocator;
};
template<class T, class Allocator, class Deleter>
inline control_block_claim_custom_delete<T, Allocator, Deleter>::control_block_claim_custom_delete(T* obj, Allocator alloc, Deleter&& deleter) noexcept
	: control_block_base<T>(obj)
	, m_deleter(std::forward<Deleter&&>(deleter))
	, m_allocator(alloc)
{
}
template<class T, class Allocator, class Deleter>
inline void control_block_claim_custom_delete<T, Allocator, Deleter>::destroy() noexcept
{
	Allocator alloc(this->m_allocator);

	T* ptrAddr(this->get());
	m_deleter(ptrAddr, alloc);
	(*this).~control_block_claim_custom_delete<T, Allocator, Deleter>();

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(alloc);

	rebound.deallocate((std::uint8_t*)this, gdul::alloc_size_sp_claim_custom_delete<T, Allocator, Deleter>());
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
template <class T>
struct type_diff_copy_helper
{
	shared_ptr<T> construct_from(compressed_storage storage) {
		return shared_ptr<T>(storage);
	};
};
constexpr std::uint16_t to_version(compressed_storage from)
{
	constexpr std::uint8_t bottomBits((std::uint8_t(1) << Cb_Ptr_Bottom_Bits) - 1);
	const std::uint8_t lower(from.m_u8[STORAGE_BYTE_VERSION_LOWER] & bottomBits);
	const std::uint8_t upper(from.m_u8[STORAGE_BYTE_VERSION_UPPER]);

	const std::uint16_t version((std::uint16_t(upper) << Cb_Ptr_Bottom_Bits) | std::uint16_t(lower));

	return version;
}
constexpr compressed_storage set_version(compressed_storage storage, std::uint16_t to)
{
	constexpr std::uint8_t bottomBits((std::uint8_t(1) << Cb_Ptr_Bottom_Bits) - 1);

	const std::uint8_t lower(((std::uint8_t)to) & bottomBits);
	const std::uint8_t upper((std::uint8_t) (to >> Cb_Ptr_Bottom_Bits));

	compressed_storage updated(storage);
	updated.m_u8[STORAGE_BYTE_VERSION_LOWER] &= ~bottomBits;
	updated.m_u8[STORAGE_BYTE_VERSION_LOWER] |= lower;
	updated.m_u8[STORAGE_BYTE_VERSION_UPPER] = upper;

	return updated;
}
constexpr compressed_storage inc_version(compressed_storage storage)
{
	return set_version(storage, to_version(storage) + 1);
}
template<class T>
constexpr void assert_alignment(std::uint8_t* block)
{

#if 201700 < __cplusplus || _HAS_CXX17
	if ((std::uintptr_t)block % alignof(T) != 0)
	{
		throw std::runtime_error("conforming with C++17 alloc_shared expects alignof(T) allocates. Minimally alignof(std::max_align_t)");
	}
#else
	static_assert(!(std::numeric_limits<std::uint8_t>::max() < alignof(T)), "alloc_shared supports only supports up to std::numeric_limits<std::uint8_t>::max() byte aligned types");

	if ((std::uintptr_t)block % alignof(std::max_align_t) != 0)
	{
		throw std::runtime_error("alloc_shared expects at least alignof(std::max_align_t) allocates");
	}
#endif
}
template <class T>
class ptr_base
{
public:
	using size_type = aspdetail::size_type;
	using value_type = T;
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	inline constexpr operator bool() const noexcept;

	inline constexpr bool operator==(const ptr_base<T>& other) const noexcept;
	inline constexpr bool operator!=(const ptr_base<T>& other) const noexcept;

	inline bool operator==(const atomic_shared_ptr<T>& other) const noexcept;
	inline bool operator!=(const atomic_shared_ptr<T>& other) const noexcept;

	inline constexpr std::uint16_t get_version() const noexcept;

	inline size_type use_count() const noexcept;

	inline void swap(ptr_base<T>& other) noexcept;
protected:
	inline constexpr const control_block_free_type_base<T>* get_control_block() const noexcept;
	inline constexpr control_block_free_type_base<T>* get_control_block() noexcept;

	inline constexpr ptr_base() noexcept;
	inline constexpr ptr_base(compressed_storage from) noexcept;

	inline void clear() noexcept;

	constexpr control_block_free_type_base<T>* to_control_block(compressed_storage from) noexcept;
	constexpr decayed_type* to_object(compressed_storage from) noexcept;

	constexpr const control_block_free_type_base<T>* to_control_block(compressed_storage from) const noexcept;
	constexpr const decayed_type* to_object(compressed_storage from) const noexcept;

	using cb_free_type = control_block_free_type_base<T>;
	using cb_count_type = control_block_base_count<T>;

	friend class atomic_shared_ptr<T>;

	// mutable for altering local refs
	mutable compressed_storage m_controlBlockStorage;
};
template <class T>
inline constexpr ptr_base<T>::ptr_base() noexcept
	: m_controlBlockStorage()
{
}
template <class T>
inline constexpr ptr_base<T>::ptr_base(compressed_storage from) noexcept
	: ptr_base<T>()
{
	m_controlBlockStorage = from;
	m_controlBlockStorage.m_u8[STORAGE_BYTE_LOCAL_REF] *= operator bool();
}
template<class T>
inline void ptr_base<T>::clear() noexcept
{
	m_controlBlockStorage = compressed_storage();
}
template <class T>
inline constexpr ptr_base<T>::operator bool() const noexcept
{
	return m_controlBlockStorage.m_u64 & aspdetail::Cb_Mask;
}
template <class T>
inline constexpr bool ptr_base<T>::operator==(const ptr_base<T>& other) const noexcept
{
	return !((m_controlBlockStorage.m_u64 ^ other.m_controlBlockStorage.m_u64) & aspdetail::Versioned_Cb_Mask);
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
	if (!operator bool())
	{
		return 0;
	}

	return get_control_block()->use_count();
}

template<class T>
inline void ptr_base<T>::swap(ptr_base<T>& other) noexcept
{
	std::swap(this->m_controlBlockStorage, other.m_controlBlockStorage);
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
inline constexpr control_block_free_type_base<T>* ptr_base<T>::to_control_block(compressed_storage from) noexcept
{
	return reinterpret_cast<control_block_free_type_base<T>*>(from.m_u64 & Cb_Mask);
}
template <class T>
inline constexpr const control_block_free_type_base<T>* ptr_base<T>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<const control_block_free_type_base<T>*>(from.m_u64 & Cb_Mask);
}
template <class T>
inline constexpr typename ptr_base<T>::decayed_type* ptr_base<T>::to_object(compressed_storage from) noexcept
{
	control_block_free_type_base<T>* const cb(to_control_block(from));
	if (cb){
		return cb->get();
	}
	return nullptr;
}
template <class T>
inline constexpr const typename ptr_base<T>::decayed_type* ptr_base<T>::to_object(compressed_storage from) const noexcept
{
	const control_block_free_type_base<T>* const cb(to_control_block(from));
	if (cb){
		return cb->get();
	}
	return nullptr;
}
template <class T>
inline constexpr const control_block_free_type_base<T>* ptr_base<T>::get_control_block() const noexcept
{
	return to_control_block(m_controlBlockStorage);
}
template <class T>
inline constexpr control_block_free_type_base<T>* ptr_base<T>::get_control_block() noexcept
{
	return to_control_block(m_controlBlockStorage);
}
template<class T>
inline constexpr std::uint16_t ptr_base<T>::get_version() const noexcept
{
	return to_version(m_controlBlockStorage);
}
};
template <class T>
class shared_ptr : public aspdetail::ptr_base<T>
{
public:
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	inline constexpr shared_ptr() noexcept;

	inline explicit constexpr shared_ptr(std::nullptr_t) noexcept;
	inline explicit constexpr shared_ptr(std::nullptr_t, std::uint16_t version) noexcept;

	using aspdetail::ptr_base<T>::ptr_base;

	inline shared_ptr(const shared_ptr<T>& other) noexcept;

	template <class U, std::enable_if_t<std::is_convertible_v<U*, T*>> * = nullptr>
	inline shared_ptr(shared_ptr<U>&& other) noexcept;

	shared_ptr<T>& operator=(const shared_ptr<T>& other) noexcept;

	template <class U, std::enable_if_t<std::is_convertible_v<U*, T*>> * = nullptr>
	shared_ptr<T>& operator=(shared_ptr<U>&& other) noexcept;

	template <class U, std::enable_if_t<std::is_convertible_v<T*, U*>> * = nullptr>
	inline operator const shared_ptr<U>&() const noexcept;
	template <class U, std::enable_if_t<std::is_convertible_v<T*, U*>> * = nullptr>
	inline operator shared_ptr<U>&() noexcept;
	template <class U, std::enable_if_t<std::is_convertible_v<T*, U*>> * = nullptr>
	inline explicit operator shared_ptr<U>&&() noexcept;

	template <class U, std::enable_if_t<std::is_base_of_v<T, U>> * = nullptr>
	inline explicit operator const shared_ptr<U>&() const noexcept;

	template <class U, std::enable_if_t<std::is_base_of_v<T, U>> * = nullptr>
	inline explicit operator shared_ptr<U>&() noexcept;

	template <class U, std::enable_if_t<std::is_base_of_v<T, U>> * = nullptr>
	inline explicit operator shared_ptr<U>&&() noexcept;

	inline explicit shared_ptr(T* object);
	template <class Allocator>
	inline explicit shared_ptr(T* object, Allocator allocator);
	template <class Allocator, class Deleter>
	inline explicit shared_ptr(T* object, Allocator allocator, Deleter&& deleter);

	~shared_ptr() noexcept;

	inline constexpr explicit operator decayed_type* () noexcept;
	inline constexpr explicit operator const decayed_type* () const noexcept;

	inline constexpr const decayed_type* get() const noexcept;
	inline constexpr decayed_type* get() noexcept;

	inline std::size_t item_count() const noexcept;

	inline constexpr decayed_type* operator->();
	inline constexpr decayed_type& operator*();

	inline constexpr const decayed_type* operator->() const;
	inline constexpr const decayed_type& operator*() const;

	inline const decayed_type& operator[](aspdetail::size_type index) const;
	inline decayed_type& operator[](aspdetail::size_type index);

	inline constexpr raw_ptr<T> get_raw_ptr() const noexcept;
	inline void swap(shared_ptr<T>& other) noexcept;

private:
	inline constexpr std::uint8_t get_local_refs() const noexcept;
	inline void set_local_refs(std::uint8_t target) noexcept;

	constexpr void clear() noexcept;

	using compressed_storage = aspdetail::compressed_storage;
	inline constexpr shared_ptr(union aspdetail::compressed_storage from) noexcept;

	template<class Deleter, class Allocator>
	inline compressed_storage create_control_block(T* object, Allocator allocator, Deleter&& deleter);
	template <class Allocator>
	inline compressed_storage create_control_block(T* object, Allocator allocator);

	friend class raw_ptr<T>;
	friend class atomic_shared_ptr<T>;
	friend struct aspdetail::type_diff_copy_helper<T>;

	template
		<
		class U,
		std::enable_if_t<!aspdetail::is_unbounded_array_v<U>>*,
		class Allocator,
		class ...Args
		>
		friend shared_ptr<U> allocate_shared(Allocator, Args&& ...);

	template
		<
		class U,
		std::enable_if_t<aspdetail::is_unbounded_array_v<U>>*,
		class Allocator,
		class ...Args
		>
		friend shared_ptr<U> allocate_shared(std::size_t, Allocator, Args&& ...);

	decayed_type* m_ptr;
};
template <class T>
inline constexpr shared_ptr<T>::shared_ptr() noexcept
	: aspdetail::ptr_base<T>()
	, m_ptr(nullptr)
{}
template <class T>
inline constexpr shared_ptr<T>::shared_ptr(std::nullptr_t) noexcept
	: aspdetail::ptr_base<T>(compressed_storage())
	, m_ptr(nullptr)
{}
template <class T>
inline constexpr shared_ptr<T>::shared_ptr(std::nullptr_t, std::uint16_t version) noexcept
	: aspdetail::ptr_base<T>(set_version(compressed_storage(), version))
	, m_ptr(nullptr)
{}
template<class T>
inline shared_ptr<T>::~shared_ptr() noexcept
{
	aspdetail::control_block_free_type_base<T>* const cb(this->get_control_block());
	if (cb)
	{
		cb->decref(this->m_controlBlockStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF]);
	}
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible_v<U*, T*>>*>
inline shared_ptr<T>::shared_ptr(shared_ptr<U>&& other) noexcept
	: shared_ptr<T>()
{
	memcpy(this, &other, sizeof(shared_ptr<T>));
	memset(&other, 0, sizeof(shared_ptr<U>));

	this->m_ptr = this->to_object(this->m_controlBlockStorage);
}
template<class T>
inline shared_ptr<T>::shared_ptr(const shared_ptr<T>& other) noexcept
	: shared_ptr<T>()
{
	compressed_storage copy(other.m_controlBlockStorage);

	if (aspdetail::control_block_free_type_base<T>* const cb = this->to_control_block(copy))
	{
		copy.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;
		cb->incref(aspdetail::Default_Local_Refs);
	}

	this->m_controlBlockStorage = copy;
	this->m_ptr = this->to_object(this->m_controlBlockStorage);
}
template <class T>
inline shared_ptr<T>::shared_ptr(T* object)
	: shared_ptr<T>()
{
	aspdetail::default_allocator alloc;
	this->m_controlBlockStorage = create_control_block(object, alloc);
	this->m_ptr = this->to_object(this->m_controlBlockStorage);
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible_v<T*, U*>>*>
inline shared_ptr<T>::operator const shared_ptr<U>&() const noexcept
{
	return *reinterpret_cast<const shared_ptr<U>*>(this);
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible_v<T*, U*>>*>
inline shared_ptr<T>::operator shared_ptr<U>&() noexcept
{
	return *reinterpret_cast<shared_ptr<U>*>(this);
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible_v<T*, U*>>*>
inline shared_ptr<T>::operator shared_ptr<U>&&() noexcept
{
	return std::move(*reinterpret_cast<shared_ptr<U>*>(this));
}
template <class T>
template <class U, std::enable_if_t<std::is_base_of_v<T, U>>*>
inline shared_ptr<T>::operator const shared_ptr<U>&() const noexcept 
{
	return *reinterpret_cast<const shared_ptr<U>*>(this);
}
template <class T>
template <class U, std::enable_if_t<std::is_base_of_v<T, U>>*>
inline shared_ptr<T>::operator shared_ptr<U>&() noexcept
{
	return *reinterpret_cast<shared_ptr<U>*>(this);
}
template <class T>
template <class U, std::enable_if_t<std::is_base_of_v<T, U>>*>
inline shared_ptr<T>::operator shared_ptr<U>&&() noexcept
{
	return std::move(*reinterpret_cast<shared_ptr<U>*>(this));
}
template <class T>
template <class Allocator>
inline shared_ptr<T>::shared_ptr(T* object, Allocator allocator)
	: shared_ptr<T>()
{
	this->m_controlBlockStorage = create_control_block(object, allocator);
	this->m_ptr = this->to_object(this->m_controlBlockStorage);
}
// The Deleter callable has signature void(T* obj, Allocator alloc)
template<class T>
template<class Allocator, class Deleter>
inline shared_ptr<T>::shared_ptr(T* object, Allocator allocator, Deleter&& deleter)
	: shared_ptr<T>()
{
	this->m_controlBlockStorage = create_control_block(object, allocator, std::forward<Deleter&&>(deleter));
	this->m_ptr = this->to_object(this->m_controlBlockStorage);
}
template<class T>
inline constexpr void shared_ptr<T>::clear() noexcept
{
	m_ptr = nullptr;
	aspdetail::ptr_base<T>::clear();
}
template<class T>
inline void shared_ptr<T>::set_local_refs(std::uint8_t target) noexcept
{
	if (aspdetail::control_block_free_type_base<T>* const cb = this->get_control_block())
	{
		const uint8_t localRefs(this->m_controlBlockStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF]);

		if (localRefs < target)
		{
			cb->incref(target - localRefs);
		}
		else if (target < localRefs)
		{
			cb->decref(localRefs - target);
		}

		this->m_controlBlockStorage.m_u64 *= (bool)target;
		m_ptr = (decayed_type*)((uint64_t)m_ptr * (bool)target);
	}

	this->m_controlBlockStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = target;
}
template <class T>
inline constexpr std::uint8_t shared_ptr<T>::get_local_refs() const noexcept
{
	return this->m_controlBlockStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF];
}
template<class T>
inline constexpr shared_ptr<T>::shared_ptr(union aspdetail::compressed_storage from) noexcept
	: aspdetail::ptr_base<T>(from)
	, m_ptr(this->to_object(from))
{
}
template<class T>
inline constexpr raw_ptr<T> shared_ptr<T>::get_raw_ptr() const noexcept
{
	return raw_ptr<T>(this->m_controlBlockStorage);
}
template <class T>
inline constexpr shared_ptr<T>::operator typename shared_ptr<T>::decayed_type* () noexcept
{
	return get();
}
template <class T>
inline constexpr shared_ptr<T>::operator const typename shared_ptr<T>::decayed_type* () const noexcept
{
	return get();
}
template <class T>
inline constexpr typename shared_ptr<T>::decayed_type* shared_ptr<T>::operator->()
{
	return get();
}
template <class T>
inline constexpr typename shared_ptr<T>::decayed_type& shared_ptr<T>::operator*()
{
	return *get();
}
template <class T>
inline constexpr const typename shared_ptr<T>::decayed_type* shared_ptr<T>::operator->() const
{
	return get();
}
template <class T>
inline constexpr const typename shared_ptr<T>::decayed_type& shared_ptr<T>::operator*() const
{
	return *get();
}
template <class T>
inline const typename shared_ptr<T>::decayed_type& shared_ptr<T>::operator[](aspdetail::size_type index) const
{
	return get()[index];
}
template <class T>
inline typename shared_ptr<T>::decayed_type& shared_ptr<T>::operator[](aspdetail::size_type index)
{
	return get()[index];
}
template <class T>
inline constexpr const typename shared_ptr<T>::decayed_type* shared_ptr<T>::get() const noexcept
{
	return m_ptr;
}
template <class T>
inline constexpr typename shared_ptr<T>::decayed_type* shared_ptr<T>::get() noexcept
{
	return m_ptr;
}
template<class T>
inline std::size_t shared_ptr<T>::item_count() const noexcept
{
	if (!this->operator bool())
	{
		return 0;
	}

	return this->get_control_block()->item_count();
}
template <class T>
template<class Deleter, class Allocator>
inline union aspdetail::compressed_storage shared_ptr<T>::create_control_block(T* object, Allocator allocator, Deleter&& deleter)
{
	static_assert(!(8 < alignof(Deleter)), "No support for custom aligned deleter objects");

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(allocator);

	aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>* controlBlock(nullptr);

	constexpr std::size_t blockSize(alloc_size_sp_claim_custom_delete<T, Allocator, Deleter>());

	void* block(nullptr);

	try
	{
		block = rebound.allocate(blockSize);

		if ((std::uintptr_t)block % alignof(std::max_align_t) != 0)
		{
			throw std::runtime_error("alloc_shared expects at least alignof(max_align_t) allocates");
		}

		controlBlock = new (block) aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>(object, allocator, std::forward<Deleter&&>(deleter));
	}
	catch (...)
	{
		rebound.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		deleter(object, allocator);
		throw;
	}
	compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;

	return storage;
}
template<class T>
template <class Allocator>
inline union aspdetail::compressed_storage shared_ptr<T>::create_control_block(T* object, Allocator allocator)
{
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(allocator);

	aspdetail::control_block_claim<T, Allocator>* controlBlock(nullptr);

	constexpr std::size_t blockSize(alloc_size_sp_claim<T, Allocator>());

	void* block(nullptr);

	try
	{
		block = rebound.allocate(blockSize);

		if ((std::uintptr_t)block % alignof(std::max_align_t) != 0)
		{
			throw std::runtime_error("alloc_shared expects at least alignof(max_align_t) allocates");
		}

		controlBlock = new (block) aspdetail::control_block_claim<T, Allocator>(object, rebound);
	}
	catch (...)
	{
		rebound.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		delete object;
		throw;
	}

	compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;

	return storage;
}
template<class T>
inline shared_ptr<T>& shared_ptr<T>::operator=(const shared_ptr<T>& other) noexcept
{
	shared_ptr<T>(other).swap(*this);

	return *this;
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible_v<U*, T*>>*>
inline shared_ptr<T>& shared_ptr<T>::operator=(shared_ptr<U>&& other) noexcept
{
	shared_ptr<T>(std::move(other)).swap(*this);

	return *this;
}

template<class T>
inline void shared_ptr<T>::swap(shared_ptr<T>& other) noexcept
{
	aspdetail::ptr_base<T>::swap(other);
	std::swap(m_ptr, other.m_ptr);
}

// raw_ptr does not share in ownership of the object
template <class T>
class raw_ptr : public aspdetail::ptr_base<T>
{
public:
	inline constexpr raw_ptr() noexcept;

	inline explicit constexpr raw_ptr(std::nullptr_t) noexcept;
	inline explicit constexpr raw_ptr(std::nullptr_t, std::uint16_t version) noexcept;

	using aspdetail::ptr_base<T>::ptr_base;
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	constexpr raw_ptr(raw_ptr<T>&& other) noexcept;
	constexpr raw_ptr(const raw_ptr<T>& other) noexcept;

	explicit constexpr raw_ptr(const shared_ptr<T>& from) noexcept;

	explicit raw_ptr(const atomic_shared_ptr<T>& from) noexcept;

	inline constexpr explicit operator decayed_type* () noexcept;
	inline constexpr explicit operator const decayed_type* () const noexcept;

	inline constexpr const decayed_type* get() const noexcept;
	inline constexpr decayed_type* get() noexcept;

	inline std::size_t item_count() const;

	inline constexpr decayed_type* operator->();
	inline constexpr decayed_type& operator*();

	inline constexpr const decayed_type* operator->() const;
	inline constexpr const decayed_type& operator*() const;

	inline const decayed_type& operator[](aspdetail::size_type index) const;
	inline decayed_type& operator[](aspdetail::size_type index);

	inline void swap(raw_ptr<T>& other) noexcept;

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
inline constexpr raw_ptr<T>::raw_ptr(std::nullptr_t) noexcept
	: aspdetail::ptr_base<T>(compressed_storage())
{}
template<class T>
inline constexpr raw_ptr<T>::raw_ptr(std::nullptr_t, std::uint16_t version) noexcept
	: aspdetail::ptr_base<T>(set_version(compressed_storage(), version))
{}
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
	: aspdetail::ptr_base<T>(from.m_controlBlockStorage)
{
	this->m_controlBlockStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 0;
}
template<class T>
inline raw_ptr<T>::raw_ptr(const atomic_shared_ptr<T>& from) noexcept
	: aspdetail::ptr_base<T>(from.m_storage.load(std::memory_order_relaxed))
{
	this->m_controlBlockStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 0;
}
template<class T>
inline constexpr raw_ptr<T>& raw_ptr<T>::operator=(const raw_ptr<T>& other)  noexcept
{
	this->m_controlBlockStorage = other.m_controlBlockStorage;

	return *this;
}
template<class T>
inline constexpr raw_ptr<T>& raw_ptr<T>::operator=(raw_ptr<T>&& other) noexcept
{
	std::swap(this->m_controlBlockStorage, other.m_controlBlockStorage);
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
inline constexpr raw_ptr<T>::operator typename raw_ptr<T>::decayed_type* () noexcept
{
	return get();
}
template <class T>
inline constexpr raw_ptr<T>::operator const typename raw_ptr<T>::decayed_type* () const noexcept
{
	return get();
}
template <class T>
inline constexpr const typename raw_ptr<T>::decayed_type* raw_ptr<T>::get() const noexcept
{
	return this->to_object(this->m_controlBlockStorage);
}
template <class T>
inline constexpr typename raw_ptr<T>::decayed_type* raw_ptr<T>::get() noexcept
{
	return this->to_object(this->m_controlBlockStorage);
}
template<class T>
inline std::size_t raw_ptr<T>::item_count() const
{
	if (!this->operator bool())
	{
		return 0;
	}

	return this->get_control_block()->item_count();
}
template <class T>
inline constexpr typename raw_ptr<T>::decayed_type* raw_ptr<T>::operator->()
{
	return get();
}
template <class T>
inline constexpr typename raw_ptr<T>::decayed_type& raw_ptr<T>::operator*()
{
	return *get();
}
template <class T>
inline constexpr const typename raw_ptr<T>::decayed_type* raw_ptr<T>::operator->() const
{
	return get();
}
template <class T>
inline constexpr const typename raw_ptr<T>::decayed_type& raw_ptr<T>::operator*() const
{
	return *get();
}
template <class T>
inline const typename raw_ptr<T>::decayed_type& raw_ptr<T>::operator[](aspdetail::size_type index) const
{
	return get()[index];
}
template <class T>
inline typename raw_ptr<T>::decayed_type& raw_ptr<T>::operator[](aspdetail::size_type index)
{
	return get()[index];
}
template<class T>
inline void raw_ptr<T>::swap(raw_ptr<T>& other) noexcept
{
	this->swap(other);
}
template<class T>
inline constexpr raw_ptr<T>::raw_ptr(compressed_storage from) noexcept
	: aspdetail::ptr_base<T>(from)
{
	this->m_controlBlockStorage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = 0;
}
#if 201700 < __cplusplus || _HAS_CXX17
// The amount of memory requested from the allocator when calling
// alloc_shared (object or statically sized array)
template <class T, class Allocator, std::enable_if_t<!aspdetail::is_unbounded_array_v<T>>*>
inline constexpr std::size_t alloc_shared_size() noexcept
{
	return sizeof(aspdetail::control_block_make_shared<T, Allocator>);
}
#else
// The amount of memory requested from the allocator when calling
// alloc_shared (object or statically sized array)
template <class T, class Allocator, std::enable_if_t<!aspdetail::is_unbounded_array_v<T>>*>
inline constexpr std::size_t alloc_shared_size() noexcept
{
	constexpr std::size_t align(alignof(T) < alignof(std::max_align_t) ? alignof(std::max_align_t) : alignof(T));
	constexpr std::size_t maxExtra(align - alignof(std::max_align_t));
	return sizeof(aspdetail::control_block_make_shared<T, Allocator>) + maxExtra;
}
#endif
// The amount of memory requested from the allocator when calling
// alloc_shared (dynamically sized array)
template <class T, class Allocator, std::enable_if_t<aspdetail::is_unbounded_array_v<T>>*>
inline constexpr std::size_t alloc_shared_size(std::size_t count) noexcept
{
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	constexpr std::size_t align(alignof(decayed_type) < alignof(std::max_align_t) ? alignof(std::max_align_t) : alignof(decayed_type));
	constexpr std::size_t maxExtra(align - alignof(std::max_align_t));
	return sizeof(aspdetail::control_block_make_unbounded_array<T, Allocator>) + maxExtra + (sizeof(decayed_type) * count);
}
// The amount of memory requested from the allocator when 
// shared_ptr takes ownership of an object using default deleter
template <class T, class Allocator>
inline constexpr std::size_t alloc_size_sp_claim() noexcept
{
	return sizeof(aspdetail::control_block_claim<T, Allocator>);
}
// The amount of memory requested from the allocator when 
// shared_ptr takes ownership of an object using a custom deleter
template<class T, class Allocator, class Deleter>
inline constexpr std::size_t alloc_size_sp_claim_custom_delete() noexcept
{
	return sizeof(aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>);
}
template
<
	class T,
	std::enable_if_t<!aspdetail::is_unbounded_array_v<T>> * = nullptr,
	class Allocator,
	class ...Args
>
inline shared_ptr<T> allocate_shared(Allocator allocator, Args&& ...args)
{
	constexpr std::size_t blockSize(alloc_shared_size<T, Allocator>());
	constexpr std::size_t blockAlign(alignof(T));

	using block_type = aspdetail::aligned_storage<blockAlign, blockSize>;
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<block_type>;

	rebound_alloc rebound(allocator);
	block_type* typedBlock(nullptr);
	aspdetail::control_block_make_shared<T, Allocator>* controlBlock(nullptr);
	try
	{
		typedBlock = rebound.allocate(1);
		std::uint8_t* const block((std::uint8_t*)typedBlock);

		const std::uintptr_t mod((std::uintptr_t)block % blockAlign);
		const std::uintptr_t offset(mod ? blockAlign - mod : 0);

		aspdetail::assert_alignment<T>(block);

		controlBlock = new (reinterpret_cast<std::uint8_t*>(block + offset)) aspdetail::control_block_make_shared<T, Allocator>(allocator, (std::uint8_t)offset, std::forward<Args>(args)...);
	}
	catch (...)
	{
		if (controlBlock){
			(*controlBlock).~control_block_make_shared();
		}
		if (typedBlock){
			rebound.deallocate(typedBlock, 1);
		}
		throw;
	}

	aspdetail::compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;

	return shared_ptr<T>(storage);
}
template
<
	class T,
	std::enable_if_t<aspdetail::is_unbounded_array_v<T>> * = nullptr,
	class Allocator,
	class ...Args
>
inline shared_ptr<T> allocate_shared(std::size_t count, Allocator allocator, Args&& ...args)
{
	using decayed_type = aspdetail::decay_unbounded_t<T>;

	const std::size_t blockSize(alloc_shared_size<T, Allocator>(count));

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<uint8_t>;

	rebound_alloc rebound(allocator);
	std::uint8_t* block(nullptr);
	aspdetail::control_block_make_unbounded_array<T, Allocator>* controlBlock(nullptr);
	decayed_type* arrayloc(nullptr);

	std::size_t constructed(0);
	try
	{
		block = rebound.allocate(blockSize);

		uint8_t* const cbend(block + sizeof(aspdetail::control_block_make_unbounded_array<T, Allocator>));
		const uintptr_t asint((uintptr_t)cbend);
		const uintptr_t mod(asint % alignof(decayed_type));
		const uintptr_t offset(mod ? alignof(decayed_type) - mod : 0);

		arrayloc = (decayed_type*)(cbend + offset);

		for (std::size_t i = 0; i < count; ++i){
			new (&arrayloc[i]) decayed_type(std::forward<Args>(args)...);
		}

		aspdetail::assert_alignment<std::max_align_t>(block);

		controlBlock = new (reinterpret_cast<std::uint8_t*>(block)) aspdetail::control_block_make_unbounded_array<T, Allocator>(arrayloc, count, allocator);
	}
	catch (...)
	{
		if (controlBlock){
			(*controlBlock).~control_block_make_unbounded_array();
		}
		for (std::size_t i = 0; i < constructed; ++i){
			arrayloc[i].~decayed_type();
		}
		if (block){
			rebound.deallocate(block, blockSize);
		}
		throw;
	}

	aspdetail::compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.m_u8[aspdetail::STORAGE_BYTE_LOCAL_REF] = aspdetail::Default_Local_Refs;

	return shared_ptr<T>(storage);
}
template
<
	class T,
	std::enable_if_t<!aspdetail::is_unbounded_array_v<T>> * = nullptr,
	class ...Args
>
inline shared_ptr<T> make_shared(Args&& ...args)
{
	return gdul::allocate_shared<T>(aspdetail::default_allocator(), std::forward<Args>(args)...);
}
template
<
	class T,
	std::enable_if_t<aspdetail::is_unbounded_array_v<T>> * = nullptr,
	class ...Args
>
inline shared_ptr<T> make_shared(std::size_t count, Args&& ...args)
{
	return gdul::allocate_shared<T>(count, aspdetail::default_allocator(), std::forward<Args>(args)...);
}
};
#pragma warning(pop)