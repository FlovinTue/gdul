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
#define GDUL_CACHELINE_SIZE 64u

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
	while (!this_producer_cached()->try_push(std::forward<Arg>(in)...)){
		if (this_producer_cached()->is_valid()){
			refresh_producer();
		}
		else{
			initialize_producer();
		}
		refresh_cached_producer();
	}
}
template<class T, class Allocator>
bool concurrent_queue_fifo<T, Allocator>::try_pop(T& out)
{
	while (!this_consumer_cached()->try_pop(out)){
		if (this_consumer_cached()->is_valid()){
			if (!refresh_consumer()){
				return false;
			}
		}
		else{
			if (!initialize_consumer()){
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
	if (rawPeek == s_dummyContainer.m_dummyBuffer){
		m_itemBuffer.compare_exchange_strong(rawPeek, create_item_buffer(alignedCapacity), std::memory_order_release, std::memory_order_relaxed);
	}

	shared_ptr_buffer_type active(nullptr);
	shared_ptr_buffer_type  front(nullptr);

	do{
		active = m_itemBuffer.load(std::memory_order_relaxed);
		front = active->find_front();

		if (!front){
			front = active;
		}

		if (!(front->get_capacity() < capacity)){
			break;
		}

	} while (!active->try_push_front(create_item_buffer(alignedCapacity)));
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::unsafe_clear()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	buffer_type* const active(m_itemBuffer.unsafe_get());

	if (active->is_valid())
	{
		active->unsafe_clear();

		shared_ptr_buffer_type front(active->find_front());
		if (front)
		{
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

	const buffer_type* const initial(t_producer.get().get());

	do
	{
		shared_ptr_buffer_type front(t_producer.get()->find_front());

		if (!front)
		{
			front = t_producer.get();
		}

		if (front.get() == t_producer.get().get())
		{
			const size_type capacity(front->get_capacity());
			const size_type newCapacity(capacity * 2);

			reserve(newCapacity);
		}
		else
		{
			t_producer = std::move(front);
		}

	} while (t_producer.get().get() == initial);
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

	bool is_readers_blocked() const;
	bool is_writers_blocked() const;

private:
	inline bool has_blockage_offset(size_type postVar, size_type preVar) const;
	inline void apply_blockage_offset(std::atomic<size_type>& preVar, std::atomic<size_type>& postVar);

	inline size_type blockage_offset() const;
	inline size_type blockage_threshhold() const;

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

	inline void try_publish_changes(size_type untilAt, std::atomic<size_t>& publishVar, std::uint8_t writeState);

	std::atomic<size_type> m_readReservationSync;
	GDUL_CQ_PADDING(64);
	std::atomic<size_type> m_readAt;
	GDUL_CQ_PADDING(64);

	GDUL_ATOMIC_WITH_VIEW(size_type, m_read);

	GDUL_CQ_PADDING(64);
	std::atomic<size_type> m_writeAt;
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
	, m_readReservationSync(0)
	, m_readAt(0)
	, m_read(0)
	, m_writeAt(0)
	, m_written(0)
{
}

template<class T, class Allocator>
inline item_buffer<T, Allocator>::~item_buffer()
{
	for (size_type i = 0; i < m_capacity; ++i){
		m_items[i].~item_slot<T>();
	}
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_active() const
{
	if (!m_next){
		return true;
	}

	const size_type read(m_read.load(std::memory_order_relaxed));
	const size_type written(m_written.load(std::memory_order_acquire));

	if (read != written){
		return true;
	}

	const size_type preWriteOffset(m_writeAt.load(std::memory_order_acquire));
	const size_type preWrite(preWriteOffset - blockage_offset());
	const size_type activeAccessors(preWrite - written);

	return activeAccessors;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_valid() const
{
	return m_items[m_written.load(std::memory_order_relaxed) % m_capacity].get_iteration() != std::numeric_limits<size_type>::max();
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::unsafe_invalidate()
{
	block_readers();
	block_writers();

	m_items[m_written.load(std::memory_order_relaxed) % m_capacity].set_iteration(std::numeric_limits<size_type>::max());

	if (m_next){
		m_next.unsafe_get()->unsafe_invalidate();
	}
}
template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::shared_ptr_buffer_type item_buffer<T, Allocator>::find_front()
{
	shared_ptr_buffer_type front(nullptr);
	item_buffer<T, allocator_type>* inspect(this);

	while (inspect->m_next){
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

	do{
		if (inspect->is_active()){
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
	const size_type readSlot(m_read.load(std::memory_order_relaxed));
	size_type accumulatedSize(m_written.load(std::memory_order_relaxed));
	accumulatedSize -= readSlot;

	if (m_next){
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

	while (last->m_next){
		last = last->m_next.unsafe_get();
	}

	if (!(last->get_capacity() < newBuffer->get_capacity())){
		return false;
	}

	last->block_writers();

	raw_ptr_buffer_type exp(nullptr);
	return last->m_next.compare_exchange_strong(exp, std::move(newBuffer));
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::unsafe_clear()
{
	m_readAt.store(0, std::memory_order_relaxed);

	const size_type written(m_written.exchange(0, std::memory_order_relaxed));
	const size_type read(m_read.exchange(0, std::memory_order_relaxed));

	m_writeAt.fetch_sub(written, std::memory_order_relaxed);
	m_readReservationSync.fetch_sub(read, std::memory_order_relaxed);

	for (size_type i = 0; i < m_capacity; ++i){
		m_items[i].set_iteration(std::uint8_t(0));
	}

	if (m_next){
		m_next.unsafe_get()->unsafe_clear();
	}
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::block_writers()
{
	apply_blockage_offset(m_writeAt, m_written);
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::block_readers()
{
	apply_blockage_offset(m_readReservationSync, m_read);
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_readers_blocked() const
{
	return has_blockage_offset(m_read.load(std::memory_order_relaxed), m_readReservationSync.load(std::memory_order_relaxed));
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_writers_blocked() const
{
	return has_blockage_offset(m_written.load(std::memory_order_relaxed), m_writeAt.load(std::memory_order_relaxed));
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::has_blockage_offset(size_type postVar, size_type preVar) const
{
	const size_type post(postVar);
	const size_type pre(preVar);

	return blockage_threshhold() < (pre - post);
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::apply_blockage_offset(std::atomic<size_type>& preVar, std::atomic<size_type>& postVar)
{
	size_type pre(preVar.load(std::memory_order_relaxed));
	size_type desired;
	do{
		const size_type post(postVar.load(std::memory_order_relaxed));

		if (has_blockage_offset(post, pre)){
			break;
		}

		desired = pre + blockage_offset();

	} while (!preVar.compare_exchange_strong(pre, desired, std::memory_order_relaxed));
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::size_type item_buffer<T, Allocator>::blockage_offset() const
{
	return (m_capacity + Buffer_Lock_Offset);
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::size_type item_buffer<T, Allocator>::blockage_threshhold() const
{
	return (m_capacity + Buffer_Lock_Threshhold);
}
template<class T, class Allocator>
template<class ...Arg>
inline bool item_buffer<T, Allocator>::try_push(Arg&& ...in)
{
	const size_type read(m_read.load(std::memory_order_acquire));
	const size_type at(m_writeAt.fetch_add(1, std::memory_order_relaxed));
	const size_type used(at - read);
	const size_type avaliable(m_capacity - used);

	if (!((avaliable - 1) < m_capacity)){
		apply_blockage_offset(m_writeAt, m_written);
		
		const size_type reRead(m_read.load(std::memory_order_acquire));
		const size_type reUsed(at - reRead);
		const size_type reAvaliable(m_capacity - reUsed);

		if (!((reAvaliable - 1) < m_capacity)){
			m_writeAt.fetch_add(1, std::memory_order_relaxed);
			return false;
		}
	}

	const size_type atLocal(at % m_capacity);

	write_in(atLocal, std::forward<Arg>(in)...);

	m_items[atLocal].increment_iteration();

	try_publish_changes(at, m_written, 1);

	return true;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::try_pop(T& out)
{
	const size_type written(m_written.load(std::memory_order_relaxed));
	const size_type reserved(m_readReservationSync.fetch_add(1, std::memory_order_relaxed));
	const size_type avaliable(written - reserved);

	if (!((avaliable - 1) < m_capacity)){
		m_readReservationSync.fetch_sub(1, std::memory_order_relaxed);
		return false;
	}

	const size_type at(m_readAt.fetch_add(1, std::memory_order_relaxed));
	const size_type atLocal(at % m_capacity);

	write_out(atLocal, out);

	m_items[atLocal].increment_iteration();

	try_publish_changes(at, m_read, 2);

	return true;
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::try_publish_changes(size_type from, std::atomic<size_t>& publishVar, std::uint8_t iterationOffset)
{
	// Most likely, this mechanism works, but occasionally publishes items before they are ready. (This causes premature
	// dequeueing of the item).

	// Something to note: While 'this' item has not been published, the maximum number of cross-publications is 
	// 'this' -> 'this' + (m_capacity - 1)

	// Hmm.. Loading published here. What if it is immediately increased one iteration? 
	size_type published(publishVar.load(std::memory_order_relaxed));
	if (from < published){
		// Simplification: Will checking published > from mean that we can early out?

		// If another caller has published 'this' caller's item, that would mean that it would
		// be in the probing state no earlier than 'this' caller incremented it's item's 
		// iteration.

		// Will an acquire barrier be necessary here?. If the loop is unrolled, and loads are performed
		// out of order.. ?

		// If the other caller originated at 'this' - (m_capacity - 1), that means no other
		// future caller would be dependant on 'this' caller (since they would not be allowed to
		// write further). 

		// If the other caller originated at 'this' - 1,  that would mean that all callers
		// that was dependent on 'this' caller, would be published by the other caller.
		return;
	}

	const size_type maxToPublish(from + m_capacity);
	
	constexpr size_type invalidIndex(std::numeric_limits<size_type>::max());
	
	size_type lastValid(invalidIndex);

	for (size_type i = from; i < maxToPublish; ++i){

		const size_type item(i % m_capacity);
		const size_type baseIteration(i / m_capacity);
		const size_type adjustedBaseIteration(baseIteration * 2);
		const size_type desiredIteration(adjustedBaseIteration + iterationOffset);

		const size_type itemIteration(m_items[item].get_iteration());

		// What kind of iteration states can this item have here?
		
		// It may be lower. This is always undesirable. A fact.
		// If it is lower, and also lower than 'from', another 
		// caller is pretty much guranteed to take care of publishing. 
		// (Because that caller will be in the middle of its operation)
		if (itemIteration < desiredIteration)
			break;
		
		// It may be the desired iteration. That would mean we are looking at
		// an item that has just been incremented to be in-line with this callee's 
		// timeline. 
		if (itemIteration == desiredIteration)
			lastValid = i;

		// It may be *not* the desired iteration. This SHOULD mean that this caller
		// stalled/lagged behind, and is reading future state. It does not mean that
		// this caller's absolved of responsibility, since there may have been callers 
		// relying ono this caller for publishing (that were not caught by the caller responsible for
		// publishing this)...
		else{
			// Will this make us skip over items?.... ?
			if (!(i < from)){
				// Second early out
				return;
			}
			lastValid = invalidIndex;
		}
	}

	if (lastValid == invalidIndex)
		return;

	published = publishVar.load(std::memory_order_relaxed);

	// It should not be possible for an earlier caller to see 'this' item as avaliable for publishing, and not
	// also catch all subsequent avaliable items, that would otherwise have been caught by 'this' caller.. ?
	while (!(from < published)){
		const size_type desired(lastValid + 1);
		if (publishVar.compare_exchange_weak(published, desired, std::memory_order_release, std::memory_order_relaxed))
			break;
	}
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
	//try
	//{
		m_items[slot].store(std::move(in));
	//}
	//catch (...)
	//{
	//	--m_writeAt;
	//	throw;
	//}
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
	//try
	//{
		m_items[slot].store(in);
	//}
	//catch (...)
	//{
	//	--m_writeAt;
	//	throw;
	//}
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
	using size_type = size_type;

	item_slot<T>(const item_slot<T>&) = delete;
	item_slot<T>& operator=(const item_slot&) = delete;

	inline item_slot();
	inline item_slot(size_type iteration);

	inline void store(const T& in);
	inline void store(T&& in);

	template<class U = T, std::enable_if_t<std::is_move_assignable<U>::value> * = nullptr>
	inline void try_move(U& out);
	template<class U = T, std::enable_if_t<!std::is_move_assignable<U>::value> * = nullptr>
	inline void try_move(U& out);

	inline void assign(T& out);
	inline void move(T& out);

	inline size_type get_iteration() const;
	inline void set_iteration(size_type iteration);
	inline void increment_iteration();

private:
	T m_data;
	size_type m_iteration;
};
template<class T>
inline item_slot<T>::item_slot()
	: item_slot(0)
{}
template<class T>
inline item_slot<T>::item_slot(size_type iteration)
	: m_data()
	, m_iteration(iteration)
{}
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
inline void item_slot<T>::set_iteration(typename item_slot<T>::size_type iteration)
{
	m_iteration = iteration;
}
template<class T>
inline void item_slot<T>::increment_iteration()
{
	++m_iteration;
}
template<class T>
inline typename item_slot<T>::size_type item_slot<T>::get_iteration() const
{
	return m_iteration;
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
	: m_dummyItem(std::numeric_limits<size_type>::max())
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
