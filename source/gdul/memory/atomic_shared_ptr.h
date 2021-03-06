// Copyright(c) 2020 Flovin Michaelsen
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
#include <cstring>

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
namespace asp_detail
{
typedef std::allocator<std::uint8_t> default_allocator;

constexpr std::uint8_t get_num_bottom_bits() { std::uint8_t i = 0, align(alignof(std::max_align_t)); for (; align; ++i, align >>= 1); return i - 1; }

constexpr std::uint8_t CbPtrBottomBits = get_num_bottom_bits();
constexpr std::uint64_t OwnedMask = (std::numeric_limits<std::uint64_t>::max() >> 16);
constexpr std::uint64_t CbMask = (std::numeric_limits<std::uint64_t>::max() >> 16) & ~std::uint64_t((std::uint16_t(1) << CbPtrBottomBits) - 1);
constexpr std::uint64_t VersionedCbMask = (std::numeric_limits<std::uint64_t>::max() >> 8);
constexpr std::uint16_t MaxVersion = (std::uint16_t(std::numeric_limits<std::uint8_t>::max()) << CbPtrBottomBits | ((std::uint16_t(1) << CbPtrBottomBits) - 1));

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

template <class T>
struct disable_deduction;

template <std::uint8_t Align, std::size_t Size>
struct max_align_storage;

template <class T>
constexpr bool is_unbounded_array_v = std::is_array<T>::value & (std::extent<T>::value == 0);

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

constexpr std::uint8_t LocalRefIndex(7);
constexpr std::uint64_t LocalRefStep(1ULL << (LocalRefIndex * 8));

// Also, the maximum number of concurrently accessing threads
constexpr std::uint8_t LocalRefFillBoundary(112);
constexpr std::uint8_t DefaultLocalRefs = std::numeric_limits<std::uint8_t>::max();

enum storage_byte : std::uint8_t
{
	storage_byte_version_lower = 0,
	storage_byte_version_upper = 6,
	storage_byte_local_ref = LocalRefIndex,
};
enum cas_flag : std::uint8_t
{
	cas_flag_none = 0,
	cas_flag_capture_on_failure = 1 << 0,
	cas_flag_steal_previous = 1 << 1,
};
}
template <class T>
class shared_ptr;

template <class T>
class raw_ptr;

template <class T, class Allocator, std::enable_if_t<!asp_detail::is_unbounded_array_v<T>>* = nullptr>
constexpr std::size_t allocate_shared_size() noexcept;

template <class T, class Allocator, std::enable_if_t<asp_detail::is_unbounded_array_v<T>>* = nullptr>
constexpr std::size_t allocate_shared_size(std::size_t count) noexcept;

template <class T, class Allocator>
constexpr std::size_t sp_claim_size() noexcept;

template <class T, class Allocator, class Deleter>
constexpr std::size_t sp_claim_size_custom_delete() noexcept;

template
<
	class T,
	std::enable_if_t<!asp_detail::is_unbounded_array_v<T>>* = nullptr,
	class Allocator,
	class ...Args
>
inline shared_ptr<T> allocate_shared(Allocator, Args&& ...);

template
<
	class T,
	std::enable_if_t<asp_detail::is_unbounded_array_v<T>>* = nullptr,
	class Allocator,
	class ...Args
>
inline shared_ptr<T> allocate_shared(std::size_t, Allocator, Args&& ...);
#pragma endregion

template <class T>
class atomic_shared_ptr
{
public:
	using size_type = asp_detail::size_type;
	using value_type = T;
	using decayed_type = asp_detail::decay_unbounded_t<T>;

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

	// Compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// Compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// Compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(raw_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// Compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(raw_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// Compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// Compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// Compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// Compare_exchange_weak may fail spuriously if another thread loads this value while attempting a swap, 
	// in addition to the regular compare_exchange_weak internal spurious failure
	inline bool compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept;

