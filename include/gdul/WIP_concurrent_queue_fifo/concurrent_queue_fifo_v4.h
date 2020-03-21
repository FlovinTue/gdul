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

#include <limits>
#include <assert.h>
#include <cmath>
#include <atomic>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/thread_local_member/thread_local_member.h>
#include "../../../Testers/Common/util.h"

// Exception handling may be enabled for basic exception safety at the cost of
// a slight performance decrease


/* #define GDUL_CQ_ENABLE_EXCEPTIONHANDLING */

// In the event an exception is thrown during a pop operation, some entries may
// be dequeued out-of-order as some consumers may already be halfway through a
// pop operation before reintegration efforts are started.

#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
#define GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(type) (std::is_nothrow_move_assignable<type>::value)
#define GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(type) (!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(type) && (std::is_nothrow_copy_assignable<type>::value))
#define GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(type) (std::is_nothrow_move_assignable<type>::value)
#define GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(type) (std::is_nothrow_copy_assignable<type>::value)
#else
#define GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(type) (std::is_move_assignable<type>::value)
#define GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(type) (!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(type))
#define GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(type) (std::is_same<type, type>::value)
#define GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(type) (std::is_same<type, type>::value)
#endif

#if !defined(GDUL_DISABLE_VIEWABLE_ATOMICS)
#ifndef GDUL_ATOMIC_WITH_VIEW
#define GDUL_ATOMIC_WITH_VIEW(type, name) union{std::atomic<type> name; type _##name;}
#endif
#else
#ifndef GDUL_ATOMIC_WITH_VIEW
#define GDUL_ATOMIC_WITH_VIEW(type, name) std::atomic<type> name
#endif
#endif

#ifndef MAKE_UNIQUE_NAME
#define CONCAT(a,b)  a##b
#define EXPAND_AND_CONCAT(a, b) CONCAT(a,b)
#define MAKE_UNIQUE_NAME(prefix) EXPAND_AND_CONCAT(prefix, __COUNTER__)
#endif

#define GDUL_CQ_PADDING(bytes) const std::uint8_t MAKE_UNIQUE_NAME(trash)[bytes] {}

// For anonymous struct and alignas
#pragma warning(push, 2)
#pragma warning(disable : 4201)
#pragma warning(disable : 4324)

#undef min
#undef max

namespace gdul
{

namespace cq_fifo_detail
{
typedef std::size_t size_type;

template <class T, class Allocator>
struct accessor_cache;

template <class T, class Allocator>
class item_buffer;

template <class T>
class item_slot;

template <class T, class Allocator>
class dummy_container;

template <class T, class Allocator>
class buffer_deleter;

enum item_state : std::uint8_t
{
	item_state_empty,
	item_state_valid,
	item_state_failed,
	item_state_dummy
};

template <class Dummy = void>
std::size_t log2_align(std::size_t from);

template <class Dummy = void>
constexpr std::size_t aligned_size(std::size_t byteSize, std::size_t align);

template <class T, class Allocator>
class shared_ptr_allocator_adaptor;

// Not quite size_type max because we need some leaway in case we
// need to throw consumers out of a buffer whilst repairing it
static constexpr size_type Buffer_Capacity_Default = 8;

static constexpr size_type Buffer_Lock_Offset = std::numeric_limits<std::uint16_t>::max();
// Users are assumed to not have more than Buffer_Lock_Threshhold threads
static constexpr size_type Buffer_Lock_Threshhold = Buffer_Lock_Offset / 2;

static constexpr std::uint64_t Ptr_Mask = (std::uint64_t(std::numeric_limits<std::uint32_t>::max()) << 16 | std::uint64_t(std::numeric_limits<std::uint16_t>::max()));
}
// The WizardLoaf MPMC unbounded lock-free queue.
// Basic exception safety may be enabled via define GDUL_CQ_ENABLE_EXCEPTIONHANDLING
// at the price of a slight performance decrease.
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

	inline void reserve(size_type capacity);

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
	friend class cq_fifo_detail::dummy_container<T, allocator_type>;

	using buffer_type = cq_fifo_detail::item_buffer<T, allocator_type>;
	using allocator_adapter_type = cq_fifo_detail::shared_ptr_allocator_adaptor<std::uint8_t, allocator_type>;

