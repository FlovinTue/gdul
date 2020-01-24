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

enum class item_state : std::uint8_t
{
	Empty,
	Valid,
	Failed,
	Dummy
};

template <class Dummy>
std::size_t log2_align(std::size_t from, std::size_t clamp);

template <class Dummy>
constexpr std::size_t next_aligned_to(std::size_t addr, std::size_t align);

template <class Dummy>
constexpr std::size_t aligned_size(std::size_t byteSize, std::size_t align);

template <class T, class Allocator>
class shared_ptr_allocator_adaptor;

// Not quite size_type max because we need some leaway in case we
// need to throw consumers out of a buffer whilst repairing it
static constexpr size_type Buffer_Capacity_Max = ~(std::numeric_limits<size_type>::max() >> 3) / 2;
static constexpr size_type Buffer_Capacity_Default = 8;

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
{}
template<class T, class Allocator>
inline concurrent_queue_fifo<T, Allocator>::concurrent_queue_fifo(Allocator allocator)
	: t_producer(s_dummyContainer.m_dummyBuffer, allocator)
	, t_consumer(s_dummyContainer.m_dummyBuffer, allocator)
	, m_itemBuffer(nullptr)
	, m_allocator(allocator)
{}
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
	(void)capacity;
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::unsafe_clear()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	m_itemBuffer->unsafe_load()->unsafe_clear();

	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::unsafe_reset()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	m_itemBuffer.unsafe_get()->invalidate();
	m_itemBuffer.unsafe_store(shared_ptr_buffer_type(nullptr), std::memory_order_relaxed);

	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::size_type concurrent_queue_fifo<T, Allocator>::size() const
{
	m_itemBuffer.load(std::memory_order_relaxed)->size();
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::size_type concurrent_queue_fifo<T, Allocator>::unsafe_size() const
{
	m_itemBuffer.unsafe_get()->size();
}
template<class T, class Allocator>
inline bool concurrent_queue_fifo<T, Allocator>::refresh_consumer()
{
	assert(t_consumer.get()->is_valid() && "refresh_consumer assumes a valid consumer");

	if (!m_itemBuffer){
		return false;
	}

	shared_ptr_buffer_type activeBuffer(m_itemBuffer.load(std::memory_order_relaxed));

	if (activeBuffer.get() != t_consumer.get().get()){
		t_consumer = std::move(activeBuffer);
		return true;
	}

	shared_ptr_buffer_type listBack(activeBuffer->find_back());
	if (listBack){
		if (m_itemBuffer.compare_exchange_strong(activeBuffer, listBack)){
			t_consumer = std::move(listBack);
		}
		else{
			t_consumer = std::move(activeBuffer);
		}
		return true;
	}

	return false;
}
template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::shared_ptr_buffer_type concurrent_queue_fifo<T, Allocator>::create_item_buffer(std::size_t withSize)
{
	const std::size_t log2size(cq_fifo_detail::log2_align<void>(withSize, cq_fifo_detail::Buffer_Capacity_Max));

	const std::size_t alignOfData(alignof(T));

	const std::size_t bufferByteSize(sizeof(buffer_type));
	const std::size_t dataBlockByteSize(sizeof(cq_fifo_detail::item_slot<T>) * log2size);

	constexpr std::size_t controlBlockByteSize(gdul::sp_claim_size_custom_delete<buffer_type, allocator_adapter_type, cq_fifo_detail::buffer_deleter<buffer_type, allocator_adapter_type>>());

	constexpr std::size_t controlBlockSize(cq_fifo_detail::aligned_size<void>(controlBlockByteSize, 8));
	constexpr std::size_t bufferSize(cq_fifo_detail::aligned_size<void>(bufferByteSize, 8));
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
	if ((t_cachedAccesses.m_lastProducer.m_addrBlock & cq_fifo_detail::Ptr_Mask) ^ reinterpret_cast<std::uintptr_t>(this)){
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

	if (!m_itemBuffer){
		return false;
	}
	t_consumer = m_itemBuffer.load(std::memory_order_relaxed);

	return true;
}

template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::initialize_producer()
{
	if (!m_itemBuffer){
		shared_ptr_buffer_type initialBuffer(create_item_buffer(cq_fifo_detail::Buffer_Capacity_Default));
		raw_ptr_buffer_type exp(nullptr, m_itemBuffer.get_version());
		m_itemBuffer.compare_exchange_strong(exp, std::move(initialBuffer));
	}
	t_producer = m_itemBuffer.load(std::memory_order_relaxed);
}

template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::refresh_producer()
{
	assert(t_producer.get()->is_valid() && "producer needs to be initialized");

	const buffer_type* const initial(t_producer.get().get());

	do{
		shared_ptr_buffer_type front(t_producer.get()->find_front());

		if (!front){
			front = t_producer.get();
		}

		if (front.get() == t_producer.get().get()){
			const size_type capacity(front->get_capacity());
			const size_type newCapacity(capacity * 2);

			shared_ptr_buffer_type newBuffer(create_item_buffer(newCapacity));

			front->try_push_front(std::move(newBuffer));
		}
		else{
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

	// Makes sure that it is entirely safe to replace this buffer with a successor
	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)> * = nullptr>
	inline bool verify_successor(const shared_ptr_buffer_type&);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)> * = nullptr>
	inline bool verify_successor(const shared_ptr_buffer_type& successor);

	// Contains entries and / or has no next buffer
	inline bool is_active() const;

	// Is this a dummybuffer or one from a destroyed structure?
	inline bool is_valid() const;

	// Used to signal a producer that it needs to be re-initialized.
	inline void invalidate();

	// Searches the buffer list towards the front for
	// the front-most buffer
	inline shared_ptr_buffer_type find_front();

	// Searches the buffer list towards the front for
	// the first buffer contining entries
	inline shared_ptr_buffer_type find_back();

	// Pushes a newly allocated buffer buffer to the front of the
	// buffer list
	inline bool try_push_front(shared_ptr_buffer_type newBuffer);

	inline void unsafe_clear();

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

	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)> * = nullptr>
	inline void post_pop_cleanup(size_type readSlot);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)> * = nullptr>
	inline void post_pop_cleanup(size_type readSlot);

	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)> * = nullptr>
	inline void check_for_damage();
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)> * = nullptr>
	inline void check_for_damage();

	inline void reintegrate_failed_entries(size_type failCount);

	std::atomic<size_type> m_preReadIterator;
	GDUL_ATOMIC_WITH_VIEW(size_type, m_readSlot);