	inline void store(const shared_ptr<T>& from, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline void store(shared_ptr<T>&& from, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline shared_ptr<T> exchange(const shared_ptr<T>& with, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline shared_ptr<T> exchange(shared_ptr<T>&& with, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline shared_ptr<T> unsafe_load(std::memory_order order = std::memory_order_seq_cst) const noexcept;

	inline shared_ptr<T> unsafe_exchange(const shared_ptr<T>& with, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline shared_ptr<T> unsafe_exchange(shared_ptr<T>&& with, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline void unsafe_store(const shared_ptr<T>& from, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline void unsafe_store(shared_ptr<T>&& from, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline std::uint16_t get_version() const noexcept;

	// Cheap hint to see if this object holds a value
	operator bool() const noexcept;

	// Cheap hint comparison to ptr_base derivatives. Does not take version into account
	bool operator==(const asp_detail::ptr_base<T>& other) const noexcept;

	// Cheap hint comparison to ptr_base derivatives. Does not take version into account
	bool operator!=(const asp_detail::ptr_base<T>& other) const noexcept;

	raw_ptr<T> get_raw_ptr() const noexcept;

	decayed_type* unsafe_get() noexcept;
	const decayed_type* unsafe_get() const noexcept;

	std::uint8_t use_count_local() const noexcept;
	std::size_t unsafe_item_count() const noexcept;
	size_type unsafe_use_count() const noexcept;
	void unsafe_set_version(std::uint16_t version) noexcept;

private:
	template <class PtrType>
	inline bool compare_exchange_strong(typename asp_detail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	template <class PtrType>
	inline bool compare_exchange_weak(typename asp_detail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline constexpr asp_detail::control_block_free_type_base<T>* get_control_block() noexcept;
	inline constexpr const asp_detail::control_block_free_type_base<T>* get_control_block() const noexcept;

	using compressed_storage = asp_detail::compressed_storage;

	inline compressed_storage unsafe_copy_internal(std::memory_order order) const;
	inline compressed_storage copy_internal(std::memory_order order) const noexcept;

	inline void unsafe_store_internal(compressed_storage from, std::memory_order order);
	inline void store_internal(compressed_storage from, std::memory_order order) noexcept;

	inline compressed_storage unsafe_exchange_internal(compressed_storage with, std::memory_order order);
	inline compressed_storage exchange_internal(compressed_storage to, asp_detail::cas_flag flags, std::memory_order order) noexcept;

	inline bool compare_exchange_weak_internal(compressed_storage& expected, compressed_storage desired, asp_detail::cas_flag flags, asp_detail::memory_orders orders) noexcept;

	inline void unsafe_fill_local_refs() const noexcept;
	inline void try_fill_local_refs(compressed_storage& expected) const noexcept;

	inline constexpr asp_detail::control_block_free_type_base<T>* to_control_block(compressed_storage from) const noexcept;

	using cb_free_type = asp_detail::control_block_free_type_base<T>;
	using cb_count_type = asp_detail::control_block_base_count<T>;

	friend class shared_ptr<T>;
	friend class raw_ptr<T>;
	friend class asp_detail::ptr_base<T>;

	union
	{
		// mutable for altering local refs
		mutable std::atomic<std::uint64_t> m_storage;
		const compressed_storage m_debugView;
	};
};
template <class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr() noexcept
	: m_storage((std::uint64_t(std::numeric_limits<std::uint8_t>::max()) << (asp_detail::storage_byte_local_ref * 8)))
{
}
template<class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr(std::nullptr_t) noexcept
	: atomic_shared_ptr<T>()
{
}
template<class T>
inline constexpr atomic_shared_ptr<T>::atomic_shared_ptr(std::nullptr_t, std::uint16_t version) noexcept
	: m_storage(asp_detail::set_version(compressed_storage(), version).m_u64 | (std::uint64_t(std::numeric_limits<std::uint8_t>::max()) << (asp_detail::storage_byte_local_ref * 8)))
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
	const std::memory_order failOrder(order == std::memory_order_release || order == std::memory_order_acq_rel ? std::memory_order_seq_cst : order);
	return compare_exchange_strong(expected, std::move(desired), order, failOrder);
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
	const std::memory_order failOrder(order == std::memory_order_release || order == std::memory_order_acq_rel ? std::memory_order_seq_cst : order);
	return compare_exchange_strong(expected, std::move(desired), order, failOrder);
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
	const std::memory_order failOrder(order == std::memory_order_release || order == std::memory_order_acq_rel ? std::memory_order_seq_cst : order);
	return compare_exchange_weak(expected, std::move(desired), order, failOrder);
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
	const std::memory_order failOrder(order == std::memory_order_release || order == std::memory_order_acq_rel ? std::memory_order_seq_cst : order);
	return compare_exchange_weak(expected, std::move(desired), order, failOrder);
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
inline bool atomic_shared_ptr<T>::compare_exchange_strong(typename asp_detail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	if (desired.use_count_local() < asp_detail::LocalRefFillBoundary)
		desired.set_local_refs(std::numeric_limits<std::uint8_t>::max());

	const compressed_storage desired_(desired.m_controlBlockStorage.m_u64);

	compressed_storage expected_(m_storage.load(std::memory_order_relaxed));
	expected_.m_u64 &= ~asp_detail::VersionedCbMask;
	expected_.m_u64 |= expected.m_controlBlockStorage.m_u64 & asp_detail::VersionedCbMask;

	const asp_detail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	constexpr bool needsCapture(std::is_same<raw_type, shared_ptr<T>>::value);
	const std::uint8_t flags(asp_detail::cas_flag_capture_on_failure * needsCapture);

	const std::uint64_t preCompare(expected_.m_u64 & asp_detail::VersionedCbMask);
	do
	{
		if (compare_exchange_weak_internal(expected_, desired_, static_cast<asp_detail::cas_flag>(flags), orders))
		{

			desired.clear();

			return true;
		}

	} while (preCompare == (expected_.m_u64 & asp_detail::VersionedCbMask));

	expected = raw_type(expected_);

	return false;
}
template<class T>
template<class PtrType>
inline bool atomic_shared_ptr<T>::compare_exchange_weak(typename asp_detail::disable_deduction<PtrType>::type& expected, shared_ptr<T>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	if (desired.use_count_local() < asp_detail::LocalRefFillBoundary)
		desired.set_local_refs(std::numeric_limits<std::uint8_t>::max());

	const compressed_storage desired_(desired.m_controlBlockStorage.m_u64);
	compressed_storage expected_(m_storage.load(std::memory_order_relaxed));
	expected_.m_u64 &= ~asp_detail::VersionedCbMask;
	expected_.m_u64 |= expected.m_controlBlockStorage.m_u64 & asp_detail::VersionedCbMask;

	const asp_detail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	constexpr bool needsCapture(std::is_same<raw_type, shared_ptr<T>>::value);
	const std::uint8_t flags(asp_detail::cas_flag_capture_on_failure * needsCapture);

	const std::uint64_t preCompare(expected_.m_u64 & asp_detail::VersionedCbMask);

	if (compare_exchange_weak_internal(expected_, desired_, static_cast<asp_detail::cas_flag>(flags), orders)) {

		desired.clear();

		return true;
	}

	const std::uint64_t postCompare(expected_.m_u64 & asp_detail::VersionedCbMask);

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
	if (from.use_count_local() < asp_detail::LocalRefFillBoundary)
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
	if (with.use_count_local() < asp_detail::LocalRefFillBoundary)
		with.set_local_refs(std::numeric_limits<std::uint8_t>::max());

	compressed_storage previous(exchange_internal(with.m_controlBlockStorage, asp_detail::cas_flag_steal_previous, order));
	with.clear();
	return shared_ptr<T>(previous);
}
template<class T>
inline std::uint16_t atomic_shared_ptr<T>::get_version() const noexcept
{
	const compressed_storage storage(m_storage.load(std::memory_order_relaxed));
	return asp_detail::to_version(storage);
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::unsafe_load(std::memory_order order) const noexcept
{
	return shared_ptr<T>(unsafe_copy_internal(order));
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::unsafe_exchange(const shared_ptr<T>& with, std::memory_order order) noexcept
{
	return unsafe_exchange(shared_ptr<T>(with), order);
}
template<class T>
inline shared_ptr<T> atomic_shared_ptr<T>::unsafe_exchange(shared_ptr<T>&& with, std::memory_order order) noexcept
{
	const compressed_storage previous(unsafe_exchange_internal(with.m_controlBlockStorage, order));
	with.clear();
	return shared_ptr<T>(previous);
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_store(const shared_ptr<T>& from, std::memory_order order) noexcept
{
	unsafe_store(shared_ptr<T>(from), order);
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_store(shared_ptr<T>&& from, std::memory_order order) noexcept
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
inline typename atomic_shared_ptr<T>::decayed_type* atomic_shared_ptr<T>::unsafe_get() noexcept
{
	asp_detail::control_block_free_type_base<T>* const cb(get_control_block());
	if (cb) {
		return cb->get();
	}
	return nullptr;
}
template<class T>
inline const typename atomic_shared_ptr<T>::decayed_type* atomic_shared_ptr<T>::unsafe_get() const noexcept
{
	const asp_detail::control_block_free_type_base<T>* const cb(get_control_block());
	if (cb) {
		return cb->get();
	}
	return nullptr;
}
template<class T>
inline std::uint8_t atomic_shared_ptr<T>::use_count_local() const noexcept
{
	return compressed_storage(m_storage.load(std::memory_order_relaxed)).m_u8[asp_detail::storage_byte_local_ref];
}
template<class T>
inline std::size_t atomic_shared_ptr<T>::unsafe_item_count() const noexcept
{
	return raw_ptr<T>(*this).item_count();
}
template<class T>
inline asp_detail::size_type atomic_shared_ptr<T>::unsafe_use_count() const noexcept
{
	return raw_ptr<T>(*this).use_count();
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_set_version(std::uint16_t version) noexcept
{
	compressed_storage storage(m_storage.load(std::memory_order_acquire));
	m_storage.store(asp_detail::set_version(storage, version).m_u64, std::memory_order_release);
}
template<class T>
inline constexpr const asp_detail::control_block_free_type_base<T>* atomic_shared_ptr<T>::get_control_block() const noexcept
{
	return to_control_block(m_storage.load(std::memory_order_acquire));
}
template<class T>
inline constexpr asp_detail::control_block_free_type_base<T>* atomic_shared_ptr<T>::get_control_block() noexcept
{
	return to_control_block(m_storage.load(std::memory_order_acquire));
}
// cheap hint to see if this object holds a value
template<class T>
inline atomic_shared_ptr<T>::operator bool() const noexcept
{
	return static_cast<bool>(m_storage.load(std::memory_order_relaxed) & asp_detail::CbMask);
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
// Cheap hint comparison to ptr_base derivatives. Does not take version into account
template<class T>
inline bool atomic_shared_ptr<T>::operator==(const asp_detail::ptr_base<T>& other) const noexcept
{
	return !((m_storage.load(std::memory_order_relaxed) ^ other.m_controlBlockStorage.m_u64) & asp_detail::CbMask);
}
// Cheap hint comparison to ptr_base derivatives. Does not take version into account
template<class T>
inline bool atomic_shared_ptr<T>::operator!=(const asp_detail::ptr_base<T>& other) const noexcept
{
	return !operator==(other);
}
// ------------------------------------------------------------------------------------

// -------------------------Internals--------------------------------------------------

// ------------------------------------------------------------------------------------
template <class T>
inline void atomic_shared_ptr<T>::store_internal(compressed_storage from, std::memory_order order) noexcept
{
	exchange_internal(from, asp_detail::cas_flag_none, order);
}
template<class T>
inline constexpr asp_detail::control_block_free_type_base<T>* atomic_shared_ptr<T>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<asp_detail::control_block_free_type_base<T>*>(from.m_u64 & asp_detail::CbMask);
}
template<class T>
inline bool atomic_shared_ptr<T>::compare_exchange_weak_internal(compressed_storage& expected, compressed_storage desired, asp_detail::cas_flag flags, asp_detail::memory_orders orders) noexcept
{
	bool result(false);

	const compressed_storage desired_(asp_detail::set_version(desired, asp_detail::to_version(expected) + 1));
	compressed_storage expected_(expected);

	result = m_storage.compare_exchange_weak(expected_.m_u64, desired_.m_u64, orders.m_first, orders.m_second);

	if (result & !(flags & asp_detail::cas_flag_steal_previous))
	{
		if (asp_detail::control_block_free_type_base<T>* const cb = to_control_block(expected))
		{
			cb->decref(expected.m_u8[asp_detail::storage_byte_local_ref]);
		}
	}

	const bool otherInterjection((expected_.m_u64 ^ expected.m_u64) & asp_detail::VersionedCbMask);

	expected = expected_;

	if (otherInterjection & (flags & asp_detail::cas_flag_capture_on_failure))
	{
		expected = copy_internal(std::memory_order_relaxed);
	}

	return result;
}
template<class T>
inline void atomic_shared_ptr<T>::try_fill_local_refs(compressed_storage& expected) const noexcept
{
	if (!(expected.m_u8[asp_detail::storage_byte_local_ref] < asp_detail::LocalRefFillBoundary)) {
		return;
	}

	asp_detail::control_block_free_type_base<T>* const cb(to_control_block(expected));

	const compressed_storage initialPtrBlock(expected.m_u64 & asp_detail::VersionedCbMask);

	do {
		const std::uint8_t localRefs(expected.m_u8[asp_detail::storage_byte_local_ref]);
		const std::uint8_t newRefs(std::numeric_limits<std::uint8_t>::max() - localRefs);

		compressed_storage desired(expected);
		desired.m_u8[asp_detail::storage_byte_local_ref] = std::numeric_limits<std::uint8_t>::max();

		// Accept 'null' refs, to be able to avoid using compare_exchange_strong during copy_internal
		if (cb) {
			cb->incref(newRefs);
		}

		if (m_storage.compare_exchange_weak(expected.m_u64, desired.m_u64, std::memory_order_relaxed)) {
			return;
		}

		if (cb) {
			cb->decref(newRefs);
		}

	} while (
		(expected.m_u64 & asp_detail::VersionedCbMask) == initialPtrBlock.m_u64 &&
		expected.m_u8[asp_detail::storage_byte_local_ref] < asp_detail::LocalRefFillBoundary);
}
template <class T>
inline union asp_detail::compressed_storage atomic_shared_ptr<T>::copy_internal(std::memory_order order) const noexcept
{
	compressed_storage initial(m_storage.fetch_sub(asp_detail::LocalRefStep, order));
	initial.m_u8[asp_detail::storage_byte_local_ref] -= 1;

	compressed_storage expected(initial);
	try_fill_local_refs(expected);

	initial.m_u8[asp_detail::storage_byte_local_ref] = 1;

	return initial;
}
template <class T>
inline union asp_detail::compressed_storage atomic_shared_ptr<T>::unsafe_copy_internal(std::memory_order order) const
{
	compressed_storage storage(m_storage.load(order));

	compressed_storage newStorage(storage);
	newStorage.m_u8[asp_detail::storage_byte_local_ref] -= 1;
	m_storage.store(newStorage.m_u64, std::memory_order_relaxed);

	storage.m_u8[asp_detail::storage_byte_local_ref] = 1;

	unsafe_fill_local_refs();

	return storage;
}
template<class T>
inline union asp_detail::compressed_storage atomic_shared_ptr<T>::unsafe_exchange_internal(compressed_storage with, std::memory_order order)
{
	const compressed_storage old(m_storage.load(std::memory_order_relaxed));
	const compressed_storage replacement(asp_detail::set_version(with, asp_detail::to_version(old) + 1));

	m_storage.store(replacement.m_u64, order);

	unsafe_fill_local_refs();

	return old;
}
template <class T>
inline void atomic_shared_ptr<T>::unsafe_store_internal(compressed_storage from, std::memory_order order)
{
	const compressed_storage previous(m_storage.load(std::memory_order_relaxed));
	const compressed_storage next(asp_detail::set_version(from, asp_detail::to_version(previous) + 1));

	m_storage.store(next.m_u64, order);

	asp_detail::control_block_free_type_base<T>* const prevCb(to_control_block(previous));
	if (prevCb)
	{
		prevCb->decref(previous.m_u8[asp_detail::storage_byte_local_ref]);
	}

	unsafe_fill_local_refs();
}
template<class T>
inline void atomic_shared_ptr<T>::unsafe_fill_local_refs() const noexcept
{
	const compressed_storage current(m_storage.load(std::memory_order_relaxed));
	asp_detail::control_block_free_type_base<T>* const cb(to_control_block(current));

	if (current.m_u8[asp_detail::storage_byte_local_ref] < asp_detail::LocalRefFillBoundary) {
		const std::uint8_t newRefs(std::numeric_limits<std::uint8_t>::max() - current.m_u8[asp_detail::storage_byte_local_ref]);

		// Accept 'null' refs, to be able to avoid using compare_exchange_strong during copy_internal
		if (cb) {
			cb->incref(newRefs);
		}

		compressed_storage newStorage(current);
		newStorage.m_u8[asp_detail::storage_byte_local_ref] = std::numeric_limits<std::uint8_t>::max();

		m_storage.store(newStorage.m_u64, std::memory_order_relaxed);
	}
}
template<class T>
inline union asp_detail::compressed_storage atomic_shared_ptr<T>::exchange_internal(compressed_storage to, asp_detail::cas_flag flags, std::memory_order order) noexcept
{
	compressed_storage expected(m_storage.load(std::memory_order_relaxed));
	while (!compare_exchange_weak_internal(expected, to, flags, { order, std::memory_order_relaxed }));
	return expected;
}
namespace asp_detail
{
template <class T>
class control_block_free_type_base
{
public:
	control_block_free_type_base(compressed_storage storage) noexcept;

	using size_type = asp_detail::size_type;
	using decayed_type = asp_detail::decay_unbounded_t<T>;

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
	, m_useCount(DefaultLocalRefs)
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
	using conv_type = asp_detail::decay_unbounded_t<U>;
	using decayed_type = asp_detail::decay_unbounded_t<T>;

	inline conv_type* get() noexcept override final;
	inline const conv_type* get() const noexcept override final;

protected:
	control_block_base(decayed_type* object, std::uint8_t blockOffset = 0) noexcept;
	std::uint8_t block_offset() const;
};
template <class T, class U>
inline typename control_block_base<T, U>::conv_type* control_block_base<T, U>::get() noexcept
{
	return static_cast<conv_type*>((decayed_type*)(this->m_ptrStorage.m_u64 & OwnedMask));
}
template<class T, class U>
inline const typename control_block_base<T, U>::conv_type* control_block_base<T, U>::get() const noexcept
{
	return static_cast<const conv_type*>((decayed_type*)(this->m_ptrStorage.m_u64 & OwnedMask));
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
	using size_type = asp_detail::size_type;
	using decayed_type = asp_detail::decay_unbounded_t<T>;

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
	template <class ...Args, class U = T, std::enable_if_t<std::is_array<U>::value>* = nullptr>
	control_block_make_shared(Allocator alloc, std::uint8_t blockOffset, Args&& ...args);
	template <class ...Args, class U = T, std::enable_if_t<!std::is_array<U>::value>* = nullptr>
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

	std::uint8_t* const block((std::uint8_t*)this - this->block_offset());
	(*this).~control_block_make_shared<T, Allocator>();

	constexpr std::size_t blockSize(gdul::allocate_shared_size<T, Allocator>());
	constexpr std::size_t blockAlign(alignof(T));

	using block_type = asp_detail::max_align_storage<blockAlign, blockSize>;
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<block_type>;

	rebound_alloc rebound(alloc);

	block_type* const typedBlock((block_type*)block);

	rebound.deallocate(typedBlock, 1);
}
template <class T, class Allocator>
class control_block_make_unbounded_array : public control_block_base_count<T>
{
public:
	using decayed_type = asp_detail::decay_unbounded_t<T>;

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

	std::uint8_t* const block((std::uint8_t*)this);

	(*this).~control_block_make_unbounded_array<T, Allocator>();

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(alloc);

	rebound.deallocate(block, allocate_shared_size<T, Allocator>(nItems));
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
	std::uint8_t* const block((std::uint8_t*)this);
	(*this).~control_block_claim<T, Allocator>();

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(alloc);

	rebound.deallocate(block, gdul::sp_claim_size<T, Allocator>());
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

	T* const ptrAddr(this->get());
	m_deleter(ptrAddr, alloc);

	std::uint8_t* const block((std::uint8_t*)this);
	(*this).~control_block_claim_custom_delete<T, Allocator, Deleter>();

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(alloc);

	rebound.deallocate(block, gdul::sp_claim_size_custom_delete<T, Allocator, Deleter>());
}

template <class T>
struct disable_deduction
{
	using type = T;
};
template <std::uint8_t Align, std::size_t Size>
struct
#if 201700 < __cplusplus || _HAS_CXX17
	alignas(Align)
#else
	alignas(std::max_align_t)
#endif
	max_align_storage
{
	std::uint8_t dummy[Size]{};
};
constexpr std::uint16_t to_version(compressed_storage from)
{
	constexpr std::uint8_t bottomBits((std::uint8_t(1) << CbPtrBottomBits) - 1);
	const std::uint8_t lower(from.m_u8[storage_byte_version_lower] & bottomBits);
	const std::uint8_t upper(from.m_u8[storage_byte_version_upper]);

	const std::uint16_t version((std::uint16_t(upper) << CbPtrBottomBits) | std::uint16_t(lower));

	return version;
}
constexpr compressed_storage set_version(compressed_storage storage, std::uint16_t to)
{
	constexpr std::uint8_t bottomBits((std::uint8_t(1) << CbPtrBottomBits) - 1);

	const std::uint8_t lower(((std::uint8_t)to) & bottomBits);
	const std::uint8_t upper((std::uint8_t) (to >> CbPtrBottomBits));

	compressed_storage updated(storage);
	updated.m_u8[storage_byte_version_lower] &= ~bottomBits;
	updated.m_u8[storage_byte_version_lower] |= lower;
	updated.m_u8[storage_byte_version_upper] = upper;

	return updated;
}
template<class T>
constexpr void assert_alignment(std::uint8_t* block)
{
	static_assert(!(std::numeric_limits<std::uint8_t>::max() < alignof(T)), "alloc_shared supports only supports up to std::numeric_limits<std::uint8_t>::max() byte aligned types");

	if ((std::uintptr_t)block % alignof(std::max_align_t) != 0)
	{
		throw std::runtime_error("alloc_shared expects at least alignof(std::max_align_t) allocates");
	}
}
template <class T>
class ptr_base
{
public:
	using size_type = asp_detail::size_type;
	using value_type = T;
	using decayed_type = asp_detail::decay_unbounded_t<T>;

	inline constexpr operator bool() const noexcept;

	// Cheap hint comparison to ptr_base derivatives. Does not take version into account
	inline constexpr bool operator==(const ptr_base<T>& other) const noexcept;
	// Cheap hint comparison to ptr_base derivatives. Does not take version into account
	inline constexpr bool operator!=(const ptr_base<T>& other) const noexcept;

	// Cheap hint comparison to atomic_shared_ptr. Does not take version into account
	inline bool operator==(const atomic_shared_ptr<T>& other) const noexcept;
	// Cheap hint comparison to atomic_shared_ptr. Does not take version into account
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
	m_controlBlockStorage.m_u8[storage_byte_local_ref] *= operator bool();
}
template<class T>
inline void ptr_base<T>::clear() noexcept
{
	m_controlBlockStorage = compressed_storage();
}
template <class T>
inline constexpr ptr_base<T>::operator bool() const noexcept
{
	return m_controlBlockStorage.m_u64 & asp_detail::CbMask;
}
// Cheap hint comparison to ptr_base derivatives. Does not take version into account
template <class T>
inline constexpr bool ptr_base<T>::operator==(const ptr_base<T>& other) const noexcept
{
	return !((m_controlBlockStorage.m_u64 ^ other.m_controlBlockStorage.m_u64) & asp_detail::CbMask);
}
// Cheap hint comparison to ptr_base derivatives. Does not take version into account
template <class T>
inline constexpr bool ptr_base<T>::operator!=(const ptr_base<T>& other) const noexcept
{
	return !operator==(other);
}
// Cheap hint comparison to atomic_shared_ptr. Does not take version into account
template<class T>
inline bool ptr_base<T>::operator==(const atomic_shared_ptr<T>& other) const noexcept
{
	return other == *this;
}
// Cheap hint comparison to atomic_shared_ptr. Does not take version into account
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
	return reinterpret_cast<control_block_free_type_base<T>*>(from.m_u64 & CbMask);
}
template <class T>
inline constexpr const control_block_free_type_base<T>* ptr_base<T>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<const control_block_free_type_base<T>*>(from.m_u64 & CbMask);
}
template <class T>
inline constexpr typename ptr_base<T>::decayed_type* ptr_base<T>::to_object(compressed_storage from) noexcept
{
	control_block_free_type_base<T>* const cb(to_control_block(from));
	if (cb) {
		return cb->get();
	}
	return nullptr;
}
template <class T>
inline constexpr const typename ptr_base<T>::decayed_type* ptr_base<T>::to_object(compressed_storage from) const noexcept
{
	const control_block_free_type_base<T>* const cb(to_control_block(from));
	if (cb) {
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
class shared_ptr : public asp_detail::ptr_base<T>
{
public:
	using decayed_type = asp_detail::decay_unbounded_t<T>;

	inline constexpr shared_ptr() noexcept;

	inline explicit constexpr shared_ptr(std::nullptr_t) noexcept;
	inline explicit constexpr shared_ptr(std::nullptr_t, std::uint16_t version) noexcept;

	using asp_detail::ptr_base<T>::ptr_base;

	inline shared_ptr(const shared_ptr<T>& other) noexcept;

	template <class U, std::enable_if_t<std::is_convertible<U*, T*>::value>* = nullptr>
	inline shared_ptr(shared_ptr<U>&& other) noexcept;

	shared_ptr<T>& operator=(const shared_ptr<T>& other) noexcept;

	template <class U, std::enable_if_t<std::is_convertible<U*, T*>::value>* = nullptr>
	shared_ptr<T>& operator=(shared_ptr<U>&& other) noexcept;

	template <class U, std::enable_if_t<std::is_convertible<T*, U*>::value>* = nullptr>
	inline operator const shared_ptr<U>& () const noexcept;
	template <class U, std::enable_if_t<std::is_convertible<T*, U*>::value>* = nullptr>
	inline operator shared_ptr<U>& () noexcept;
	template <class U, std::enable_if_t<std::is_convertible<T*, U*>::value>* = nullptr>
	inline explicit operator shared_ptr<U> && () noexcept;

	template <class U, std::enable_if_t<std::is_base_of<T, U>::value>* = nullptr>
	inline explicit operator const shared_ptr<U>& () const noexcept;

	template <class U, std::enable_if_t<std::is_base_of<T, U>::value>* = nullptr>
	inline explicit operator shared_ptr<U>& () noexcept;

	template <class U, std::enable_if_t<std::is_base_of<T, U>::value>* = nullptr>
	inline explicit operator shared_ptr<U> && () noexcept;

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

	inline const decayed_type& operator[](asp_detail::size_type index) const;
	inline decayed_type& operator[](asp_detail::size_type index);

	inline constexpr raw_ptr<T> get_raw_ptr() const noexcept;
	inline void swap(shared_ptr<T>& other) noexcept;
	inline constexpr std::uint8_t use_count_local() const noexcept;

private:
	inline void set_local_refs(std::uint8_t target) noexcept;

	constexpr void clear() noexcept;

	using compressed_storage = asp_detail::compressed_storage;
	inline constexpr shared_ptr(union asp_detail::compressed_storage from) noexcept;

	template<class Deleter, class Allocator>
	inline compressed_storage create_control_block(T* object, Allocator allocator, Deleter&& deleter);
	template <class Allocator>
	inline compressed_storage create_control_block(T* object, Allocator allocator);

	friend class raw_ptr<T>;
	friend class atomic_shared_ptr<T>;

	template
		<
		class U,
		std::enable_if_t<!asp_detail::is_unbounded_array_v<U>>*,
		class Allocator,
		class ...Args
		>
		friend shared_ptr<U> allocate_shared(Allocator, Args&& ...);

	template
		<
		class U,
		std::enable_if_t<asp_detail::is_unbounded_array_v<U>>*,
		class Allocator,
		class ...Args
		>
		friend shared_ptr<U> allocate_shared(std::size_t, Allocator, Args&& ...);

	decayed_type* m_ptr;
};
template <class T>
inline constexpr shared_ptr<T>::shared_ptr() noexcept
	: asp_detail::ptr_base<T>()
	, m_ptr(nullptr)
{}
template <class T>
inline constexpr shared_ptr<T>::shared_ptr(std::nullptr_t) noexcept
	: asp_detail::ptr_base<T>(compressed_storage())
	, m_ptr(nullptr)
{}
template <class T>
inline constexpr shared_ptr<T>::shared_ptr(std::nullptr_t, std::uint16_t version) noexcept
	: asp_detail::ptr_base<T>(set_version(compressed_storage(), version))
	, m_ptr(nullptr)
{}
template<class T>
inline shared_ptr<T>::~shared_ptr() noexcept
{
	asp_detail::control_block_free_type_base<T>* const cb(this->get_control_block());
	if (cb)
	{
		cb->decref(this->m_controlBlockStorage.m_u8[asp_detail::storage_byte_local_ref]);
	}
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible<U*, T*>::value>*>
inline shared_ptr<T>::shared_ptr(shared_ptr<U>&& other) noexcept
	: shared_ptr<T>()
{
	std::memmove(this, &other, sizeof(shared_ptr<T>));
	std::memset(&other, 0, sizeof(shared_ptr<U>));

	this->m_ptr = this->to_object(this->m_controlBlockStorage);
}
template<class T>
inline shared_ptr<T>::shared_ptr(const shared_ptr<T>& other) noexcept
	: shared_ptr<T>()
{
	compressed_storage copy(other.m_controlBlockStorage);

	if (asp_detail::control_block_free_type_base<T>* const cb = this->to_control_block(copy))
	{
		copy.m_u8[asp_detail::storage_byte_local_ref] = asp_detail::DefaultLocalRefs;
		cb->incref(asp_detail::DefaultLocalRefs);
	}

	this->m_controlBlockStorage = copy;
	this->m_ptr = this->to_object(this->m_controlBlockStorage);
}
template <class T>
inline shared_ptr<T>::shared_ptr(T* object)
	: shared_ptr<T>()
{
	asp_detail::default_allocator alloc;
	this->m_controlBlockStorage = create_control_block(object, alloc);
	this->m_ptr = this->to_object(this->m_controlBlockStorage);
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible<T*, U*>::value>*>
inline shared_ptr<T>::operator const shared_ptr<U>& () const noexcept
{
	return *reinterpret_cast<const shared_ptr<U>*>(this);
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible<T*, U*>::value>*>
inline shared_ptr<T>::operator shared_ptr<U>& () noexcept
{
	return *reinterpret_cast<shared_ptr<U>*>(this);
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible<T*, U*>::value>*>
inline shared_ptr<T>::operator shared_ptr<U> && () noexcept
{
	return std::move(*reinterpret_cast<shared_ptr<U>*>(this));
}
template <class T>
template <class U, std::enable_if_t<std::is_base_of<T, U>::value>*>
inline shared_ptr<T>::operator const shared_ptr<U>& () const noexcept
{
	return *reinterpret_cast<const shared_ptr<U>*>(this);
}
template <class T>
template <class U, std::enable_if_t<std::is_base_of<T, U>::value>*>
inline shared_ptr<T>::operator shared_ptr<U>& () noexcept
{
	return *reinterpret_cast<shared_ptr<U>*>(this);
}
template <class T>
template <class U, std::enable_if_t<std::is_base_of<T, U>::value>*>
inline shared_ptr<T>::operator shared_ptr<U> && () noexcept
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
	asp_detail::ptr_base<T>::clear();
}
template<class T>
inline void shared_ptr<T>::set_local_refs(std::uint8_t target) noexcept
{
	if (asp_detail::control_block_free_type_base<T>* const cb = this->get_control_block())
	{
		const uint8_t localRefs(this->m_controlBlockStorage.m_u8[asp_detail::storage_byte_local_ref]);

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

	this->m_controlBlockStorage.m_u8[asp_detail::storage_byte_local_ref] = target;
}
template <class T>
inline constexpr std::uint8_t shared_ptr<T>::use_count_local() const noexcept
{
	return this->m_controlBlockStorage.m_u8[asp_detail::storage_byte_local_ref];
}
template<class T>
inline constexpr shared_ptr<T>::shared_ptr(union asp_detail::compressed_storage from) noexcept
	: asp_detail::ptr_base<T>(from)
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
inline const typename shared_ptr<T>::decayed_type& shared_ptr<T>::operator[](asp_detail::size_type index) const
{
	return get()[index];
}
template <class T>
inline typename shared_ptr<T>::decayed_type& shared_ptr<T>::operator[](asp_detail::size_type index)
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
	if (!asp_detail::ptr_base<T>::operator bool())
	{
		return 0;
	}

	return this->get_control_block()->item_count();
}
template <class T>
template<class Deleter, class Allocator>
inline union asp_detail::compressed_storage shared_ptr<T>::create_control_block(T* object, Allocator allocator, Deleter&& deleter)
{
	static_assert(!(8 < alignof(Deleter)), "No support for custom aligned deleter objects");

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(allocator);

	asp_detail::control_block_claim_custom_delete<T, Allocator, Deleter>* controlBlock(nullptr);

	constexpr std::size_t blockSize(sp_claim_size_custom_delete<T, Allocator, Deleter>());

	void* block(nullptr);

	try
	{
		block = rebound.allocate(blockSize);

		if ((std::uintptr_t)block % alignof(std::max_align_t) != 0)
		{
			throw std::runtime_error("alloc_shared expects at least alignof(max_align_t) allocates");
		}

		controlBlock = new (block) asp_detail::control_block_claim_custom_delete<T, Allocator, Deleter>(object, allocator, std::forward<Deleter&&>(deleter));
	}
	catch (...)
	{
		rebound.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		deleter(object, allocator);
		throw;
	}
	compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.m_u8[asp_detail::storage_byte_local_ref] = asp_detail::DefaultLocalRefs;

	return storage;
}
template<class T>
template <class Allocator>
inline union asp_detail::compressed_storage shared_ptr<T>::create_control_block(T* object, Allocator allocator)
{
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	rebound_alloc rebound(allocator);

	asp_detail::control_block_claim<T, Allocator>* controlBlock(nullptr);

	constexpr std::size_t blockSize(sp_claim_size<T, Allocator>());

	void* block(nullptr);

	try
	{
		block = rebound.allocate(blockSize);

		if ((std::uintptr_t)block % alignof(std::max_align_t) != 0)
		{
			throw std::runtime_error("alloc_shared expects at least alignof(max_align_t) allocates");
		}

		controlBlock = new (block) asp_detail::control_block_claim<T, Allocator>(object, rebound);
	}
	catch (...)
	{
		rebound.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		delete object;
		throw;
	}

	compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.m_u8[asp_detail::storage_byte_local_ref] = asp_detail::DefaultLocalRefs;

	return storage;
}
template<class T>
inline shared_ptr<T>& shared_ptr<T>::operator=(const shared_ptr<T>& other) noexcept
{
	shared_ptr<T>(other).swap(*this);

	return *this;
}
template<class T>
template <class U, std::enable_if_t<std::is_convertible<U*, T*>::value>*>
inline shared_ptr<T>& shared_ptr<T>::operator=(shared_ptr<U>&& other) noexcept
{
	shared_ptr<T>(std::move(other)).swap(*this);

	return *this;
}

template<class T>
inline void shared_ptr<T>::swap(shared_ptr<T>& other) noexcept
{
	asp_detail::ptr_base<T>::swap(other);
	std::swap(m_ptr, other.m_ptr);
}

// raw_ptr does not share in ownership of the object
template <class T>
class raw_ptr : public asp_detail::ptr_base<T>
{
public:
	inline constexpr raw_ptr() noexcept;

	inline explicit constexpr raw_ptr(std::nullptr_t) noexcept;
	inline explicit constexpr raw_ptr(std::nullptr_t, std::uint16_t version) noexcept;

	using asp_detail::ptr_base<T>::ptr_base;
	using decayed_type = asp_detail::decay_unbounded_t<T>;

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

	inline const decayed_type& operator[](asp_detail::size_type index) const;
	inline decayed_type& operator[](asp_detail::size_type index);

	inline void swap(raw_ptr<T>& other) noexcept;

	constexpr raw_ptr<T>& operator=(const raw_ptr<T>& other) noexcept;
	constexpr raw_ptr<T>& operator=(raw_ptr<T>&& other) noexcept;
	constexpr raw_ptr<T>& operator=(const shared_ptr<T>& from) noexcept;
	constexpr raw_ptr<T>& operator=(const atomic_shared_ptr<T>& from) noexcept;

private:
	typedef asp_detail::compressed_storage compressed_storage;

	explicit constexpr raw_ptr(compressed_storage from) noexcept;

	friend class asp_detail::ptr_base<T>;
	friend class atomic_shared_ptr<T>;
	friend class shared_ptr<T>;
};
template<class T>
inline constexpr raw_ptr<T>::raw_ptr() noexcept
	: asp_detail::ptr_base<T>()
{
}
template<class T>
inline constexpr raw_ptr<T>::raw_ptr(std::nullptr_t) noexcept
	: asp_detail::ptr_base<T>(compressed_storage())
{}
template<class T>
inline constexpr raw_ptr<T>::raw_ptr(std::nullptr_t, std::uint16_t version) noexcept
	: asp_detail::ptr_base<T>(set_version(compressed_storage(), version))
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
	: asp_detail::ptr_base<T>(from.m_controlBlockStorage)
{
	this->m_controlBlockStorage.m_u8[asp_detail::storage_byte_local_ref] = 0;
}
template<class T>
inline raw_ptr<T>::raw_ptr(const atomic_shared_ptr<T>& from) noexcept
	: asp_detail::ptr_base<T>(from.m_storage.load(std::memory_order_relaxed))
{
	this->m_controlBlockStorage.m_u8[asp_detail::storage_byte_local_ref] = 0;
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
inline const typename raw_ptr<T>::decayed_type& raw_ptr<T>::operator[](asp_detail::size_type index) const
{
	return get()[index];
}
template <class T>
inline typename raw_ptr<T>::decayed_type& raw_ptr<T>::operator[](asp_detail::size_type index)
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
	: asp_detail::ptr_base<T>(from)
{
	this->m_controlBlockStorage.m_u8[asp_detail::storage_byte_local_ref] = 0;
}
#if 201700 < __cplusplus || _HAS_CXX17
// The amount of memory requested from the allocator when calling
// alloc_shared (object or statically sized array)
template <class T, class Allocator, std::enable_if_t<!asp_detail::is_unbounded_array_v<T>>*>
inline constexpr std::size_t allocate_shared_size() noexcept
{
	return sizeof(asp_detail::control_block_make_shared<T, Allocator>);
}
#else
// The amount of memory requested from the allocator when calling
// alloc_shared (object or statically sized array)
template <class T, class Allocator, std::enable_if_t<!asp_detail::is_unbounded_array_v<T>>*>
inline constexpr std::size_t allocate_shared_size() noexcept
{
	constexpr std::size_t align(alignof(T) < alignof(std::max_align_t) ? alignof(std::max_align_t) : alignof(T));
	constexpr std::size_t maxExtra(align - alignof(std::max_align_t));
	return sizeof(asp_detail::control_block_make_shared<T, Allocator>) + maxExtra;
}
#endif
// The amount of memory requested from the allocator when calling
// alloc_shared (dynamically sized array)
template <class T, class Allocator, std::enable_if_t<asp_detail::is_unbounded_array_v<T>>*>
inline constexpr std::size_t allocate_shared_size(std::size_t count) noexcept
{
	using decayed_type = asp_detail::decay_unbounded_t<T>;

	constexpr std::size_t align(alignof(decayed_type) < alignof(std::max_align_t) ? alignof(std::max_align_t) : alignof(decayed_type));
	constexpr std::size_t maxExtra(align - alignof(std::max_align_t));
	return sizeof(asp_detail::control_block_make_unbounded_array<T, Allocator>) + maxExtra + (sizeof(decayed_type) * count);
}
// The amount of memory requested from the allocator when 
// shared_ptr takes ownership of an object using default deleter
template <class T, class Allocator>
inline constexpr std::size_t sp_claim_size() noexcept
{
	return sizeof(asp_detail::control_block_claim<T, Allocator>);
}
// The amount of memory requested from the allocator when 
// shared_ptr takes ownership of an object using a custom deleter
template<class T, class Allocator, class Deleter>
inline constexpr std::size_t sp_claim_size_custom_delete() noexcept
{
	return sizeof(asp_detail::control_block_claim_custom_delete<T, Allocator, Deleter>);
}
template
<
	class T,
	std::enable_if_t<!asp_detail::is_unbounded_array_v<T>>*,
	class Allocator,
	class ...Args
>
inline shared_ptr<T> allocate_shared(Allocator allocator, Args&& ...args)
{
	constexpr std::size_t blockSize(allocate_shared_size<T, Allocator>());
	constexpr std::size_t blockAlign(alignof(T));

	using block_type = asp_detail::max_align_storage<blockAlign, blockSize>;
	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<block_type>;

	rebound_alloc rebound(allocator);
	block_type* typedBlock(nullptr);
	asp_detail::control_block_make_shared<T, Allocator>* controlBlock(nullptr);
	try
	{
		typedBlock = rebound.allocate(1);
		std::uint8_t* const block((std::uint8_t*)typedBlock);

		const std::uintptr_t mod((std::uintptr_t)block % blockAlign);
		const std::uintptr_t offset(mod ? blockAlign - mod : 0);

		asp_detail::assert_alignment<std::max_align_t>(block);

		controlBlock = new (reinterpret_cast<std::uint8_t*>(block + offset)) asp_detail::control_block_make_shared<T, Allocator>(allocator, (std::uint8_t)offset, std::forward<Args>(args)...);
	}
	catch (...)
	{
		if (controlBlock) {
			(*controlBlock).~control_block_make_shared();
		}
		if (typedBlock) {
			rebound.deallocate(typedBlock, 1);
		}
		throw;
	}

	asp_detail::compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.m_u8[asp_detail::storage_byte_local_ref] = asp_detail::DefaultLocalRefs;

	return shared_ptr<T>(storage);
}
template
<
	class T,
	std::enable_if_t<asp_detail::is_unbounded_array_v<T>>*,
	class Allocator,
	class ...Args
>
inline shared_ptr<T> allocate_shared(std::size_t count, Allocator allocator, Args&& ...args)
{
	using decayed_type = asp_detail::decay_unbounded_t<T>;

	const std::size_t blockSize(allocate_shared_size<T, Allocator>(count));

	using rebound_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<uint8_t>;

	rebound_alloc rebound(allocator);
	std::uint8_t* block(nullptr);
	asp_detail::control_block_make_unbounded_array<T, Allocator>* controlBlock(nullptr);
	decayed_type* arrayloc(nullptr);

	std::size_t constructed(0);
	try
	{
		block = rebound.allocate(blockSize);

		uint8_t* const cbend(block + sizeof(asp_detail::control_block_make_unbounded_array<T, Allocator>));
		const uintptr_t asint((uintptr_t)cbend);
		const uintptr_t mod(asint % alignof(decayed_type));
		const uintptr_t offset(mod ? alignof(decayed_type) - mod : 0);

		arrayloc = (decayed_type*)(cbend + offset);

		for (std::size_t i = 0; i < count; ++i, ++constructed) {
			new (&arrayloc[i]) decayed_type(std::forward<Args>(args)...);
		}

		asp_detail::assert_alignment<std::max_align_t>(block);

		controlBlock = new (reinterpret_cast<std::uint8_t*>(block)) asp_detail::control_block_make_unbounded_array<T, Allocator>(arrayloc, count, allocator);
	}
	catch (...)
	{
		if (controlBlock) {
			(*controlBlock).~control_block_make_unbounded_array();
		}
		for (std::size_t i = 0; i < constructed; ++i) {
			const std::size_t reverseIndex(constructed - i - 1);
			arrayloc[reverseIndex].~decayed_type();
		}
		if (block) {
			rebound.deallocate(block, blockSize);
		}
		throw;
	}

	asp_detail::compressed_storage storage(reinterpret_cast<std::uint64_t>(controlBlock));
	storage.m_u8[asp_detail::storage_byte_local_ref] = asp_detail::DefaultLocalRefs;

	return shared_ptr<T>(storage);
}
template
<
	class T,
	std::enable_if_t<!asp_detail::is_unbounded_array_v<T>>* = nullptr,
	class ...Args
>
inline shared_ptr<T> make_shared(Args&& ...args)
{
	return gdul::allocate_shared<T>(asp_detail::default_allocator(), std::forward<Args>(args)...);
}
template
<
	class T,
	std::enable_if_t<asp_detail::is_unbounded_array_v<T>>* = nullptr,
	class ...Args
>
inline shared_ptr<T> make_shared(std::size_t count, Args&& ...args)
{
	return gdul::allocate_shared<T>(count, asp_detail::default_allocator(), std::forward<Args>(args)...);
}
};
#pragma warning(pop)