	using raw_ptr_buffer_type = raw_ptr<buffer_type>;
	using shared_ptr_buffer_type = shared_ptr<buffer_type>;
	using atomic_shared_ptr_buffer_type = atomic_shared_ptr<buffer_type>;

	template <class ...Arg>
	void push_internal(Arg&&... in);


	inline shared_ptr_buffer_type create_item_buffer(std::size_t withSize);

	inline buffer_type* this_producer_cached();
	inline buffer_type* this_consumer_cached();

	inline bool initialize_consumer();
	inline void initialize_producer();

	inline bool refresh_consumer();
	inline void refresh_producer();

	inline void refresh_cached_consumer();
	inline void refresh_cached_producer();

	tlm<shared_ptr_buffer_type, allocator_type> t_producer;
	tlm<shared_ptr_buffer_type, allocator_type> t_consumer;

	static thread_local struct cache_container { cq_fifo_detail::accessor_cache<T, allocator_type> m_lastConsumer, m_lastProducer; } t_cachedAccesses;

	static cq_fifo_detail::dummy_container<T, allocator_type> s_dummyContainer;

	atomic_shared_ptr_buffer_type m_itemBuffer;

	allocator_type m_allocator;
};

template<class T, class Allocator>
inline concurrent_queue_fifo<T, Allocator>::concurrent_queue_fifo()
	: concurrent_queue_fifo<T, Allocator>(Allocator())
{
}
template<class T, class Allocator>
inline concurrent_queue_fifo<T, Allocator>::concurrent_queue_fifo(Allocator allocator)
	: t_producer(s_dummyContainer.m_dummyBuffer, allocator)
	, t_consumer(s_dummyContainer.m_dummyBuffer, allocator)
	, m_itemBuffer(s_dummyContainer.m_dummyBuffer)
	, m_allocator(allocator)
{
}
template<class T, class Allocator>
inline concurrent_queue_fifo<T, Allocator>::~concurrent_queue_fifo() noexcept
{
	unsafe_reset();
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
template<class ...Arg>
inline void concurrent_queue_fifo<T, Allocator>::push_internal(Arg&& ...in)
{
	while (!this_producer_cached()->try_push(std::forward<Arg>(in)...))
	{
		if (this_producer_cached()->is_valid())
		{
			refresh_producer();
		}
		else
		{
			initialize_producer();
		}
		refresh_cached_producer();
	}
}
template<class T, class Allocator>
bool concurrent_queue_fifo<T, Allocator>::try_pop(T& out)
{
	while (!this_consumer_cached()->try_pop(out)) {
		if (this_consumer_cached()->is_valid()) {
			if (!refresh_consumer()) {
				return false;
			}
		}
		else {
			if (!initialize_consumer()) {
				return false;
			}
		}
		refresh_cached_consumer();
	}
	return true;
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::reserve(typename concurrent_queue_fifo<T, Allocator>::size_type capacity)
{
	const size_type alignedCapacity(cq_fifo_detail::log2_align<>(capacity));

	raw_ptr_buffer_type rawPeek(m_itemBuffer);
	if (rawPeek == s_dummyContainer.m_dummyBuffer)
	{
		m_itemBuffer.compare_exchange_strong(rawPeek, create_item_buffer(alignedCapacity), std::memory_order_release, std::memory_order_relaxed);
	}

	shared_ptr_buffer_type active(nullptr);
	shared_ptr_buffer_type  front(nullptr);

	do
	{
		active = m_itemBuffer.load(std::memory_order_relaxed);
		front = active->find_front();

		if (!front)
		{
			front = active;
		}

		if (!(front->get_capacity() < capacity))
		{
			break;
		}

	} while (!active->try_push_front(create_item_buffer(alignedCapacity)));
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::unsafe_clear()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	buffer_type* const active(m_itemBuffer.unsafe_get());

	if (active->is_valid()){
		active->unsafe_clear();

		shared_ptr_buffer_type front(active->find_front());
		if (front) {
			m_itemBuffer.unsafe_store(std::move(front), std::memory_order_relaxed);
		}
	}

	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::unsafe_reset()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	m_itemBuffer.unsafe_get()->unsafe_invalidate();
	m_itemBuffer.unsafe_store(s_dummyContainer.m_dummyBuffer, std::memory_order_relaxed);

	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::size_type concurrent_queue_fifo<T, Allocator>::size() const
{
	return m_itemBuffer.load(std::memory_order_relaxed)->size();
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::size_type concurrent_queue_fifo<T, Allocator>::unsafe_size() const
{
	return m_itemBuffer.unsafe_get()->size();
}
template<class T, class Allocator>
inline bool concurrent_queue_fifo<T, Allocator>::refresh_consumer()
{
	assert(t_consumer.get()->is_valid() && "refresh_consumer assumes a valid consumer");

	if (m_itemBuffer == s_dummyContainer.m_dummyBuffer)
	{
		return false;
	}

	shared_ptr_buffer_type activeBuffer(m_itemBuffer.load(std::memory_order_relaxed));

	if (activeBuffer != t_consumer.get())
	{
		t_consumer = std::move(activeBuffer);
		return true;
	}

	shared_ptr_buffer_type listBack(activeBuffer->find_back());
	if (listBack)
	{
		if (m_itemBuffer.compare_exchange_strong(activeBuffer, listBack))
		{
			t_consumer = std::move(listBack);
		}
		else
		{
			t_consumer = std::move(activeBuffer);
		}
		return true;
	}

	return false;
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::shared_ptr_buffer_type concurrent_queue_fifo<T, Allocator>::create_item_buffer(std::size_t withSize)
{
	const std::size_t log2size(cq_fifo_detail::log2_align<>(withSize));

	const std::size_t alignOfData(alignof(T));

	const std::size_t bufferByteSize(sizeof(buffer_type));
	const std::size_t dataBlockByteSize(sizeof(cq_fifo_detail::item_slot<T>) * log2size);

	constexpr std::size_t controlBlockByteSize(gdul::sp_claim_size_custom_delete<buffer_type, allocator_adapter_type, cq_fifo_detail::buffer_deleter<buffer_type, allocator_adapter_type>>());

	constexpr std::size_t controlBlockSize(cq_fifo_detail::aligned_size<>(controlBlockByteSize, 8));
	constexpr std::size_t bufferSize(cq_fifo_detail::aligned_size<>(bufferByteSize, 8));
	const std::size_t dataBlockSize(dataBlockByteSize);

	const std::size_t totalBlockSize(controlBlockSize + bufferSize + dataBlockSize + (8 < alignOfData ? alignOfData : 0));

	std::uint8_t* totalBlock(nullptr);

	buffer_type* buffer(nullptr);
	cq_fifo_detail::item_slot<T>* data(nullptr);

	std::size_t constructed(0);

#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	try
	{
#endif
		totalBlock = m_allocator.allocate(totalBlockSize);

		const std::size_t totalBlockBegin(reinterpret_cast<std::size_t>(totalBlock));
		const std::size_t controlBlockBegin(totalBlockBegin);
		const std::size_t bufferBegin(controlBlockBegin + controlBlockSize);

		const std::size_t bufferEnd(bufferBegin + bufferSize);
		const std::size_t dataBeginOffset((bufferEnd % alignOfData ? (alignOfData - (bufferEnd % alignOfData)) : 0));
		const std::size_t dataBegin(bufferEnd + dataBeginOffset);

		const std::size_t bufferOffset(bufferBegin - totalBlockBegin);
		const std::size_t dataOffset(dataBegin - totalBlockBegin);

		// new (addr) (type[n]) is unreliable...
		data = reinterpret_cast<cq_fifo_detail::item_slot<T>*>(totalBlock + dataOffset);
		for (; constructed < log2size; ++constructed)
		{
			cq_fifo_detail::item_slot<T>* const item(&data[constructed]);
			new (item) (cq_fifo_detail::item_slot<T>);
		}

		buffer = new(totalBlock + bufferOffset) buffer_type(static_cast<size_type>(log2size), data);
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	}
	catch (...)
	{
		m_allocator.deallocate(totalBlock, totalBlockSize);
		for (std::size_t i = 0; i < constructed; ++i)
		{
			data[i].~item_slot<T>();
		}
		throw;
	}
#endif

	allocator_adapter_type allocAdaptor(totalBlock, totalBlockSize);

	shared_ptr_buffer_type returnValue(buffer, allocAdaptor, cq_fifo_detail::buffer_deleter<buffer_type, allocator_adapter_type>());

	return returnValue;
}

template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::buffer_type* concurrent_queue_fifo<T, Allocator>::this_producer_cached()
{
	if ((t_cachedAccesses.m_lastProducer.m_addrBlock & cq_fifo_detail::Ptr_Mask) ^ reinterpret_cast<std::uintptr_t>(this))
	{
		refresh_cached_producer();
	}
	return t_cachedAccesses.m_lastProducer.m_buffer;
}

template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::buffer_type* concurrent_queue_fifo<T, Allocator>::this_consumer_cached()
{
	if ((t_cachedAccesses.m_lastConsumer.m_addrBlock & cq_fifo_detail::Ptr_Mask) ^ reinterpret_cast<std::uintptr_t>(this))
	{
		refresh_cached_consumer();
	}
	return t_cachedAccesses.m_lastConsumer.m_buffer;
}

template<class T, class Allocator>
inline bool concurrent_queue_fifo<T, Allocator>::initialize_consumer()
{
	assert(!t_consumer.get()->is_valid() && "initialize_consumer assumes an invalid consumer");

	if (m_itemBuffer == s_dummyContainer.m_dummyBuffer)
	{
		return false;
	}
	t_consumer = m_itemBuffer.load(std::memory_order_relaxed);

	return true;
}

template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::initialize_producer()
{
	if (m_itemBuffer == s_dummyContainer.m_dummyBuffer)
	{
		reserve(cq_fifo_detail::Buffer_Capacity_Default);
	}
	t_producer = m_itemBuffer.load(std::memory_order_relaxed);
}

template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::refresh_producer()
{
	assert(t_producer.get()->is_valid() && "producer needs to be initialized");

	//const buffer_type* const initial(t_producer.get().get());
	//
	//do
	//{
	//	shared_ptr_buffer_type front(t_producer.get()->find_front());
	//
	//	if (!front)
	//	{
	//		front = t_producer.get();
	//	}
	//
	//	if (front.get() == t_producer.get().get())
	//	{
	//		const size_type capacity(front->get_capacity());
	//		const size_type newCapacity(capacity * 2);
	//
	//		reserve(newCapacity);
	//	}
	//	else
	//	{
	//		t_producer = std::move(front);
	//	}
	//
	//} while (t_producer.get().get() == initial);

	t_producer = m_itemBuffer.load(std::memory_order_relaxed);
}

template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::refresh_cached_consumer()
{
	t_cachedAccesses.m_lastConsumer.m_buffer = t_consumer.get().get();
	t_cachedAccesses.m_lastConsumer.m_addr = this;
}

template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::refresh_cached_producer()
{
	t_cachedAccesses.m_lastProducer.m_buffer = t_producer.get().get();
	t_cachedAccesses.m_lastProducer.m_addr = this;
}

namespace cq_fifo_detail
{

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

	item_buffer(size_type capacity, item_slot<T>* dataBlock);
	~item_buffer();

	template<class ...Arg>
	inline bool try_push(Arg&&... in);
	inline bool try_pop(T& out);

	inline size_type size() const;
	inline size_type get_capacity() const;

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

	// Pushes a newly allocated buffer buffer to the front of the
	// buffer list
	inline bool try_push_front(shared_ptr_buffer_type newBuffer);

	// Used to signal that this buffer list is to be discarded
	inline void unsafe_invalidate();
	inline void unsafe_clear();

	inline void block_writers();
	inline void block_readers();

private:
	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)> * = nullptr>
	inline void write_in(size_type slot, U&& in);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)> * = nullptr>
	inline void write_in(size_type slot, U&& in);
	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)> * = nullptr>
	inline void write_in(size_type slot, const U& in);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)> * = nullptr>
	inline void write_in(size_type slot, const U& in);

	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U)> * = nullptr>
	inline void write_out(size_type slot, U& out);
	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)> * = nullptr>
	inline void write_out(size_type slot, U& out);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)> * = nullptr>
	inline void write_out(size_type slot, U& out);

	std::atomic<size_type> m_preReadSync;
	GDUL_CQ_PADDING(64);

	GDUL_ATOMIC_WITH_VIEW(size_type, m_readAt);

	GDUL_CQ_PADDING(64);

	std::atomic<size_type> m_writeAt;
	GDUL_CQ_PADDING(64);
	std::atomic<size_type> m_postWriteSync;
	GDUL_CQ_PADDING(64);

	GDUL_ATOMIC_WITH_VIEW(size_type, m_written);

	GDUL_CQ_PADDING(64);

	atomic_shared_ptr_buffer_type m_next;

	const size_type m_capacity;
	item_slot<T>* const m_items;
};