#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	std::atomic<size_type> m_postReadIterator;
	std::atomic<std::uint16_t> m_failureCount;
	std::atomic<std::uint16_t> m_failureIndex;
	GDUL_CQ_PADDING((GDUL_CACHELINE_SIZE * 2) - ((sizeof(size_type) * 3) + (sizeof(std::uint16_t) * 2)));

#else
	GDUL_CQ_PADDING((GDUL_CACHELINE_SIZE * 2) - sizeof(size_type) * 2);
#endif
	size_type m_writeSlot;
	GDUL_ATOMIC_WITH_VIEW(size_type, m_postWriteIterator);
	GDUL_CQ_PADDING(GDUL_CACHELINE_SIZE - sizeof(size_type) * 2);
	atomic_shared_ptr_buffer_type m_next;

	const size_type m_capacity;
	item_slot<T>* const m_dataBlock;
};

template<class T, class Allocator>
inline item_buffer<T, Allocator>::item_buffer(typename item_buffer<T, Allocator>::size_type capacity, item_slot<T>* dataBlock)
	: m_next(nullptr)
	, m_dataBlock(dataBlock)
	, m_capacity(capacity)
	, m_readSlot(0)
	, m_preReadIterator(0)
	, m_writeSlot(0)
	, m_postWriteIterator(0)
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	, m_failureIndex(0)
	, m_failureCount(0)
	, m_postReadIterator(0)
#endif
{
}

