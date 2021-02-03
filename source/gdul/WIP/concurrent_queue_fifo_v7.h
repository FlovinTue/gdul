//Copyright(c) 2019 Flovin Michaelsen
//
//Permission is hereby granted, free of charge, to any person obtining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#pragma once

#include <gdul/memory/atomic_shared_ptr.h>
#include <gdul/memory/thread_local_member.h>
#include <gdul/math/math.h>

#include <limits>
#include <assert.h>
#include <cmath>
#include <atomic>
#include <thread>
#include "../../../Testers/Common/util.h"

#ifndef GDUL_ATOMIC_WITH_VIEW
#define GDUL_ATOMIC_WITH_VIEW(type, name) union{std::atomic<type> name; type _##name;}
#else
#define GDUL_ATOMIC_WITH_VIEW(type, name) std::atomic<type> name
#endif

#ifndef MAKE_UNIQUE_NAME
#define CONCAT(a,b)  a##b
#define EXPAND_AND_CONCAT(a, b) CONCAT(a,b)
#define MAKE_UNIQUE_NAME(prefix) EXPAND_AND_CONCAT(prefix, __LINE__)
#endif

#define GDUL_CACHE_PAD const std::uint8_t MAKE_UNIQUE_NAME(trash)[std::hardware_destructive_interference_size] {}

// For anonymous struct and alignas
#pragma warning(push, 2)
#pragma warning(disable : 4201)
#pragma warning(disable : 4324)

namespace gdul {

namespace cq_fifo_detail {
typedef std::size_t size_type;

template <class T, class Allocator>
class item_buffer;

template <class T, class Allocator>
class buffer_deleter;

template <class T, class Allocator>
class shared_ptr_allocator_adapter;

constexpr size_type Buffer_Capacity_Default = 8;

}
// MPMC unbounded lock-free queue.
template <class T, class Allocator = std::allocator<std::uint8_t>>
class concurrent_queue_fifo
{
public:
	typedef cq_fifo_detail::size_type size_type;

	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	inline concurrent_queue_fifo();
	inline concurrent_queue_fifo(Allocator allocator);
	inline ~concurrent_queue_fifo() noexcept;

	inline void push(const T& in);
	inline void push(T&& in);

	bool try_pop(T& out);

	inline void unsafe_reserve(size_type capacity);

	// size hint
	inline size_type size() const;

	// fast unsafe size hint
	inline size_type unsafe_size() const;

	// logically remove entries
	void unsafe_clear();

	// reset the structure to its initial state
	void unsafe_reset();

private:
	friend class cq_fifo_detail::item_buffer<T, allocator_type>;

	using buffer_type = cq_fifo_detail::item_buffer<T, allocator_type>;
	using allocator_adapter_type = cq_fifo_detail::shared_ptr_allocator_adapter<std::uint8_t, allocator_type>;

	using raw_ptr_buffer_type = raw_ptr<buffer_type>;
	using shared_ptr_buffer_type = shared_ptr<buffer_type>;
	using atomic_shared_ptr_buffer_type = atomic_shared_ptr<buffer_type>;

	template <class In>
	void push_internal(In&& in);

	static inline shared_ptr_buffer_type create_item_buffer(std::size_t withSize, allocator_type allocator);

	inline void initialize_producer();

	inline bool refresh_consumer();
	inline void refresh_producer();

	allocator_type m_allocator;

	// Maintained by consumers
	atomic_shared_ptr_buffer_type m_back;

	// Maintained by producers
	atomic_shared_ptr_buffer_type m_front;

