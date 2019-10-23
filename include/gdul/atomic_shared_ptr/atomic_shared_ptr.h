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

#undef max

#pragma warning(push)
#pragma warning(disable : 4201)

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

struct memory_orders{std::memory_order myFirst, mySecond;};

enum STORAGE_BYTE : std::uint8_t;
enum CAS_FLAG : std::uint8_t;

static constexpr std::uint8_t Copy_Request_Index(7);
static constexpr std::uint64_t Copy_Request_Step(1ULL << (Copy_Request_Index * 8));

}
template <class T, class Allocator = aspdetail::default_allocator>
class shared_ptr;

template <class T, class Allocator = aspdetail::default_allocator>
class raw_ptr;

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

	inline constexpr atomic_shared_ptr() noexcept;
	inline constexpr atomic_shared_ptr(std::nullptr_t) noexcept;
	inline constexpr atomic_shared_ptr(std::nullptr_t, std::uint8_t version) noexcept;

	inline atomic_shared_ptr(const shared_ptr<T, Allocator>& from) noexcept;
	inline atomic_shared_ptr(shared_ptr<T, Allocator>&& from) noexcept;

	inline ~atomic_shared_ptr() noexcept;

	inline atomic_shared_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& from) noexcept;
	inline atomic_shared_ptr<T, Allocator>& operator=(shared_ptr<T, Allocator>&& from) noexcept;

	inline bool compare_exchange_strong(raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline bool compare_exchange_strong(raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline bool compare_exchange_strong(raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	inline bool compare_exchange_strong(raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline bool compare_exchange_strong(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline bool compare_exchange_strong(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline bool compare_exchange_strong(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	inline bool compare_exchange_strong(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;
	// compare_exchange_weak may fail if another thread loads this value while attempting swap
	inline bool compare_exchange_weak(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline shared_ptr<T, Allocator> load(std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline void store(const shared_ptr<T, Allocator>& from, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline void store(shared_ptr<T, Allocator>&& from, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline shared_ptr<T, Allocator> exchange(const shared_ptr<T, Allocator>& with, std::memory_order order = std::memory_order_seq_cst) noexcept;
	inline shared_ptr<T, Allocator> exchange(shared_ptr<T, Allocator>&& with, std::memory_order order = std::memory_order_seq_cst) noexcept;

	inline std::uint8_t get_version() const noexcept;

	inline shared_ptr<T, Allocator> unsafe_load(std::memory_order order = std::memory_order_seq_cst);

	inline shared_ptr<T, Allocator> unsafe_exchange(const shared_ptr<T, Allocator>& with, std::memory_order order = std::memory_order_seq_cst);
	inline shared_ptr<T, Allocator> unsafe_exchange(shared_ptr<T, Allocator>&& with, std::memory_order order = std::memory_order_seq_cst);

	inline void unsafe_store(const shared_ptr<T, Allocator>& from, std::memory_order order = std::memory_order_seq_cst);
	inline void unsafe_store(shared_ptr<T, Allocator>&& from, std::memory_order order = std::memory_order_seq_cst);

	// cheap hint to see if this object holds a value
	operator bool() const noexcept;

	// cheap hint comparison to ptr_base derivatives
	bool operator==(const aspdetail::ptr_base<T, Allocator>& other) const noexcept;

	// cheap hint comparison to ptr_base derivatives
	bool operator!=(const aspdetail::ptr_base<T, Allocator>& other) const noexcept;

	size_type unsafe_use_count() const;

	raw_ptr<T, Allocator> unsafe_get_raw_ptr() const;

	T* unsafe_get_owned();
	const T* unsafe_get_owned() const;

private:
	inline constexpr aspdetail::control_block_base<T, Allocator>* get_control_block() noexcept;
	inline constexpr const aspdetail::control_block_base<T, Allocator>* get_control_block() const noexcept;

	typedef typename aspdetail::compressed_storage compressed_storage;
	typedef typename aspdetail::ptr_base<T, Allocator>::size_type size_type;

	inline compressed_storage copy_internal(std::memory_order order) noexcept;
	inline compressed_storage unsafe_copy_internal(std::memory_order order);
	inline compressed_storage unsafe_exchange_internal(compressed_storage with, std::memory_order order);

	inline void unsafe_store_internal(compressed_storage from, std::memory_order order);
	inline void store_internal(compressed_storage from, std::memory_order order) noexcept;

	inline compressed_storage exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags, std::memory_order order) noexcept;

	template <class PtrType>
	inline bool compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	template <class PtrType>
	inline bool compare_exchange_weak(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept;

	inline bool compare_exchange_weak_internal(compressed_storage& expected, compressed_storage desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept;
	inline bool compare_exchange_weak_internal_fast_swap(compressed_storage & expected, const compressed_storage & desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept;
	inline bool compare_exchange_weak_internal_helping_swap(compressed_storage & expected, const compressed_storage & desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept;

	inline bool try_help_increment_and_try_swap(compressed_storage& expected, compressed_storage desired, aspdetail::memory_orders orders) noexcept;
	inline void try_help_increment(compressed_storage expected, std::memory_order order) noexcept;

	inline constexpr aspdetail::control_block_base<T, Allocator>* to_control_block(compressed_storage from) const noexcept;

	friend class shared_ptr<T, Allocator>;
	friend class raw_ptr<T, Allocator>;
	friend class aspdetail::ptr_base<T, Allocator>;

	union
	{
		std::atomic<std::uint64_t> myStorage;
		const compressed_storage myDebugView;
	};
};
template <class T, class Allocator>
inline constexpr atomic_shared_ptr<T, Allocator>::atomic_shared_ptr() noexcept
	: myStorage(0ULL)
{
	static_assert(std::is_same<Allocator::value_type, std::uint8_t>(), "value_type for allocator must be std::uint8_t");
}
template<class T, class Allocator>
inline constexpr atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(std::nullptr_t) noexcept
	: atomic_shared_ptr<T, Allocator>()
{
}
template<class T, class Allocator>
inline constexpr atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(std::nullptr_t, std::uint8_t version) noexcept
	: myStorage(0ULL | (std::uint64_t(version) << (aspdetail::STORAGE_BYTE_VERSION * 8)))
{
}
template <class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(shared_ptr<T, Allocator>&& other) noexcept
	: atomic_shared_ptr<T, Allocator>()
{
	unsafe_store(std::move(other));
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(const shared_ptr<T, Allocator>& from) noexcept
	: atomic_shared_ptr<T, Allocator>()
{
	unsafe_store(from);
}
template <class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::~atomic_shared_ptr() noexcept
{
	unsafe_store_internal(compressed_storage(), std::memory_order_relaxed);
}

template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order order) noexcept
{
	return compare_exchange_strong(expected, shared_ptr<T, Allocator>(desired), order);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order order) noexcept
{
	return compare_exchange_strong(expected, std::move(desired), order, order);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_strong(expected, shared_ptr<T, Allocator>(desired), successOrder, failOrder);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_strong<decltype(expected)>(expected, std::move(desired), successOrder, failOrder);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order order) noexcept
{
	return compare_exchange_strong(expected, shared_ptr<T, Allocator>(desired), order);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order order) noexcept
{
	return compare_exchange_strong(expected, std::move(desired), order, order);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_strong(expected, shared_ptr<T, Allocator>(desired), successOrder, failOrder);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_strong<decltype(expected)>(expected, std::move(desired), successOrder, failOrder);
}

template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order order) noexcept
{
	return compare_exchange_weak(expected, shared_ptr<T, Allocator>(desired), order);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order order) noexcept
{
	return compare_exchange_weak(expected, std::move(desired), order, order);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_weak(expected, shared_ptr<T, Allocator>(desired), successOrder, failOrder);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_weak<decltype(expected)>(expected, std::move(desired), successOrder, failOrder);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order order) noexcept
{
	return compare_exchange_weak(expected, shared_ptr<T, Allocator>(desired), order);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order order) noexcept
{
	return compare_exchange_weak(expected, std::move(desired), order, order);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_weak(expected, shared_ptr<T, Allocator>(desired), successOrder, failOrder);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	return compare_exchange_weak<decltype(expected)>(expected, std::move(desired), successOrder, failOrder);
}
template<class T, class Allocator>
template<class PtrType>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type & expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	const compressed_storage desired_(desired.myControlBlockStorage.myU64);
	compressed_storage expected_(expected.myControlBlockStorage.myU64);

	const aspdetail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	const bool needsCapture(std::is_same<raw_type, shared_ptr<T, Allocator>>::value);
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
template<class T, class Allocator>
template<class PtrType>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak(typename aspdetail::disable_deduction<PtrType>::type & expected, shared_ptr<T, Allocator>&& desired, std::memory_order successOrder, std::memory_order failOrder) noexcept
{
	const compressed_storage desired_(desired.myControlBlockStorage.myU64);
	compressed_storage expected_(expected.myControlBlockStorage.myU64);

	const aspdetail::memory_orders orders{ successOrder, failOrder };

	typedef typename std::remove_reference<PtrType>::type raw_type;

	const bool needsCapture(std::is_same<raw_type, shared_ptr<T, Allocator>>::value);
	const std::uint8_t flags(aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE * static_cast<std::uint8_t>(needsCapture));

	const std::uint64_t preCompare(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);

	if (compare_exchange_weak_internal(expected_, desired_, static_cast<aspdetail::CAS_FLAG>(flags), orders)) {

		desired.reset();

		return true;
	}

	const std::uint64_t postCompare(expected_.myU64 & aspdetail::Versioned_Ptr_Mask);

	// Another thread altered copy request value, causing compare_exchange to fail, even 
	// though held pointer remains the same.
	if (preCompare == postCompare) {
		std::atomic_thread_fence(orders.mySecond);
		return false;
	}

	expected = raw_type(expected_);

	return false;
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>& atomic_shared_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& from) noexcept
{
	store(from);
	return *this;
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>& atomic_shared_ptr<T, Allocator>::operator=(shared_ptr<T, Allocator>&& from) noexcept
{
	store(std::move(from));
	return *this;
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::load(std::memory_order order) noexcept
{
	compressed_storage copy(copy_internal(order));
	return shared_ptr<T, Allocator>(copy);
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store(const shared_ptr<T, Allocator>& from, std::memory_order order) noexcept
{
	store(shared_ptr<T, Allocator>(from), order);
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store(shared_ptr<T, Allocator>&& from, std::memory_order order) noexcept
{
	store_internal(from.myControlBlockStorage.myU64, order);
	from.reset();
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::exchange(const shared_ptr<T, Allocator>& with, std::memory_order order) noexcept
{
	return exchange(shared_ptr<T, Allocator>(with), order);
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::exchange(shared_ptr<T, Allocator>&& with, std::memory_order order) noexcept
{
	compressed_storage previous(exchange_internal(with.myControlBlockStorage, aspdetail::CAS_FLAG_STEAL_PREVIOUS, order));
	with.reset();
	return shared_ptr<T, Allocator>(previous);
}
template<class T, class Allocator>
inline std::uint8_t atomic_shared_ptr<T, Allocator>::get_version() const noexcept
{
	const compressed_storage storage(myStorage.load(std::memory_order_relaxed));
	return storage.myU8[aspdetail::STORAGE_BYTE_VERSION];
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_load(std::memory_order order)
{
	return shared_ptr<T, Allocator>(unsafe_copy_internal(order));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_exchange(const shared_ptr<T, Allocator>& with, std::memory_order order)
{
	return unsafe_exchange(shared_ptr<T, Allocator>(with), order);
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_exchange(shared_ptr<T, Allocator>&& with, std::memory_order order)
{
	const compressed_storage previous(unsafe_exchange_internal(with.myControlBlockStorage, order));
	with.reset();
	return shared_ptr<T, Allocator>(previous);
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store(const shared_ptr<T, Allocator>& from, std::memory_order order)
{
	unsafe_store(shared_ptr<T, Allocator>(from), order);
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store(shared_ptr<T, Allocator>&& from, std::memory_order order)
{
	unsafe_store_internal(from.myControlBlockStorage, order);
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
inline raw_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_get_raw_ptr() const
{
	compressed_storage storage(myStorage.load(std::memory_order_acquire));
	return raw_ptr<T, Allocator>(storage);
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
inline constexpr const aspdetail::control_block_base<T, Allocator>* atomic_shared_ptr<T, Allocator>::get_control_block() const noexcept
{
	return to_control_block(myStorage.load(std::memory_order_acquire));
}
template<class T, class Allocator>
inline constexpr aspdetail::control_block_base<T, Allocator>* atomic_shared_ptr<T, Allocator>::get_control_block() noexcept
{
	return to_control_block(myStorage.load(std::memory_order_acquire));
}
// cheap hint to see if this object holds a value
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::operator bool() const noexcept
{
	return static_cast<bool>(myStorage.load(std::memory_order_relaxed) & aspdetail::Ptr_Mask);
}
// cheap hint to see if this object holds a value
template <class T, class Allocator>
inline constexpr bool operator==(std::nullptr_t /*aNullptr*/, const atomic_shared_ptr<T, Allocator>& ptr) noexcept
{
	return !ptr;
}
// cheap hint to see if this object holds a value
template <class T, class Allocator>
inline constexpr bool operator!=(std::nullptr_t /*aNullptr*/, const atomic_shared_ptr<T, Allocator>& ptr) noexcept
{
	return ptr;
}
// cheap hint to see if this object holds a value
template <class T, class Allocator>
inline constexpr bool operator==(const atomic_shared_ptr<T, Allocator>& ptr, std::nullptr_t /*aNullptr*/) noexcept
{
	return !ptr;
}
// cheap hint to see if this object holds a value
template <class T, class Allocator>
inline constexpr bool operator!=(const atomic_shared_ptr<T, Allocator>& ptr, std::nullptr_t /*aNullptr*/) noexcept
{
	return ptr;
}
// cheap hint comparison to ptr_base derivatives
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::operator==(const aspdetail::ptr_base<T, Allocator>& other) const noexcept
{
	return !((myStorage.load(std::memory_order_relaxed) ^ other.myControlBlockStorage.myU64) & aspdetail::Versioned_Ptr_Mask);
}
// cheap hint comparison to ptr_base derivatives
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::operator!=(const aspdetail::ptr_base<T, Allocator>& other) const noexcept
{
	return !operator==(other);
}
// ------------------------------------------------------------------------------------

// -------------------------Internals--------------------------------------------------

// ------------------------------------------------------------------------------------
template <class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store_internal(compressed_storage from, std::memory_order order) noexcept
{
	exchange_internal(from, aspdetail::CAS_FLAG_NONE, order);
}
template <class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::try_help_increment_and_try_swap(compressed_storage & expected, compressed_storage desired, aspdetail::memory_orders orders) noexcept
{
	const compressed_storage initialPtrBlock(expected.myU64 & aspdetail::Versioned_Ptr_Mask);

	aspdetail::control_block_base<T, Allocator>* const controlBlock(to_control_block(expected));

	compressed_storage desired_(desired);
	do {
		const std::uint8_t copyRequests(expected.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST]);

		if (controlBlock)
			controlBlock->incref(copyRequests);

		if (myStorage.compare_exchange_weak(expected.myU64, desired_.myU64, orders.myFirst, std::memory_order_relaxed)) {
			return true;
		}

		if (controlBlock)
			controlBlock->decref(copyRequests);

	} while ((expected.myU64 & aspdetail::Versioned_Ptr_Mask) == initialPtrBlock.myU64);

	std::atomic_thread_fence(orders.mySecond);

	return false;
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::try_help_increment(compressed_storage expected, std::memory_order order) noexcept
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
		const std::uint8_t copyRequests(expected_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST]);

		controlBlock->incref(copyRequests);

		if (myStorage.compare_exchange_weak(expected_.myU64, desired.myU64, std::memory_order_relaxed, order)) {
			return;
		}
		controlBlock->decref(copyRequests);

	} while (
		(expected_.myU64 & aspdetail::Versioned_Ptr_Mask) == initialPtrBlock.myU64 &&
		expected_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST]);

	std::atomic_thread_fence(order);
}
template<class T, class Allocator>
inline constexpr aspdetail::control_block_base<T, Allocator>* atomic_shared_ptr<T, Allocator>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<aspdetail::control_block_base<T, Allocator>*>(from.myU64 & aspdetail::Ptr_Mask);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak_internal(compressed_storage & expected, compressed_storage desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept
{
	bool result(false);

	compressed_storage desired_(desired);
	desired_.myU8[aspdetail::STORAGE_BYTE_VERSION] = expected.myU8[aspdetail::STORAGE_BYTE_VERSION] + 1;
	desired_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] = 0;

	compressed_storage expected_(expected);

	// There are no discernable copy requests, we can just try a fast swap
	if (!expected_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST]) {
		result = compare_exchange_weak_internal_fast_swap(expected_, desired_, flags, orders);
	}

	// There are copy requests, we must go through a incrementation helping process as we try to swap in
	else {
		result = compare_exchange_weak_internal_helping_swap(expected_, desired_, flags, orders);
	}

	const bool otherInterjection((expected_.myU64 ^ expected.myU64) & aspdetail::Versioned_Ptr_Mask);

	expected = expected_;

	if (otherInterjection & (flags & aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE)) {
		expected = copy_internal(orders.mySecond);
	}

	return result;
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak_internal_fast_swap(compressed_storage & expected, const compressed_storage & desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept
{
	const bool result = myStorage.compare_exchange_weak(expected.myU64, desired.myU64, orders.myFirst, std::memory_order_relaxed);

	const bool decrementPrevious(result & !(flags & aspdetail::CAS_FLAG_STEAL_PREVIOUS));

	if (decrementPrevious) {
		aspdetail::control_block_base<T, Allocator>* const controlBlock(to_control_block(expected));
		if (controlBlock) {
			controlBlock->decref();
		}
	}
	return result;
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_weak_internal_helping_swap(compressed_storage & expected, const compressed_storage & desired, aspdetail::CAS_FLAG flags, aspdetail::memory_orders orders) noexcept
{
	bool result(false);

	compressed_storage expected_(expected);

	expected_ = myStorage.fetch_add(aspdetail::Copy_Request_Step, std::memory_order_relaxed);
	expected_.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] += 1;

	aspdetail::control_block_base<T, Allocator>* const controlBlock(to_control_block(expected_));

	const bool otherInterjection((expected_.myU64 ^ expected.myU64) & aspdetail::Versioned_Ptr_Mask);

	if (!otherInterjection) {
		result = try_help_increment_and_try_swap(expected_, desired, orders);
	}
	else {
		try_help_increment(expected_, (flags & aspdetail::CAS_FLAG_CAPTURE_ON_FAILURE) ? std::memory_order_relaxed : orders.mySecond);
	}

	const bool decrementPrevious(result & !(flags & aspdetail::CAS_FLAG_STEAL_PREVIOUS));

	if (controlBlock) {
		controlBlock->decref(1 + decrementPrevious);
	}

	expected = expected_;

	return result;
}
template <class T, class Allocator>
inline typename  aspdetail::compressed_storage atomic_shared_ptr<T, Allocator>::copy_internal(std::memory_order order) noexcept
{
	compressed_storage initial(myStorage.fetch_add(aspdetail::Copy_Request_Step, std::memory_order_relaxed));
	initial.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] += 1;

	if (to_control_block(initial)) {
		compressed_storage expected(initial);
		try_help_increment(expected, order);
	}

	initial.myU8[aspdetail::STORAGE_BYTE_COPYREQUEST] = 0;

	return initial;
}
template <class T, class Allocator>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T, Allocator>::unsafe_copy_internal(std::memory_order order)
{
	std::atomic_thread_fence(std::memory_order_acquire);

	aspdetail::control_block_base<T, Allocator>* const cb(to_control_block(compressed_storage(myStorage.load(std::memory_order_relaxed))));
	if (cb) {
		cb->incref();
	}

	std::atomic_thread_fence(order);

	return compressed_storage(myStorage.load(std::memory_order_relaxed));
}
template<class T, class Allocator>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T, Allocator>::unsafe_exchange_internal(compressed_storage with, std::memory_order order)
{
	std::atomic_thread_fence(std::memory_order_acquire);

	const compressed_storage old(myStorage.load(std::memory_order_relaxed));

	compressed_storage replacement(with.myU64);
	replacement.myU8[aspdetail::STORAGE_BYTE_VERSION] = old.myU8[aspdetail::STORAGE_BYTE_VERSION] + 1;

	myStorage.load(std::memory_order_relaxed) = replacement.myU64;

	std::atomic_thread_fence(order);

	return old;
}
template <class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store_internal(compressed_storage from, std::memory_order order)
{
	std::atomic_thread_fence(std::memory_order_acquire);

	const compressed_storage previous(myStorage.load(std::memory_order_relaxed));
	myStorage.store(from.myU64, std::memory_order_relaxed);

	aspdetail::control_block_base<T, Allocator>* const prevCb(to_control_block(previous));
	if (prevCb) {
		prevCb->decref();
	}
	
	std::atomic_thread_fence(order);
}
template<class T, class Allocator>
inline typename aspdetail::compressed_storage atomic_shared_ptr<T, Allocator>::exchange_internal(compressed_storage to, aspdetail::CAS_FLAG flags, std::memory_order order) noexcept
{
	compressed_storage expected(myStorage.load(std::memory_order_relaxed));
	while (!compare_exchange_weak_internal(expected, to, flags, { std::memory_order_relaxed, order }));
	return expected;
}
namespace aspdetail {
template <class T, class Allocator>
class control_block_base
{
public:
	typedef typename atomic_shared_ptr<T>::size_type size_type;

	control_block_base(T* object, Allocator& allocator);

	T* get_owned() noexcept;
	const T* get_owned() const noexcept;

	size_type use_count() const noexcept;

	void incref(size_type count = 1) noexcept;
	void decref(size_type count = 1) noexcept;

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
inline void control_block_base<T, Allocator>::incref(size_type count) noexcept
{
	myUseCount.fetch_add(count, std::memory_order_relaxed);
}
template<class T, class Allocator>
inline void control_block_base<T, Allocator>::decref(size_type count) noexcept
{
	if (!(myUseCount.fetch_sub(count, std::memory_order_acq_rel) - count))
	{
		destroy();
	}
}
template <class T, class Allocator>
inline T* control_block_base<T, Allocator>::get_owned() noexcept
{
	return myPtr;
}
template<class T, class Allocator>
inline const T* control_block_base<T, Allocator>::get_owned() const noexcept
{
	return myPtr;
}
template <class T, class Allocator>
inline typename control_block_base<T, Allocator>::size_type control_block_base<T, Allocator>::use_count() const noexcept
{
	return myUseCount.load(std::memory_order_relaxed);
}
template <class T, class Allocator>
class control_block_make_shared : public control_block_base<T, Allocator>
{
public:
	template <class ...Args, class U = T, std::enable_if_t<std::is_array<U>::value>* = nullptr>
	control_block_make_shared(Allocator& alloc, Args&& ...args);
	template <class ...Args, class U = T, std::enable_if_t<!std::is_array<U>::value>* = nullptr>
	control_block_make_shared(Allocator& alloc, Args&& ...args);

	void destroy() noexcept override;
private:
	T myOwned;
};
template<class T, class Allocator>
template <class ...Args, class U, std::enable_if_t<std::is_array<U>::value>*>
inline control_block_make_shared<T, Allocator>::control_block_make_shared(Allocator& alloc, Args&& ...args)
	: control_block_base<T, Allocator>(&myOwned, alloc)
	, myOwned{ std::forward<Args&&>(args)... }
{
}
template<class T, class Allocator>
template <class ...Args, class U, std::enable_if_t<!std::is_array<U>::value>*>
inline control_block_make_shared<T, Allocator>::control_block_make_shared(Allocator& alloc, Args&& ...args)
	: control_block_base<T, Allocator>(&myOwned, alloc)
	, myOwned(std::forward<Args&&>(args)...)
{
}
template<class T, class Allocator>
inline void control_block_make_shared<T, Allocator>::destroy() noexcept
{
	Allocator alloc(this->myAllocator);
	(*this).~control_block_make_shared<T, Allocator>();
	alloc.deallocate(reinterpret_cast<std::uint8_t*>(this), shared_ptr<T, Allocator>::alloc_size_make_shared());
}
template <class T, class Allocator>
class control_block_claim : public control_block_base<T, Allocator>
{
public:
	control_block_claim(T* obj, Allocator& alloc) noexcept;
	void destroy() noexcept override;
};
template<class T, class Allocator>
inline control_block_claim<T, Allocator>::control_block_claim(T * obj, Allocator & alloc) noexcept
	:control_block_base<T, Allocator>(obj, alloc)
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
class control_block_claim_custom_delete : public control_block_base<T, Allocator>
{
public:
	control_block_claim_custom_delete(T* obj, Allocator& alloc, Deleter&& deleter) noexcept;
	void destroy() noexcept override;

private:
	Deleter myDeleter;
};
template<class T, class Allocator, class Deleter>
inline control_block_claim_custom_delete<T, Allocator, Deleter>::control_block_claim_custom_delete(T* obj, Allocator& alloc, Deleter && deleter) noexcept
	: control_block_base<T, Allocator>(obj, alloc)
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
	STORAGE_BYTE_COPYREQUEST = Copy_Request_Index,
};
enum CAS_FLAG : std::uint8_t
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
	typedef T value_type;
	typedef Allocator allocator_type;

	inline constexpr ptr_base(std::nullptr_t) noexcept;
	inline constexpr ptr_base(std::nullptr_t, std::uint8_t version) noexcept;

	inline constexpr operator bool() const noexcept;

	inline constexpr bool operator==(const ptr_base<T, Allocator>& other) const noexcept;
	inline constexpr bool operator!=(const ptr_base<T, Allocator>& other) const noexcept;

	inline bool operator==(const atomic_shared_ptr<T, Allocator>& other) const noexcept;
	inline bool operator!=(const atomic_shared_ptr<T, Allocator>& other) const noexcept;

	inline constexpr explicit operator T*() noexcept;
	inline constexpr explicit operator const T*() const noexcept;

	inline constexpr const T* get_owned() const noexcept;
	inline constexpr T* get_owned() noexcept;

	inline constexpr raw_ptr<T, Allocator> get_raw_ptr() const noexcept;

	inline constexpr std::uint8_t get_version() const noexcept;

	inline size_type use_count() const noexcept;

	inline constexpr T* operator->();
	inline constexpr T& operator*();

	inline constexpr const T* operator->() const;
	inline constexpr const T& operator*() const;

	inline const T& operator[](size_type index) const;
	inline T& operator[](size_type index);

protected:
	inline constexpr const control_block_base<T, Allocator>* get_control_block() const noexcept;
	inline constexpr control_block_base<T, Allocator>* get_control_block() noexcept;

	inline constexpr ptr_base() noexcept;
	inline constexpr ptr_base(compressed_storage from) noexcept;

	inline void reset() noexcept;

	constexpr control_block_base<T, Allocator>* to_control_block(compressed_storage from) noexcept;
	constexpr T* to_object(compressed_storage from) noexcept;

	constexpr const control_block_base<T, Allocator>* to_control_block(compressed_storage from) const noexcept;
	constexpr const T* to_object(compressed_storage from) const noexcept;

	friend class atomic_shared_ptr<T, Allocator>;

	compressed_storage myControlBlockStorage;
	T* myPtr;
};
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::ptr_base() noexcept
	: myPtr(nullptr)
{
	static_assert(std::is_same<Allocator::value_type, std::uint8_t>(), "value_type for allocator must be std::uint8_t");
}
template<class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::ptr_base(std::nullptr_t) noexcept
	: ptr_base<T, Allocator>()
{
}
template<class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::ptr_base(std::nullptr_t, std::uint8_t version) noexcept
	: myPtr(nullptr)
	, myControlBlockStorage(0ULL | (std::uint64_t(version) << (STORAGE_BYTE_VERSION * 8)))
{
}
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::ptr_base(compressed_storage from) noexcept
	: ptr_base<T, Allocator>()
{
	myControlBlockStorage = from;
	myControlBlockStorage.myU8[STORAGE_BYTE_COPYREQUEST] = 0;
	myPtr = to_object(from);
}
template<class T, class Allocator>
inline void ptr_base<T, Allocator>::reset() noexcept
{
	myControlBlockStorage = compressed_storage();
	myPtr = nullptr;
}
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::operator bool() const noexcept
{
	return myControlBlockStorage.myU64 & aspdetail::Ptr_Mask;
}
template <class T, class Allocator>
inline constexpr bool ptr_base<T, Allocator>::operator==(const ptr_base<T, Allocator>& other) const noexcept
{
	return !((myControlBlockStorage.myU64 ^ other.myControlBlockStorage.myU64) & aspdetail::Versioned_Ptr_Mask);
}
template <class T, class Allocator>
inline constexpr bool ptr_base<T, Allocator>::operator!=(const ptr_base<T, Allocator>& other) const noexcept
{
	return !operator==(other);
}
template<class T, class Allocator>
inline bool ptr_base<T, Allocator>::operator==(const atomic_shared_ptr<T, Allocator>& other) const noexcept
{
	return other == *this;
}
template<class T, class Allocator>
inline bool ptr_base<T, Allocator>::operator!=(const atomic_shared_ptr<T, Allocator>& other) const noexcept
{
	return !operator==(other);
}
template <class T, class Allocator>
inline typename ptr_base<T, Allocator>::size_type ptr_base<T, Allocator>::use_count() const noexcept
{
	if (!operator bool()) {
		return 0;
	}

	return get_control_block()->use_count();
}
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::operator T*() noexcept
{
	return get_owned();
}
template <class T, class Allocator>
inline constexpr ptr_base<T, Allocator>::operator const T*() const noexcept
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
	return get_owned()[index];
}
template <class T, class Allocator>
inline T & ptr_base<T, Allocator>::operator[](size_type index)
{
	return get_owned()[index];
}
template <class T, class Allocator>
inline constexpr bool operator==(std::nullptr_t /*aNullptr*/, const ptr_base<T, Allocator>& ptr) noexcept
{
	return !ptr;
}
template <class T, class Allocator>
inline constexpr bool operator!=(std::nullptr_t /*aNullptr*/, const ptr_base<T, Allocator>& ptr) noexcept
{
	return ptr;
}
template <class T, class Allocator>
inline constexpr bool operator==(const ptr_base<T, Allocator>& ptr, std::nullptr_t /*aNullptr*/) noexcept
{
	return !ptr;
}
template <class T, class Allocator>
inline constexpr bool operator!=(const ptr_base<T, Allocator>& ptr, std::nullptr_t /*aNullptr*/) noexcept
{
	return ptr;
}
template <class T, class Allocator>
inline constexpr control_block_base<T, Allocator>* ptr_base<T, Allocator>::to_control_block(compressed_storage from) noexcept
{
	return reinterpret_cast<control_block_base<T, Allocator>*>(from.myU64 & Ptr_Mask);
}
template <class T, class Allocator>
inline constexpr T* ptr_base<T, Allocator>::to_object(compressed_storage from) noexcept
{
	control_block_base<T, Allocator>* const cb(to_control_block(from));
	if (cb) {
		return cb->get_owned();
	}
	return nullptr;
}
template <class T, class Allocator>
inline constexpr const control_block_base<T, Allocator>* ptr_base<T, Allocator>::to_control_block(compressed_storage from) const noexcept
{
	return reinterpret_cast<const control_block_base<T, Allocator>*>(from.myU64 & Ptr_Mask);
}
template <class T, class Allocator>
inline constexpr const T* ptr_base<T, Allocator>::to_object(compressed_storage from) const noexcept
{
	control_block_base<T, Allocator>* const cb(to_control_block(from));
	if (cb)
	{
		return cb->get_owned();
	}
	return nullptr;
}
template <class T, class Allocator>
inline constexpr const control_block_base<T, Allocator>* ptr_base<T, Allocator>::get_control_block() const noexcept
{
	return to_control_block(myControlBlockStorage);
}
template <class T, class Allocator>
inline constexpr const T* ptr_base<T, Allocator>::get_owned() const noexcept
{
	return myPtr;
}
template <class T, class Allocator>
inline constexpr control_block_base<T, Allocator>* ptr_base<T, Allocator>::get_control_block() noexcept
{
	return to_control_block(myControlBlockStorage);
}
template <class T, class Allocator>
inline constexpr T* ptr_base<T, Allocator>::get_owned() noexcept
{
	return myPtr;
}
template<class T, class Allocator>
inline constexpr std::uint8_t ptr_base<T, Allocator>::get_version() const noexcept
{
	return myControlBlockStorage.myU8[STORAGE_BYTE_VERSION];
}
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator> ptr_base<T, Allocator>::get_raw_ptr() const noexcept
{
	return raw_ptr<T, Allocator>(myControlBlockStorage);
}
};
template <class T, class Allocator>
class shared_ptr : public aspdetail::ptr_base<T, Allocator>
{
public:
	inline constexpr shared_ptr() = default;

	using aspdetail::ptr_base<T, Allocator>::ptr_base;

	inline shared_ptr(const shared_ptr<T, Allocator>& other) noexcept;
	inline shared_ptr(shared_ptr<T, Allocator>&& other) noexcept;

	inline explicit shared_ptr(T* object);
	inline explicit shared_ptr(T* object, Allocator& allocator);
	template <class Deleter>
	inline explicit shared_ptr(T* object, Deleter&& deleter);
	template <class Deleter>
	inline explicit shared_ptr(T* object, Deleter&& deleter, Allocator& allocator);

	~shared_ptr() noexcept;

	shared_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& other) noexcept;
	shared_ptr<T, Allocator>& operator=(shared_ptr<T, Allocator>&& other) noexcept;

	// The amount of memory requested from the allocator when calling
	// make_shared
	static constexpr std::size_t alloc_size_make_shared() noexcept;

	// The amount of memory requested from the allocator when taking 
	// ownership of an object using default deleter
	static constexpr std::size_t alloc_size_claim() noexcept;

	// The amount of memory requested from the allocator when taking 
	// ownership of an object using a custom deleter
	template <class Deleter>
	static constexpr std::size_t alloc_size_claim_custom_delete() noexcept;

private:
	typedef aspdetail::compressed_storage compressed_storage;

	shared_ptr(compressed_storage from) noexcept;

	template<class Deleter>
	inline compressed_storage create_control_block(T* object, Deleter&& deleter, Allocator& allocator);
	inline compressed_storage create_control_block(T* object, Allocator& allocator);

	friend class raw_ptr<T, Allocator>;
	friend class atomic_shared_ptr<T, Allocator>;

	template <class T, class Allocator, class ...Args>
	friend shared_ptr<T, Allocator> make_shared<T, Allocator>(Allocator&, Args&&...);
};
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::~shared_ptr()
{
	aspdetail::control_block_base<T, Allocator>* const cb(this->get_control_block());
	if (cb) {
		cb->decref();
	}
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(shared_ptr<T, Allocator>&& other) noexcept
	: shared_ptr<T, Allocator>()
{
	operator=(std::move(other));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(const shared_ptr<T, Allocator>& other) noexcept
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
template <class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(T * object, Allocator& allocator)
	: shared_ptr<T, Allocator>()
{
	this->myControlBlockStorage = create_control_block(object, allocator);
	this->myPtr = this->to_object(this->myControlBlockStorage);
}
// The Deleter callable has signature void(T* obj, Allocator& alloc)
template<class T, class Allocator>
template<class Deleter>
inline shared_ptr<T, Allocator>::shared_ptr(T* object, Deleter && deleter)
	: shared_ptr<T, Allocator>()
{
	Allocator alloc;
	this->myControlBlockStorage = create_control_block(object, std::forward<Deleter&&>(deleter), alloc);
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
inline shared_ptr<T, Allocator>::shared_ptr(compressed_storage from) noexcept
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
		allocator.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		deleter(object, allocator);
		throw;
	}

	return compressed_storage(reinterpret_cast<std::uint64_t>(controlBlock));
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
		allocator.deallocate(static_cast<std::uint8_t*>(block), blockSize);
		object->~T();
		allocator.deallocate(reinterpret_cast<std::uint8_t*>(object), sizeof(T));
		throw;
	}

	return compressed_storage(reinterpret_cast<std::uint64_t>(controlBlock));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>& shared_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& other) noexcept
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
inline shared_ptr<T, Allocator>& shared_ptr<T, Allocator>::operator=(shared_ptr<T, Allocator>&& other) noexcept
{
	std::swap(this->myControlBlockStorage, other.myControlBlockStorage);
	std::swap(this->myPtr, other.myPtr);
	return *this;
}
template<class T, class Allocator>
inline constexpr std::size_t shared_ptr<T, Allocator>::alloc_size_make_shared() noexcept
{
	return sizeof(aspdetail::control_block_make_shared<T, Allocator>);
}
template<class T, class Allocator>
inline constexpr std::size_t shared_ptr<T, Allocator>::alloc_size_claim() noexcept
{
	return sizeof(aspdetail::control_block_claim<T, Allocator>);
}
template<class T, class Allocator>
template<class Deleter>
inline constexpr std::size_t shared_ptr<T, Allocator>::alloc_size_claim_custom_delete() noexcept
{
	return sizeof(aspdetail::control_block_claim_custom_delete<T, Allocator, Deleter>);
}
// raw_ptr does not share in ownership of the object
template <class T, class Allocator>
class raw_ptr : public aspdetail::ptr_base<T, Allocator>
{
public:
	inline constexpr raw_ptr() = default;

	using aspdetail::ptr_base<T, Allocator>::ptr_base;

	constexpr raw_ptr(raw_ptr<T, Allocator>&& other) noexcept;
	constexpr raw_ptr(const raw_ptr<T, Allocator>& other) noexcept;

	explicit constexpr raw_ptr(const shared_ptr<T, Allocator>& from) noexcept;

	explicit raw_ptr(const atomic_shared_ptr<T, Allocator>& from) noexcept;

	constexpr raw_ptr<T, Allocator>& operator=(const raw_ptr<T, Allocator>& other) noexcept;
	constexpr raw_ptr<T, Allocator>& operator=(raw_ptr<T, Allocator>&& other) noexcept;
	constexpr raw_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& from) noexcept;
	constexpr raw_ptr<T, Allocator>& operator=(const atomic_shared_ptr<T, Allocator>& from) noexcept;

private:
	typedef aspdetail::compressed_storage compressed_storage;

	explicit constexpr raw_ptr(compressed_storage from) noexcept;

	friend class aspdetail::ptr_base<T, Allocator>;
	friend class atomic_shared_ptr<T, Allocator>;
};
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator>::raw_ptr(raw_ptr<T, Allocator>&& other) noexcept
	: raw_ptr()
{
	operator=(std::move(other));
}
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator>::raw_ptr(const raw_ptr<T, Allocator>& other) noexcept
	: raw_ptr()
{
	operator=(other);
}
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator>::raw_ptr(const shared_ptr<T, Allocator>& from) noexcept
	: aspdetail::ptr_base<T, Allocator>(from.myControlBlockStorage)
{
}
template<class T, class Allocator>
inline raw_ptr<T, Allocator>::raw_ptr(const atomic_shared_ptr<T, Allocator>& from) noexcept
	: aspdetail::ptr_base<T, Allocator>(from.myStorage.load())
{
}
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator>& raw_ptr<T, Allocator>::operator=(const raw_ptr<T, Allocator>& other)  noexcept{
	this->myControlBlockStorage = other.myControlBlockStorage;
	this->myPtr = other.myPtr;

	return *this;
}
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator>& raw_ptr<T, Allocator>::operator=(raw_ptr<T, Allocator>&& other) noexcept {
	std::swap(this->myControlBlockStorage, other.myControlBlockStorage);
	std::swap(this->myPtr, other.myPtr);
	return *this;
}
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator>& raw_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& from) noexcept
{
	*this = raw_ptr<T, Allocator>(from);
	return *this;
}
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator>& raw_ptr<T, Allocator>::operator=(const atomic_shared_ptr<T, Allocator>& from) noexcept
{
	*this = raw_ptr<T, Allocator>(from);
	return *this;
}
template<class T, class Allocator>
inline constexpr raw_ptr<T, Allocator>::raw_ptr(compressed_storage from) noexcept
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
	std::uint8_t* const block(allocator.allocate(shared_ptr<T, Allocator>::alloc_size_make_shared()));

	aspdetail::control_block_make_shared<T, Allocator>* controlBlock(nullptr);
	try {
		controlBlock = new (reinterpret_cast<std::uint8_t*>(block)) aspdetail::control_block_make_shared<T, Allocator>(allocator, std::forward<Args&&>(args)...);
	}
	catch (...) {
		if (controlBlock) {
			(*controlBlock).~control_block_make_shared<T, Allocator>();
		}
		allocator.deallocate(block, shared_ptr<T, Allocator>::alloc_size_make_shared());
		throw;
	}
	return shared_ptr<T, Allocator>(aspdetail::compressed_storage(reinterpret_cast<std::uint64_t>(controlBlock)));
}
};
#pragma warning(pop)