template<class T, class Allocator>
inline item_buffer<T, Allocator>::~item_buffer()
{
	for (size_type i = 0; i < m_capacity; ++i)
	{
		m_dataBlock[i].~item_slot<T>();
	}
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_active() const
{
	return (!m_next || (m_readSlot.load(std::memory_order_relaxed) != m_postWriteIterator.load(std::memory_order_relaxed)));
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::is_valid() const
{
	return m_dataBlock[m_writeSlot % m_capacity].get_state_local() != item_state::Dummy;
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::invalidate()
{
	m_dataBlock[m_writeSlot % m_capacity].set_state(item_state::Dummy);
	if (m_next)
	{
		m_next.unsafe_get()->invalidate();
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

	while (inspect)
	{
		const size_type readSlot(inspect->m_readSlot.load(std::memory_order_relaxed));
		const size_type postWrite(inspect->m_postWriteIterator.load(std::memory_order_relaxed));

		const bool thisBufferEmpty(readSlot == postWrite);
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
		const bool veto(inspect->m_failureCount.load(std::memory_order_relaxed) != inspect->m_failureIndex.load(std::memory_order_relaxed));
		const bool valid(!thisBufferEmpty | veto);
#else
		const bool valid(!thisBufferEmpty);
#endif
		if (valid)
		{
			break;
		}

		back = inspect->m_next.load(std::memory_order_seq_cst);
		inspect = back.get();
	}
	return back;
}

template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::size_type item_buffer<T, Allocator>::size() const
{
	const size_type readSlot(m_readSlot.load(std::memory_order_relaxed));
	size_type accumulatedSize(m_postWriteIterator.load(std::memory_order_relaxed));
	accumulatedSize -= readSlot;

	if (m_next)
		accumulatedSize += m_next.unsafe_get()->size();

	return accumulatedSize;
}

template<class T, class Allocator>
inline typename item_buffer<T, Allocator>::size_type item_buffer<T, Allocator>::get_capacity() const
{
	return m_capacity;
}
template<class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline bool item_buffer<T, Allocator>::verify_successor(const shared_ptr_buffer_type&)
{
	return true;
}
template<class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline bool item_buffer<T, Allocator>::verify_successor(const shared_ptr_buffer_type& successor)
{
#if defined(GDUL_CQ_ENABLE_EXCEPTIONHANDLING)
	if (!m_next){
		return false;
	}

	shared_ptr_buffer_type next(nullptr);
	item_buffer<T, Allocator>* inspect(this);

	do{
		const size_type preRead(inspect->m_preReadIterator.load(std::memory_order_relaxed));
		for (size_type i = 0; i < inspect->m_capacity; ++i){
			const size_type index((preRead - i) % inspect->m_capacity);

			if (inspect->m_dataBlock[index].get_state_local() != item_state::Empty){
				return false;
			}
		}

		next = inspect->m_next.load(std::memory_order_seq_cst);
		inspect = next.get();

		if (inspect == successor.get())
		{
			break;
		}
	} while (inspect->m_next);
#else
	(void)successor;
#endif
	return true;
}
template<class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::check_for_damage()
{
}
template<class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::check_for_damage()
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	const size_type preRead(m_preReadIterator.load(std::memory_order_relaxed));
	const size_type preReadLockOffset(preRead - Buffer_Lock_Offset);
	if (preReadLockOffset != m_postReadIterator.load(std::memory_order_relaxed))
	{
		return;
	}

	const std::uint16_t failiureIndex(m_failureIndex.load(std::memory_order_relaxed));
	const std::uint16_t failiureCount(m_failureCount.load(std::memory_order_relaxed));
	const std::uint16_t difference(failiureCount - failiureIndex);

	const bool failCheckA(0 == difference);
	const bool failCheckB(!(difference < cq_fifo_detail::Max_Producers));
	if (failCheckA | failCheckB)
	{
		return;
	}

	const size_type toReintegrate(failiureCount - failiureIndex);

	std::uint16_t expected(failiureIndex);
	const std::uint16_t desired(failiureCount);
	if (m_failureIndex.compare_exchange_strong(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed))
	{
		reintegrate_failed_entries(toReintegrate);

		m_postReadIterator.fetch_sub(toReintegrate);
		m_readSlot.fetch_sub(toReintegrate);
		m_preReadIterator.fetch_sub(Buffer_Lock_Offset + toReintegrate);
	}
#endif
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

	raw_ptr_buffer_type exp(nullptr);
	return last->m_next.compare_exchange_strong(exp, std::move(newBuffer));
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::unsafe_clear()
{
	const size_type postWrite(m_postWriteIterator.load(std::memory_order_relaxed));
	m_preReadIterator.store(postWrite, std::memory_order_relaxed);
	m_readSlot.store(postWrite, std::memory_order_relaxed);

	for (size_type i = 0; i < m_capacity; ++i)
	{
		m_dataBlock[i].set_state_local(item_state::Empty);
	}

#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	m_failureCount.store(0, std::memory_order_relaxed);
	m_failureIndex.store(0, std::memory_order_relaxed);

	m_postReadIterator.store(postWrite, std::memory_order_relaxed);
#endif
	if (m_next)
	{
		m_next.unsafe_get()->unsafe_clear();
	}
}
template<class T, class Allocator>
template<class ...Arg>
inline bool item_buffer<T, Allocator>::try_push(Arg&& ...in)
{
	const size_type slotTotal(m_writeSlot++);
	const size_type slot(slotTotal % m_capacity);

	std::atomic_thread_fence(std::memory_order_acquire);

	if (m_dataBlock[slot].get_state_local() != item_state::Empty)
	{
		--m_writeSlot;
		return false;
	}

	write_in(slot, std::forward<Arg>(in)...);

	m_dataBlock[slot].set_state_local(item_state::Valid);

	m_postWriteIterator.fetch_add(1, std::memory_order_release);

	return true;
}
template<class T, class Allocator>
inline bool item_buffer<T, Allocator>::try_pop(T& out)
{
	const size_type lastWritten(m_postWriteIterator.load(std::memory_order_relaxed));
	const size_type slotReserved(m_preReadIterator.fetch_add(1, std::memory_order_relaxed) + 1);
	const size_type avaliable(lastWritten - slotReserved);

	if (m_capacity < avaliable)
	{
		m_preReadIterator.fetch_sub(1, std::memory_order_relaxed);
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
		check_for_damage();
#endif
		return false;
	}
	const size_type readSlotTotal(m_readSlot.fetch_add(1, std::memory_order_relaxed));
	const size_type readSlot(readSlotTotal % m_capacity);

	write_out(readSlot, out);

	post_pop_cleanup(readSlot);

	return true;
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)>*>
inline void item_buffer<T, Allocator>::write_in(typename item_buffer<T, Allocator>::size_type slot, U&& in)
{
	m_dataBlock[slot].store(std::move(in));
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)>*>
inline void item_buffer<T, Allocator>::write_in(typename item_buffer<T, Allocator>::size_type slot, U&& in)
{
	try
	{
		m_dataBlock[slot].store(std::move(in));
	}
	catch (...)
	{
		--m_writeSlot;
		throw;
	}
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::write_in(typename item_buffer<T, Allocator>::size_type slot, const U& in)
{
	m_dataBlock[slot].store(in);
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::write_in(typename item_buffer<T, Allocator>::size_type slot, const U& in)
{
	try
	{
		m_dataBlock[slot].store(in);
	}
	catch (...)
	{
		--m_writeSlot;
		throw;
	}
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U)>*>
inline void item_buffer<T, Allocator>::write_out(typename item_buffer<T, Allocator>::size_type slot, U& out)
{
	m_dataBlock[slot].move(out);
}
template<class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::post_pop_cleanup(typename item_buffer<T, Allocator>::size_type readSlot)
{
	m_dataBlock[readSlot].set_state(item_state::Empty);
	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::post_pop_cleanup(typename item_buffer<T, Allocator>::size_type readSlot)
{
	m_dataBlock[readSlot].set_state(item_state::Empty);
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	m_dataBlock[readSlot].reset_ref();
	m_postReadIterator.fetch_add(1, std::memory_order_release);
#else
	std::atomic_thread_fence(std::memory_order_release);
#endif
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::write_out(typename item_buffer<T, Allocator>::size_type slot, U& out)
{
	m_dataBlock[slot].assign(out);
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void item_buffer<T, Allocator>::write_out(typename item_buffer<T, Allocator>::size_type slot, U& out)
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	try
	{
#endif
		m_dataBlock[slot].try_move(out);
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	}
	catch (...)
	{
		if (m_failureCount.fetch_add(1, std::memory_order_relaxed) == m_failureIndex.load(std::memory_order_relaxed))
		{
			m_preReadIterator.fetch_add(Buffer_Lock_Offset, std::memory_order_release);
		}
		m_dataBlock[slot].set_state(item_state::Failed);
		m_postReadIterator.fetch_add(1, std::memory_order_release);
		throw;
	}
#endif
}
template<class T, class Allocator>
inline void item_buffer<T, Allocator>::reintegrate_failed_entries(typename item_buffer<T, Allocator>::size_type failCount)
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	const size_type readSlotTotal(m_readSlot.load(std::memory_order_relaxed));
	const size_type readSlotTotalOffset(readSlotTotal + m_capacity);

	const size_type startIndex(readSlotTotalOffset - 1);

	size_type numRedirected(0);
	for (size_type i = 0, j = startIndex; numRedirected != failCount; ++i, --j)
	{
		const size_type currentIndex((startIndex - i) % m_capacity);
		item_slot<T>& currentItem(m_dataBlock[currentIndex]);
		const item_state currentState(currentItem.get_state_local());

		if (currentState == item_state::Failed)
		{
			const size_type toRedirectIndex((startIndex - numRedirected) % m_capacity);
			item_slot<T>& toRedirect(m_dataBlock[toRedirectIndex]);

			toRedirect.redirect(currentItem);
			currentItem.set_state_local(item_state::Valid);
			++numRedirected;
		}
	}
#else
	failCount;
#endif
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
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	inline void redirect(item_slot<T>& to);
#endif
	template<class U = T, std::enable_if_t<std::is_move_assignable<U>::value> * = nullptr>
	inline void try_move(U& out);
	template<class U = T, std::enable_if_t<!std::is_move_assignable<U>::value> * = nullptr>
	inline void try_move(U& out);

	inline void assign(T& out);
	inline void move(T& out);

	inline item_state get_state_local() const;
	inline void set_state(item_state state);
	inline void set_state_local(item_state state);

	inline void reset_ref();

private:
	T m_data;
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	// May or may not reference this continer
	inline item_slot<T>& reference() const;

	union
	{
		std::uint64_t m_stateBlock;
		item_slot<T>* m_reference;
		struct
		{
			std::uint16_t trash[3];
			item_state m_state;
		};
	};
#else
	item_state m_state;
#endif
};
template<class T>
inline item_slot<T>::item_slot()
	: m_data()
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	, m_reference(this)
#else
	, m_state(item_state::Empty)
#endif
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
	reset_ref();
}
template<class T>
inline void item_slot<T>::store(T&& in)
{
	m_data = std::move(in);
	reset_ref();
}
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
template<class T>
inline void item_slot<T>::redirect(item_slot<T>& to)
{
	const std::uint64_t otherPtrBlock(to.m_stateBlock & Ptr_Mask);
	m_stateBlock &= ~Ptr_Mask;
	m_stateBlock |= otherPtrBlock;
}
#endif
template<class T>
inline void item_slot<T>::assign(T& out)
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	out = reference().m_data;
#else
	out = m_data;
#endif
}
template<class T>
inline void item_slot<T>::move(T& out)
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	out = std::move(reference().m_data);
#else
	out = std::move(m_data);
#endif
}
template<class T>
inline void item_slot<T>::set_state(item_state state)
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	reference().m_state = state;
#else
	m_state = state;
#endif
}

template<class T>
inline void item_slot<T>::set_state_local(item_state state)
{
	m_state = state;
}
template<class T>
inline void item_slot<T>::reset_ref()
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	m_reference = this;
#endif
}
template<class T>
inline item_state item_slot<T>::get_state_local() const
{
	return m_state;
}
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
template<class T>
inline item_slot<T>& item_slot<T>::reference() const
{
	return *reinterpret_cast<item_slot<T>*>(m_stateBlock & Ptr_Mask);
}
#endif
template<class T>
template<class U, std::enable_if_t<std::is_move_assignable<U>::value>*>
inline void item_slot<T>::try_move(U& out)
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	out = std::move(reference().m_data);
#else
	out = std::move(m_data);
#endif
}
template<class T>
template<class U, std::enable_if_t<!std::is_move_assignable<U>::value>*>
inline void item_slot<T>::try_move(U& out)
{
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	out = reference().m_data;
#else
	out = m_data;
#endif
}
template <class Dummy>
std::size_t log2_align(std::size_t from, std::size_t clamp)
{
	const std::size_t from_(from < 2 ? 2 : from);

	const float flog2(std::log2f(static_cast<float>(from_)));
	const float nextLog2(std::ceil(flog2));
	const float fNextVal(powf(2.f, nextLog2));

	const std::size_t nextVal(static_cast<size_t>(fNextVal));
	const std::size_t clampedNextVal((clamp < nextVal) ? clamp : nextVal);

	return clampedNextVal;
}
template <class Dummy>
inline std::uint8_t to_store_array_slot(std::uint16_t producerIndex)
{
	const float fSourceStoreSlot(log2f(static_cast<float>(producerIndex)));
	const std::uint8_t sourceStoreSlot(static_cast<std::uint8_t>(fSourceStoreSlot));
	return sourceStoreSlot;
}
template <class Dummy>
std::uint16_t to_store_array_capacity(std::uint16_t storeSlot)
{
	return std::uint16_t(static_cast<std::uint16_t>(powf(2.f, static_cast<float>(storeSlot + 1))));
}
template <class Dummy>
constexpr std::size_t next_aligned_to(std::size_t addr, std::size_t align)
{
	const std::size_t mod(addr % align);
	const std::size_t remainder(align - mod);
	const std::size_t offset(remainder == align ? 0 : remainder);

	return addr + offset;
}
template <class Dummy>
constexpr std::size_t aligned_size(std::size_t byteSize, std::size_t align)
{
	const std::size_t div(byteSize / align);
	const std::size_t mod(byteSize % align);
	const std::size_t total(div + static_cast<std::size_t>(static_cast<bool>(mod)));

	return align * total;
}
template <class SharedPtrSlotType, class SharedPtrArrayType>
struct consumer_wrapper
{
	consumer_wrapper()
		: consumer_wrapper<SharedPtrSlotType, SharedPtrArrayType>::consumer_wrapper(SharedPtrSlotType(nullptr))
	{
	}

	consumer_wrapper(SharedPtrSlotType buffer)
		: m_buffer(std::move(buffer))
		, m_popCounter(0)
	{
	}

	SharedPtrSlotType m_buffer;
	SharedPtrArrayType m_lastKnownArray;
	std::uint16_t m_popCounter;
};
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
	};

	shared_ptr_allocator_adaptor()
		: m_address(nullptr)
		, m_size(0)
	{
	};
	shared_ptr_allocator_adaptor(T* retAddr, std::size_t size)
		: m_address(retAddr)
		, m_size(size)
	{
	};

	T* allocate(std::size_t count)
	{
		if (!m_address)
		{
			m_address = this->Allocator::allocate(count);
			m_size = count;
		}
		return m_address;
	};
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
	: m_dummyItem(item_state::Dummy)
	, m_dummyRawBuffer(1, &m_dummyItem)
{
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