	tlm<shared_ptr_buffer_type, allocator_type> t_producer;
	tlm<shared_ptr_buffer_type, allocator_type> t_consumer;
};

template<class T, class Allocator>
inline concurrent_queue_fifo<T, Allocator>::concurrent_queue_fifo()
	: concurrent_queue_fifo<T, Allocator>(Allocator())
{
}
template<class T, class Allocator>
inline concurrent_queue_fifo<T, Allocator>::concurrent_queue_fifo(Allocator allocator)
	: m_back(create_item_buffer(cq_fifo_detail::Buffer_Capacity_Default, allocator))
	, m_front(m_back.load(std::memory_order_relaxed))
	, t_producer(allocator, m_front.load(std::memory_order_relaxed))
	, t_consumer(allocator, m_back.load(std::memory_order_relaxed))
	, m_allocator(allocator)
{
	static_assert(std::is_default_constructible_v<T>, "Type must be default constructible");
}
template<class T, class Allocator>
inline concurrent_queue_fifo<T, Allocator>::~concurrent_queue_fifo() noexcept
{
}
template<class T, class Allocator>
void concurrent_queue_fifo<T, Allocator>::push(const T& in)
{
	push_internal<const T&>(in);
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::push(T&& in)
{
	push_internal<T&&>(std::move(in));
}
template<class T, class Allocator>
template<class In>
inline void concurrent_queue_fifo<T, Allocator>::push_internal(In&& in)
{
	while (!t_producer.get()->try_push(std::forward<In>(in))) {
		if (t_producer.get()->is_valid()) {
			refresh_producer();
		}
		else {
			initialize_producer();
		}
	}
}
template<class T, class Allocator>
bool concurrent_queue_fifo<T, Allocator>::try_pop(T& out)
{
	while (!t_consumer.get()->try_pop(out)) {
		if (!refresh_consumer()) {
			return false;
		}
	}
	return true;
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::unsafe_reserve(typename concurrent_queue_fifo<T, Allocator>::size_type capacity)
{
	// For testing purposes
	return; capacity;


	//raw_ptr_buffer_type front(m_front);
	//
	//if (!(front->capacity() < capacity)){
	//	return;
	//}
	//
	//m_back.unsafe_load(std::memory_order_relaxed)->unsafe_invalidate();
	//
	//m_back.unsafe_store(create_item_buffer(capacity, m_allocator));
	//m_front.unsafe_store(m_back.unsafe_load(std::memory_order_relaxed));
	//
	//std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::unsafe_clear()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	buffer_type* const active(m_back.unsafe_get());

	if (active->is_valid()) {
		active->unsafe_clear();

		shared_ptr_buffer_type front(active->find_front());
		if (front) {
			m_back.unsafe_store(std::move(front), std::memory_order_relaxed);
		}
	}

	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::unsafe_reset()
{
	m_back.unsafe_get()->unsafe_invalidate();
	m_back.unsafe_store(create_item_buffer(cq_fifo_detail::Buffer_Capacity_Default, m_allocator), std::memory_order_relaxed);
	m_front.unsafe_store(m_back.unsafe_load(std::memory_order_relaxed));

	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::size_type concurrent_queue_fifo<T, Allocator>::size() const
{
	return m_back.load(std::memory_order_relaxed)->size();
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::size_type concurrent_queue_fifo<T, Allocator>::unsafe_size() const
{
	return m_back.unsafe_get()->size();
}
template<class T, class Allocator>
inline bool concurrent_queue_fifo<T, Allocator>::refresh_consumer()
{
	shared_ptr_buffer_type& active(t_consumer.get());
	raw_ptr_buffer_type globalBack(m_back);

	std::atomic_thread_fence(std::memory_order_acquire);

	if (active != globalBack) {
		active = m_back.load(std::memory_order_relaxed);
		return true;
	}

	shared_ptr_buffer_type back(active->find_back());

	if (!back) {
		return false;
	}

	active = back;

	m_back.compare_exchange_strong(globalBack, std::move(back), std::memory_order_release);

	return true;
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::shared_ptr_buffer_type concurrent_queue_fifo<T, Allocator>::create_item_buffer(std::size_t withSize, allocator_type allocator)
{
	const std::size_t pow2size(align_value_pow2(withSize));

	constexpr std::size_t alignOfData(alignof(T));

	constexpr std::size_t bufferByteSize(sizeof(buffer_type));
	const std::size_t dataBlockByteSize(sizeof(T) * pow2size);

	constexpr std::size_t controlBlockByteSize(gdul::sp_claim_size_custom_delete<buffer_type, allocator_adapter_type, cq_fifo_detail::buffer_deleter<buffer_type, allocator_adapter_type>>());

	constexpr std::size_t controlBlockSize(align_value(controlBlockByteSize, 8));
	constexpr std::size_t bufferSize(align_value(bufferByteSize, 8));
	const std::size_t dataBlockSize(dataBlockByteSize);

	const std::size_t totalBlockSize(controlBlockSize + bufferSize + dataBlockSize + (8 < alignOfData ? alignOfData : 0));

	std::uint8_t* totalBlock(nullptr);

	buffer_type* buffer(nullptr);
	T* data(nullptr);

	totalBlock = allocator.allocate(totalBlockSize);

	const std::size_t totalBlockBegin(reinterpret_cast<std::size_t>(totalBlock));
	const std::size_t controlBlockBegin(totalBlockBegin);
	const std::size_t bufferBegin(controlBlockBegin + controlBlockSize);

	const std::size_t bufferEnd(bufferBegin + bufferSize);
	const std::size_t dataBeginOffset((bufferEnd % alignOfData ? (alignOfData - (bufferEnd % alignOfData)) : 0));
	const std::size_t dataBegin(bufferEnd + dataBeginOffset);

	const std::size_t bufferOffset(bufferBegin - totalBlockBegin);
	const std::size_t dataOffset(dataBegin - totalBlockBegin);

	data = reinterpret_cast<T*>(totalBlock + dataOffset);

	std::uninitialized_default_construct(data, data + pow2size);

	buffer = new(totalBlock + bufferOffset) buffer_type(data, static_cast<size_type>(pow2size));

	const allocator_adapter_type allocAdaptor(totalBlock, totalBlockSize);

	shared_ptr_buffer_type returnValue(buffer, allocAdaptor, cq_fifo_detail::buffer_deleter<buffer_type, allocator_adapter_type>());

	return returnValue;
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::initialize_producer()
{
	t_producer = m_front.load(std::memory_order_acquire);
}

template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::refresh_producer()
{
	assert(t_producer.get()->is_valid() && "producer needs to be initialized");

	shared_ptr_buffer_type& active(t_producer);
	raw_ptr_buffer_type globalFront(m_front);

	if (active != globalFront) {
		active = m_front.load(std::memory_order_acquire);
	}
	else {
		shared_ptr_buffer_type next(active->find_front());

		if (!next) {
			shared_ptr_buffer_type desired(create_item_buffer(active->capacity() * 2, m_allocator));
			if (active->compare_exchange_next(next, desired)) {
				next = std::move(desired);
			}
		}

		active = next;

		m_front.compare_exchange_strong(globalFront, std::move(next), std::memory_order_release);
	}
}
namespace cq_fifo_detail {
enum buffer_state : std::uint8_t
{
	buffer_state_valid,
	buffer_state_invalid,
};
template <class T, class Allocator>
class item_buffer
{
private:
	using atomic_shared_ptr_buffer_type = typename concurrent_queue_fifo<T, Allocator>::atomic_shared_ptr_buffer_type;
	using shared_ptr_buffer_type = typename concurrent_queue_fifo<T, Allocator>::shared_ptr_buffer_type;
	using raw_ptr_buffer_type = typename concurrent_queue_fifo<T, Allocator>::raw_ptr_buffer_type;
public:
	typedef typename concurrent_queue_fifo<T, Allocator>::size_type size_type;
	typedef typename concurrent_queue_fifo<T, Allocator>::allocator_type allocator_type;
	typedef item_buffer<T, Allocator> buffer_type;

	item_buffer(T* dataBlock, typename item_buffer<T, Allocator>::size_type capacity);
	~item_buffer();

	template<class In>
	inline bool try_push(In&& in);
	inline bool try_pop(T& out);

	inline size_type size() const;
	inline size_type capacity() const;

	// Contains entries and / or has no next buffer
	inline bool is_active() const;

	// Is this a dummybuffer or one from a destroyed structure?
	inline bool is_valid() const;

	// Searches the buffer list towards the front for
	// the front-most buffer
	inline shared_ptr_buffer_type find_front();

	// Searches the buffer list towards the front for
	// the first buffer contining entries
	inline shared_ptr_buffer_type find_back();

	// Try push a buffer to the front of buffer list. Returns value after attempt
	inline bool compare_exchange_next(shared_ptr_buffer_type& expected, shared_ptr_buffer_type desired);

	// Used to signal that this buffer list is to be discarded
	inline void unsafe_invalidate();
	inline void unsafe_clear();

private:

	std::atomic<size_type> m_preReadSync;
	GDUL_CACHE_PAD;
	GDUL_ATOMIC_WITH_VIEW(size_type, m_readAt);
	GDUL_CACHE_PAD;
	std::atomic<size_type> m_writeAt;
	GDUL_CACHE_PAD;
	GDUL_ATOMIC_WITH_VIEW(size_type, m_postWriteSync);
	GDUL_CACHE_PAD;
	GDUL_ATOMIC_WITH_VIEW(size_type, m_written);
	GDUL_CACHE_PAD;

	atomic_shared_ptr_buffer_type m_next;

	const size_type m_capacity;

	T* const m_items;

	std::atomic<buffer_state> m_state;
};

template<class T, class Allocator>
inline item_buffer<T, Allocator>::item_buffer(T* dataBlock, typename item_buffer<T, Allocator>::size_type capacity)
	: m_preReadSync(0)
	, m_readAt(0)
	, m_writeAt(0)
	, m_postWriteSync(0)
	, m_written(0)
	, m_next(nullptr)
	, m_capacity(capacity)
	, m_items(dataBlock)
	, m_state(buffer_state_valid)
{
}

template<class T, class Allocator>
inline item_buffer<T, Allocator>::~item_buffer()
{
	for (size_type i = 0; i < m_capacity; ++i) {
		m_items[i].~T();
	}

	if (m_readAt) {
		assert(m_written == m_readAt);
	}
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_active() const
{
	if (!m_next) {
		return true;
	}


	// Is this really so ? No
	//const size_type writeSlot(m_writeAt.load(std::memory_order_relaxed));

	//if (writeSlot < m_capacity) {
	//	return true;
	//}

	//const size_type readSync(m_preReadSync.load(std::memory_order_relaxed));

	//if (readSync < m_capacity) {
	//	return true;
	//}

	if (m_readAt.load(std::memory_order_acquire) < m_capacity) {
		return true;
	}


	return false;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_valid() const
{
	return m_state.load(std::memory_order_acquire) != buffer_state_invalid;
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::unsafe_invalidate()
{
	m_state.store(buffer_state_invalid, std::memory_order_relaxed);
	m_writeAt.store(m_capacity, std::memory_order_relaxed);

	if (m_next) {
		m_next.unsafe_get()->unsafe_invalidate();
	}
}
template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::shared_ptr_buffer_type item_buffer<T, Allocator>::find_front()
{
	item_buffer<T, allocator_type>* inspect(this);

	while (item_buffer<T, allocator_type>* next = inspect->m_next.unsafe_get()) {
		if (!next->m_next)
			break;
		inspect = next;
	}
	return inspect->m_next.load(std::memory_order_relaxed);
}
template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::shared_ptr_buffer_type item_buffer<T, Allocator>::find_back()
{
	item_buffer<T, allocator_type>* inspect(this);

	if (inspect->is_active())
		return shared_ptr_buffer_type(nullptr);

	while (item_buffer<T, allocator_type>* next = inspect->m_next.unsafe_get()) {
		if (next->is_active())
			return inspect->m_next.load(std::memory_order_acquire);
		inspect = next;
	}
	return shared_ptr_buffer_type(nullptr);
}
template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::size_type item_buffer<T, Allocator>::size() const
{
	const size_type readSlot(m_readAt.load(std::memory_order_relaxed));
	const size_type read(!(m_capacity < readSlot) ? readSlot : m_capacity);
	size_type accumulatedSize(m_written.load(std::memory_order_relaxed));
	accumulatedSize -= read;

	if (m_next) {
		accumulatedSize += m_next.unsafe_get()->size();
	}
	return accumulatedSize;
}

template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::size_type item_buffer<T, Allocator>::capacity() const
{
	return m_capacity;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::compare_exchange_next(shared_ptr_buffer_type& expected, shared_ptr_buffer_type desired)
{
	return m_next.compare_exchange_strong(expected, std::move(desired), std::memory_order_release, std::memory_order_acquire);
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::unsafe_clear()
{
	m_preReadSync.store(0, std::memory_order_relaxed);

	m_readAt.store(0, std::memory_order_relaxed);
	m_writeAt.store(0, std::memory_order_relaxed);

	m_postWriteSync.store(0, std::memory_order_relaxed);

	m_written.store(0, std::memory_order_relaxed);

	if (m_next) {
		m_next.unsafe_get()->unsafe_clear();
	}
}
template<class T, class Allocator>
template<class In>
inline bool item_buffer<T, Allocator>::try_push(In&& in)
{
	size_type at(m_writeAt.fetch_add(1, std::memory_order_relaxed));

	if (!(at < m_capacity)) {
		return false;
	}

	m_items[at] = std::forward<In>(in);

	const size_type postSync(m_postWriteSync.fetch_add(1, std::memory_order_acq_rel));
	const size_type postAt(std::clamp<size_type>(m_writeAt.load(std::memory_order_relaxed), 0, m_capacity));

	if ((postSync + 1) == postAt) {
		size_type xchg(postAt);
		size_type target;
		do {
			target = xchg;
			xchg = m_written.exchange(target, std::memory_order_relaxed);
		} while (target < xchg);
	}

	return true;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::try_pop(T& out)
{
	const size_type written(m_written.load(std::memory_order_relaxed));
	
	std::atomic_thread_fence(std::memory_order_acquire);
	
	const size_type preReadSync(m_preReadSync.fetch_add(1, std::memory_order_relaxed));
	
	if (!(preReadSync < written)) {
		m_preReadSync.fetch_sub(1, std::memory_order_relaxed);
		return false;
	}
	
	const size_type at(m_readAt.fetch_add(1, std::memory_order_relaxed));

	//size_type at(m_readAt.load(std::memory_order_relaxed));
	//for(;;) {

	//	const size_type written = m_written.load(std::memory_order_relaxed);
	//	std::atomic_thread_fence(std::memory_order_acquire);

	//	if (!(at < written)) {
	//		return false;
	//	}

	//	if (m_readAt.compare_exchange_weak(at, at + 1, std::memory_order_relaxed)) {
	//		break;
	//	}
	//};

	out = std::move(m_items[at]);

	return true;
}
template <class T, class Allocator>
class shared_ptr_allocator_adapter : public Allocator
{
public:
	template <typename U>
	struct rebind
	{
		using other = shared_ptr_allocator_adapter<U, Allocator>;
	};

	template <class U>
	shared_ptr_allocator_adapter(const shared_ptr_allocator_adapter<U, Allocator>& other)
		: m_address(other.m_address)
		, m_size(other.m_size)
	{
	}

	shared_ptr_allocator_adapter()
		: m_address(nullptr)
		, m_size(0)
	{
	}
	shared_ptr_allocator_adapter(T* retAddr, std::size_t size)
		: m_address(retAddr)
		, m_size(size)
	{
	}

	T* allocate(std::size_t count)
	{
		if (!m_address) {
			m_address = this->Allocator::allocate(count);
			m_size = count;
		}
		return m_address;
	}

	void deallocate(T* /*addr*/, std::size_t /*count*/)
	{
		T* const addr(m_address);
		std::size_t size(m_size);

		m_address = nullptr;
		m_size = 0;

		Allocator::deallocate(addr, size);
	}

private:
	T* m_address;
	std::size_t m_size;
};
template <class T, class Allocator>
class dummy_container
{
public:
	dummy_container();
	~dummy_container() = default;

	using shared_ptr_buffer_type = typename concurrent_queue_fifo<T, Allocator>::shared_ptr_buffer_type;
	using allocator_adapter_type = typename concurrent_queue_fifo<T, Allocator>::allocator_adapter_type;
	using buffer_type = typename concurrent_queue_fifo<T, Allocator>::buffer_type;
	using allocator_type = typename concurrent_queue_fifo<T, Allocator>::allocator_type;

	shared_ptr_buffer_type m_dummyBuffer;
	T m_dummyItem;
	buffer_type m_dummyRawBuffer;
};
template<class T, class Allocator>
inline dummy_container<T, Allocator>::dummy_container()
	: m_dummyItem()
	, m_dummyRawBuffer(&m_dummyItem, 1)
{
	m_dummyRawBuffer.unsafe_invalidate();

	Allocator alloc;
	m_dummyBuffer = shared_ptr_buffer_type(&m_dummyRawBuffer, alloc, [](buffer_type*, Allocator&) {});
}
template <class T, class Allocator>
class buffer_deleter
{
public:
	void operator()(T* obj, Allocator& /*alloc*/) noexcept;
};
template<class T, class Allocator>
inline void buffer_deleter<T, Allocator>::operator()(T* obj, Allocator&) noexcept
{
	std::destroy_at(obj);
}
}
}
#pragma warning(pop)