template<class T, class Allocator>
inline item_buffer<T, Allocator>::item_buffer(typename item_buffer<T, Allocator>::size_type capacity, item_slot<T>* dataBlock)
	: m_next(nullptr)
	, m_items(dataBlock)
	, m_capacity(capacity)
	, m_preReadSync(0)
	, m_readAt(0)
	, m_writeAt(0)
	, m_postWriteSync(0)
	, m_written(0)
{
}

template<class T, class Allocator>
inline item_buffer<T, Allocator>::~item_buffer()
{
	for (size_type i = 0; i < m_capacity; ++i)
	{
		m_items[i].~item_slot<T>();
	}
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_active() const
{
	if (!m_next) {
		return true;
	}

	const size_type written(m_written.load(std::memory_order_acquire));

	if (written != m_capacity) {
		return true;
	}

	const size_type read(m_readAt.load(std::memory_order_relaxed));
	if (read != written) {
		return true;
	}

	return false;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_valid() const
{
	return m_items[m_writeAt.load(std::memory_order_relaxed) % m_capacity].get_state() != item_state_dummy;
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::unsafe_invalidate()
{
	block_readers();
	block_writers();

	m_items[m_writeAt.load(std::memory_order_relaxed) % m_capacity].set_state(item_state_dummy);

	if (m_next)
	{
		m_next.unsafe_get()->unsafe_invalidate();
	}
}
template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::shared_ptr_buffer_type item_buffer<T, Allocator>::find_front()
{
	shared_ptr_buffer_type front(nullptr);
	item_buffer<T, allocator_type>* inspect(this);

	while (inspect->m_next)
	{
		front = inspect->m_next.load(std::memory_order_relaxed);
		inspect = front.get();
	}
	return front;
}
template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::shared_ptr_buffer_type item_buffer<T, Allocator>::find_back()
{
	shared_ptr_buffer_type back(nullptr);
	item_buffer<T, allocator_type>* inspect(this);

	do {
		if (inspect->is_active()) {
			break;
		}
		back = inspect->m_next.load(std::memory_order_seq_cst);
		inspect = back.get();
	} while (back);

	return back;
}

template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::size_type item_buffer<T, Allocator>::size() const
{
	const size_type readSlot(m_readAt.load(std::memory_order_relaxed));
	const size_type read(!(m_capacity < readSlot) ? readSlot : m_capacity);
	size_type accumulatedSize(m_written.load(std::memory_order_relaxed));
	accumulatedSize -= read;

	if (m_next)
	{
		accumulatedSize += m_next.unsafe_get()->size();
	}
	return accumulatedSize;
}

template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::size_type item_buffer<T, Allocator>::get_capacity() const
{
	return m_capacity;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::try_push_front(shared_ptr_buffer_type newBuffer)
{
	item_buffer<T, allocator_type>* last(this);

	while (last->m_next) {
		last = last->m_next.unsafe_get();
	}

	if (last->m_writeAt.load(std::memory_order_relaxed) < m_capacity) {
		return false;
	}

	last->block_writers();

	raw_ptr_buffer_type exp(nullptr);
	return last->m_next.compare_exchange_strong(exp, std::move(newBuffer), std::memory_order_release, std::memory_order_relaxed);
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::unsafe_clear()
{
	m_writeAt.store(0, std::memory_order_relaxed);
	m_readAt.store(0, std::memory_order_relaxed);

	m_written.store(0, std::memory_order_relaxed);

	m_preReadSync.store(0, std::memory_order_relaxed);
	m_postWriteSync.store(0, std::memory_order_relaxed);

	for (size_type i = 0; i < m_capacity; ++i)
	{
		m_items[i].set_state(item_state_empty);
	}

	if (m_next)
	{
		m_next.unsafe_get()->unsafe_clear();
	}
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::block_writers()
{
	m_writeAt.store(m_capacity, std::memory_order_relaxed);
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::block_readers()
{
	m_readAt.store(m_capacity + std::numeric_limits<std::uint16_t>::max(), std::memory_order_relaxed);
}
template<class T, class Allocator>
template<class ...Arg>
inline bool item_buffer<T, Allocator>::try_push(Arg&& ...in)
{
	const size_type at(m_writeAt.fetch_add(1, std::memory_order_relaxed));

	if (!(at < m_capacity)) {
		return false;
	}

	write_in(at, std::forward<Arg>(in)...);

	const size_type postSync(m_postWriteSync.fetch_add(1, std::memory_order_acq_rel));
	const size_type postAt(m_writeAt.load(std::memory_order_relaxed));
	if ((postSync + 1) == postAt) {
		size_type xchg(postAt);
		size_type target(0);
		do {
			target = xchg;
			xchg = m_written.exchange(target, std::memory_order_release);
		} while (target < xchg);
	}
	//m_written.fetch_add(1, std::memory_order_release);

	return true;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::try_pop(T& out)
{
	const size_type written(m_written.load(std::memory_order_acquire));
	const size_type preReadSync(m_preReadSync.fetch_add(1, std::memory_order_relaxed));

	if (!(preReadSync < written)) {
		m_preReadSync.fetch_sub(1, std::memory_order_relaxed);
		return false;
	}

	const size_type at(m_readAt.fetch_add(1, std::memory_order_relaxed));

	write_out(at, out);

	return true;
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)>*>
inline void item_buffer<T, Allocator>::write_in(typename item_buffer<T, Allocator>::size_type slot, U&& in)
{
	m_items[slot].store(std::move(in));
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)>*>
inline void item_buffer<T, Allocator>::write_in(typename item_buffer<T, Allocator>::size_type slot, U&& in)
{
	try
	{
		m_items[slot].store(std::move(in));
	}
	catch (...)
	{
		--m_writeAt;
		throw;
	}
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::write_in(typename item_buffer<T, Allocator>::size_type slot, const U& in)
{
	m_items[slot].store(in);
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::write_in(typename item_buffer<T, Allocator>::size_type slot, const U& in)
{
	try
	{
		m_items[slot].store(in);
	}
	catch (...)
	{
		--m_writeAt;
		throw;
	}
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U)>*>
inline void item_buffer<T, Allocator>::write_out(typename item_buffer<T, Allocator>::size_type slot, U& out)
{
	m_items[slot].move(out);
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::write_out(typename item_buffer<T, Allocator>::size_type slot, U& out)
{
	m_items[slot].assign(out);
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::write_out(typename item_buffer<T, Allocator>::size_type slot, U& out)
{
	m_items[slot].try_move(out);
}
template <class T>
class item_slot
{
public:
	item_slot<T>(const item_slot<T>&) = delete;
	item_slot<T>& operator=(const item_slot&) = delete;

	inline item_slot();
	inline item_slot(item_state state);

	inline void store(const T& in);
	inline void store(T&& in);

	template<class U = T, std::enable_if_t<std::is_move_assignable<U>::value> * = nullptr>
	inline void try_move(U& out);
	template<class U = T, std::enable_if_t<!std::is_move_assignable<U>::value> * = nullptr>
	inline void try_move(U& out);

	inline void assign(T& out);
	inline void move(T& out);

	inline item_state get_state() const;
	inline void set_state(item_state state);

private:
	T m_data;
	item_state m_state;
};
template<class T>
inline item_slot<T>::item_slot()
	: m_data()
	, m_state(item_state_empty)
{
}
template<class T>
inline item_slot<T>::item_slot(item_state state)
	: m_data()
	, m_state(state)
{
}
template<class T>
inline void item_slot<T>::store(const T& in)
{
	m_data = in;
}
template<class T>
inline void item_slot<T>::store(T&& in)
{
	m_data = std::move(in);
}
template<class T>
inline void item_slot<T>::assign(T& out)
{
	out = m_data;
}
template<class T>
inline void item_slot<T>::move(T& out)
{
	out = std::move(m_data);
}
template<class T>
inline void item_slot<T>::set_state(item_state state)
{
	m_state = state;
}
template<class T>
inline item_state item_slot<T>::get_state() const
{
	return m_state;
}
template<class T>
template<class U, std::enable_if_t<std::is_move_assignable<U>::value>*>
inline void item_slot<T>::try_move(U& out)
{
	out = std::move(m_data);
}
template<class T>
template<class U, std::enable_if_t<!std::is_move_assignable<U>::value>*>
inline void item_slot<T>::try_move(U& out)
{
	out = m_data;
}
template <class Dummy>
std::size_t log2_align(std::size_t from)
{
	const std::size_t from_(from < 2 ? 2 : from);

	const float flog2(std::log2f(static_cast<float>(from_)));
	const float nextLog2(std::ceil(flog2));
	const float fNextVal(powf(2.f, nextLog2));

	const std::size_t nextVal(static_cast<size_t>(fNextVal));

	return nextVal;
}
template <class Dummy>
constexpr std::size_t aligned_size(std::size_t byteSize, std::size_t align)
{
	const std::size_t div(byteSize / align);
	const std::size_t mod(byteSize % align);
	const std::size_t total(div + static_cast<std::size_t>(static_cast<bool>(mod)));

	return align * total;
}
template <class T, class Allocator>
class shared_ptr_allocator_adaptor : public Allocator
{
public:
	template <typename U>
	struct rebind
	{
		using other = shared_ptr_allocator_adaptor<U, Allocator>;
	};

	template <class U>
	shared_ptr_allocator_adaptor(const shared_ptr_allocator_adaptor<U, Allocator>& other)
		: m_address(other.m_address)
		, m_size(other.m_size)
	{
	}

	shared_ptr_allocator_adaptor()
		: m_address(nullptr)
		, m_size(0)
	{
	}
	shared_ptr_allocator_adaptor(T* retAddr, std::size_t size)
		: m_address(retAddr)
		, m_size(size)
	{
	}

	T* allocate(std::size_t count)
	{
		if (!m_address)
		{
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
struct accessor_cache
{
	item_buffer<T, Allocator>* m_buffer;

	union
	{
		void* m_addr;
		std::uint64_t m_addrBlock;
		struct
		{
			std::uint16_t padding[3];
			std::uint16_t m_counter;
		};
	};
};
template <class T, class Allocator>
class dummy_container
{
public:
	dummy_container();
	using shared_ptr_buffer_type = typename concurrent_queue_fifo<T, Allocator>::shared_ptr_buffer_type;
	using allocator_adapter_type = typename concurrent_queue_fifo<T, Allocator>::allocator_adapter_type;
	using buffer_type = typename concurrent_queue_fifo<T, Allocator>::buffer_type;
	using allocator_type = typename concurrent_queue_fifo<T, Allocator>::allocator_type;
	~dummy_container()
	{

	}

	shared_ptr_buffer_type m_dummyBuffer;
	item_slot<T> m_dummyItem;
	buffer_type m_dummyRawBuffer;
};
template<class T, class Allocator>
inline dummy_container<T, Allocator>::dummy_container()
	: m_dummyItem(item_state_dummy)
	, m_dummyRawBuffer(1, &m_dummyItem)
{
	m_dummyRawBuffer.block_writers();

	Allocator alloc;
	m_dummyBuffer = shared_ptr_buffer_type(&m_dummyRawBuffer, alloc, [](buffer_type*, Allocator&) {});
}
template <class T, class Allocator>
class buffer_deleter
{
public:
	void operator()(T* obj, Allocator& /*alloc*/);
};
template<class T, class Allocator>
inline void buffer_deleter<T, Allocator>::operator()(T* obj, Allocator&)
{
	(*obj).~T();
}
}
template <class T, class Allocator>
cq_fifo_detail::dummy_container<T, typename concurrent_queue_fifo<T, Allocator>::allocator_type> concurrent_queue_fifo<T, Allocator>::s_dummyContainer;
template <class T, class Allocator>
thread_local typename concurrent_queue_fifo<T, Allocator>::cache_container concurrent_queue_fifo<T, Allocator>::t_cachedAccesses{ {&concurrent_queue_fifo<T, Allocator>::s_dummyContainer.m_dummyRawBuffer, nullptr}, {&concurrent_queue_fifo<T, Allocator>::s_dummyContainer.m_dummyRawBuffer, nullptr} };
}
#pragma warning(pop)
