//Copyright(c) 2020 Flovin Michaelsen
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
#include <atomic>
#include <thread>


// In the event an exception is thrown during a pop operation, some entries may
// be dequeued out-of-order as some consumers may already be halfway through a
// pop operation before reintegration efforts are started.

#ifndef GDUL_ATOMIC_WITH_VIEW
#define GDUL_ATOMIC_WITH_VIEW(type, name) union{std::atomic<type> name; type _##name;}
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

namespace gdul {

namespace cq_detail {

typedef std::size_t size_type;

template <class T, class Allocator>
struct accessor_cache;

template <class SharedPtrSlotType, class SharedPtrArrayType>
struct consumer_wrapper;

template <class T, class Allocator>
class producer_buffer;

template <class T, class Allocator>
class dummy_container;

template <class T, class Allocator>
class buffer_deleter;

template <class T, class Allocator>
class shared_ptr_allocator_adaptor;

// The maximum allowed consumption from a producer per visit
static constexpr std::uint16_t ConsumerForceRelocationPopCount = 24;

static constexpr size_type InitialProducerCapacity = 8;
static constexpr size_type BufferCapacityMax = ~(std::numeric_limits<size_type>::max() >> 3) / 2;

static constexpr std::uint64_t PtrMask = (std::uint64_t(std::numeric_limits<std::uint32_t>::max()) << 16 | std::uint64_t(std::numeric_limits<std::uint16_t>::max()));

}
// MPMC unbounded lock-free queue. FIFO is respected within the context of single producers.
template <class T, class Allocator = std::allocator<std::uint8_t>>
class concurrent_queue
{
public:
	typedef cq_detail::size_type size_type;

	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	inline concurrent_queue();
	inline concurrent_queue(Allocator allocator);
	inline ~concurrent_queue() noexcept;

	inline void push(const T& in);
	inline void push(T&& in);

	bool try_pop(T& out);

	// reserves a minimum capacity for the calling producer
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
	friend class cq_detail::producer_buffer<T, allocator_type>;
	friend class cq_detail::dummy_container<T, allocator_type>;

	using buffer_type = cq_detail::producer_buffer<T, allocator_type>;
	using allocator_adapter_type = cq_detail::shared_ptr_allocator_adaptor<std::uint8_t, allocator_type>;
	using shared_ptr_slot_type = shared_ptr<buffer_type>;
	using atomic_shared_ptr_slot_type = atomic_shared_ptr<buffer_type>;
	using atomic_shared_ptr_array_type = atomic_shared_ptr<atomic_shared_ptr_slot_type[]>;
	using shared_ptr_array_type = shared_ptr<atomic_shared_ptr_slot_type[]>;

	template <class In>
	void push_internal(In&& in);

	inline void init_producer(size_type withCapacity);
	inline void add_producer_buffer();

	inline bool relocate_consumer();

	inline shared_ptr_slot_type create_producer_buffer(std::size_t withSize);
	inline void push_producer_buffer(shared_ptr_slot_type buffer);
	inline void try_swap_producer_count(std::uint16_t toValue);

	inline std::uint16_t claim_producer_slot();
	inline void force_store_to_producer_slot(shared_ptr_slot_type producer, std::uint16_t slot);
	inline void ensure_producer_slots_capacity(std::uint16_t minCapacity);

	inline buffer_type* this_producer_cached();
	inline buffer_type* this_consumer_cached();

	inline void refresh_cached_consumer();
	inline void refresh_cached_producer();

	cq_detail::dummy_container<T, allocator_type> m_dummyContainer;

	tlm<shared_ptr_slot_type, allocator_type> t_producer;
	tlm<cq_detail::consumer_wrapper<shared_ptr_slot_type, shared_ptr_array_type>, allocator_type> t_consumer;

	struct cache_container { cq_detail::accessor_cache<T, allocator_type> m_lastConsumer, m_lastProducer; };

	static thread_local cache_container t_cachedAccesses;

	atomic_shared_ptr_array_type m_producerSlots;
	atomic_shared_ptr_array_type m_producerSlotsSwap;

	GDUL_ATOMIC_WITH_VIEW(std::uint16_t, m_producerCount);

	std::atomic<std::uint16_t> m_relocationIndex;
	std::atomic<std::uint16_t> m_producerSlotReservation;
	std::atomic<std::uint16_t> m_producerSlotPostReservation;

	allocator_type m_allocator;
};

template<class T, class Allocator>
inline concurrent_queue<T, Allocator>::concurrent_queue()
	: concurrent_queue<T, Allocator>(Allocator())
{
}
template<class T, class Allocator>
inline concurrent_queue<T, Allocator>::concurrent_queue(Allocator allocator)
	: m_dummyContainer()
	, m_producerCount(0)
	, m_producerSlotPostReservation(0)
	, m_producerSlotReservation(0)
	, t_producer(allocator, m_dummyContainer.m_dummyBuffer)
	, t_consumer(allocator, cq_detail::consumer_wrapper<shared_ptr_slot_type, shared_ptr_array_type>(m_dummyContainer.m_dummyBuffer))
	, m_producerSlots(nullptr)
	, m_producerSlotsSwap(nullptr)
	, m_relocationIndex(0)
	, m_allocator(allocator)
{
}
template<class T, class Allocator>
inline concurrent_queue<T, Allocator>::~concurrent_queue() noexcept
{
	unsafe_reset();
}

template<class T, class Allocator>
void concurrent_queue<T, Allocator>::push(const T& in)
{
	push_internal(in);
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::push(T&& in)
{
	push_internal(std::move(in));
}
template<class T, class Allocator>
template<class In>
inline void concurrent_queue<T, Allocator>::push_internal(In&& in)
{
	if (!this_producer_cached()->try_push(std::forward<In>(in))) {
		if (this_producer_cached()->is_valid()) {
			add_producer_buffer();
		}
		else {
			init_producer(cq_detail::InitialProducerCapacity);
		}

		refresh_cached_producer();

		this_producer_cached()->try_push(std::forward<In>(in));
	}
}
template<class T, class Allocator>
bool concurrent_queue<T, Allocator>::try_pop(T& out)
{
	while (!this_consumer_cached()->try_pop(out)) {
		if (!relocate_consumer()) {
			return false;
		}
	}

	if ((1 < m_producerCount.load(std::memory_order_relaxed)) && !(++t_cachedAccesses.m_lastConsumer.m_counter < cq_detail::ConsumerForceRelocationPopCount)) {
		relocate_consumer();
		t_cachedAccesses.m_lastConsumer.m_counter = 0;
	}

	return true;
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::reserve(typename concurrent_queue<T, Allocator>::size_type capacity)
{
	shared_ptr_slot_type& producer(t_producer);

	if (!producer->is_valid()) {
		init_producer(capacity);
	}
	else if (producer->capacity() < capacity) {
		const size_type pow2Capacity(align_value_pow2(capacity, cq_detail::BufferCapacityMax));
		shared_ptr_slot_type buffer(create_producer_buffer(pow2Capacity));
		producer->push_front(buffer);
		producer = std::move(buffer);
	}
	refresh_cached_producer();
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::unsafe_clear()
{
	shared_ptr_array_type producerArray(m_producerSlots.unsafe_load(std::memory_order_relaxed));
	for (std::uint16_t i = 0; i < m_producerCount.load(std::memory_order_relaxed); ++i) {
		producerArray[i].unsafe_get()->unsafe_clear();
	}
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::unsafe_reset()
{
	const std::uint16_t producerCount(m_producerCount.load(std::memory_order_relaxed));

	m_relocationIndex.store(0, std::memory_order_relaxed);
	m_producerCount.store(0, std::memory_order_relaxed);
	m_producerSlotPostReservation.store(0, std::memory_order_relaxed);
	m_producerSlotReservation.store(0, std::memory_order_relaxed);

	shared_ptr_array_type slots(m_producerSlots.unsafe_load(std::memory_order_relaxed));

	for (std::uint16_t i = 0; i < producerCount; ++i) {
		buffer_type* slot(slots[i].unsafe_get());
		slot->unsafe_clear();
		slot->invalidate();
	}

	m_producerSlots.unsafe_store(shared_ptr_array_type(nullptr), std::memory_order_relaxed);
	m_producerSlotsSwap.unsafe_store(shared_ptr_array_type(nullptr), std::memory_order_relaxed);
}
template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::size_type concurrent_queue<T, Allocator>::size() const
{
	const std::uint16_t producerCount(m_producerCount.load(std::memory_order_acquire));

	shared_ptr_array_type producerArray(m_producerSlots.load(std::memory_order_relaxed));

	size_type accumulatedSize(0);
	for (std::uint16_t i = 0; i < producerCount; ++i) {
		const shared_ptr_slot_type slot(producerArray[i].load(std::memory_order_relaxed));
		accumulatedSize += slot->size();
	}
	return accumulatedSize;
}
template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::size_type concurrent_queue<T, Allocator>::unsafe_size() const
{
	const std::uint16_t producerCount(m_producerCount.load(std::memory_order_relaxed));

	const atomic_shared_ptr_slot_type* const producerArray(m_producerSlots.unsafe_get());

	size_type accumulatedSize(0);
	for (std::uint16_t i = 0; i < producerCount; ++i) {
		const buffer_type* const slot(producerArray[i].unsafe_get());
		accumulatedSize += slot->size();
	}
	return accumulatedSize;
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::init_producer(typename concurrent_queue<T, Allocator>::size_type withCapacity)
{
	shared_ptr_slot_type newBuffer(create_producer_buffer(withCapacity));
	push_producer_buffer(newBuffer);
	t_producer = std::move(newBuffer);
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::add_producer_buffer()
{
	buffer_type* const cachedProducer(this_producer_cached());
	shared_ptr_slot_type next(create_producer_buffer(std::size_t(cachedProducer->capacity()) * 2));
	cachedProducer->push_front(next);
	t_producer = std::move(next);
}
template<class T, class Allocator>
inline bool concurrent_queue<T, Allocator>::relocate_consumer()
{
	const std::uint16_t producers(m_producerCount.load(std::memory_order_acquire));

	if (producers < 2) {
		buffer_type* const cached(this_consumer_cached());
		if ((cached->is_valid() && cached->is_active())) {
			return false;
		}
		if (producers == 0) {
			return false;
		}
	}

	const std::uint16_t relocation(m_relocationIndex.fetch_add(1, std::memory_order_relaxed));
	const std::uint16_t maxVisited(producers);

	cq_detail::consumer_wrapper<shared_ptr_slot_type, shared_ptr_array_type>& consumer(t_consumer);

	if (consumer.m_lastKnownArray != m_producerSlots) {
		consumer.m_lastKnownArray = m_producerSlots.load(std::memory_order_relaxed);
	}

	for (std::uint16_t i = 0, j = relocation; i < maxVisited; ++i, ++j) {
		const std::uint16_t entry(j % producers);
		shared_ptr_slot_type producerBuffer(consumer.m_lastKnownArray[entry].load(std::memory_order_relaxed));

		assert(producerBuffer->is_valid() && "An invalid buffer should not exist in the queue structure. Rogue 'unsafe_reset()' ?");

		if (!producerBuffer->size()) {
			continue;
		}

		if (!producerBuffer->is_active()) {
			shared_ptr_slot_type successor(producerBuffer->find_back());
			if (successor) {
				if (producerBuffer->verify_successor(successor)) {
					producerBuffer = std::move(successor);
					consumer.m_lastKnownArray[entry].store(producerBuffer, std::memory_order_release);
				}
			}
		}

		consumer.m_buffer = std::move(producerBuffer);
		consumer.m_popCounter = 0;

		refresh_cached_consumer();

		return true;
	}
	return false;
}

template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::shared_ptr_slot_type concurrent_queue<T, Allocator>::create_producer_buffer(std::size_t withSize)
{
	const std::size_t pow2size(align_value_pow2(withSize, cq_detail::BufferCapacityMax));

	const std::size_t bufferByteSize(sizeof(buffer_type));
	const std::size_t dataBlockByteSize(sizeof(T) * pow2size);
	const std::size_t stateBlockByteSize(sizeof(cq_detail::item_state) * pow2size);

	constexpr std::size_t controlBlockByteSize(gdul::sp_claim_size_custom_delete<buffer_type, allocator_adapter_type, cq_detail::buffer_deleter<buffer_type, allocator_adapter_type>>());

	constexpr std::size_t controlBlockSize(align_value(controlBlockByteSize, 8));
	constexpr std::size_t bufferBlockSize(align_value(bufferByteSize, 8));
	const std::size_t dataBlockSize(align_value(dataBlockByteSize, alignof(T)));

	const std::size_t totalBlockSize(controlBlockSize + bufferBlockSize + dataBlockSize + stateBlockByteSize);

	std::uint8_t* totalBlock(nullptr);

	buffer_type* buffer(nullptr);
	T* data(nullptr);
	cq_detail::item_state* states(nullptr);

	totalBlock = m_allocator.allocate(totalBlockSize);

	const std::size_t totalBlockBegin(reinterpret_cast<std::size_t>(totalBlock));
	const std::size_t controlBlockBegin(totalBlockBegin);
	const std::size_t bufferBegin(controlBlockBegin + controlBlockSize);
	const std::size_t dataBegin(align_value(bufferBegin + bufferBlockSize, alignof(T)));
	const std::size_t stateBegin(dataBegin + dataBlockSize);

	data = reinterpret_cast<T*>(dataBegin);
	states = reinterpret_cast<cq_detail::item_state*>(stateBegin);

	std::uninitialized_default_construct(data, data + pow2size);
	std::uninitialized_fill(states, states + pow2size, cq_detail::item_state::empty);

	buffer = new(reinterpret_cast<buffer_type*>(bufferBegin)) buffer_type(static_cast<size_type>(pow2size), data);

	allocator_adapter_type allocAdaptor(totalBlock, totalBlockSize);

	shared_ptr_slot_type returnValue(buffer, allocAdaptor, cq_detail::buffer_deleter<buffer_type, allocator_adapter_type>());

	return returnValue;
}
template <class T, class Allocator>
std::uint16_t concurrent_queue<T, Allocator>::claim_producer_slot()
{
	std::uint16_t desiredSlot(m_producerSlotReservation.load(std::memory_order_acquire));
	do {
		ensure_producer_slots_capacity(desiredSlot + 1);
	} while (!m_producerSlotReservation.compare_exchange_strong(desiredSlot, desiredSlot + 1, std::memory_order_release, std::memory_order_relaxed));

	return desiredSlot;
}
template <class T, class Allocator>
void concurrent_queue<T, Allocator>::ensure_producer_slots_capacity(std::uint16_t minCapacity)
{
	shared_ptr_array_type activeArray(nullptr);
	shared_ptr_array_type swapArray(nullptr);
	do {
		activeArray = m_producerSlots.load();

		if (!(activeArray.item_count() < minCapacity)) {
			break;
		}

		swapArray = m_producerSlotsSwap.load();

		if (swapArray.item_count() < minCapacity) {
			const float growth((float)minCapacity * 1.4f);
			shared_ptr_array_type grown(gdul::allocate_shared<atomic_shared_ptr_slot_type[]>((size_type)growth, m_allocator));

			raw_ptr<atomic_shared_ptr_slot_type[]> exp(swapArray);
			m_producerSlotsSwap.compare_exchange_strong(exp, std::move(grown));

			continue;
		}

		for (size_type i = 0, itemCount((size_type)activeArray.item_count()); i < itemCount; ++i) {
			shared_ptr<buffer_type> active(activeArray[i].load());

			if (!active) {
				continue;
			}

			raw_ptr<buffer_type> exp(nullptr, 0);
			swapArray[i].compare_exchange_strong(exp, std::move(active));
		}

		if (m_producerSlots.compare_exchange_strong(activeArray, swapArray)) {
			break;
		}
	} while (activeArray.item_count() < minCapacity);

	assert(m_producerSlots);

	if (m_producerSlotsSwap) {
		raw_ptr<atomic_shared_ptr_slot_type[]> expSwap(m_producerSlotsSwap);
		if (expSwap == swapArray) {
			m_producerSlotsSwap.compare_exchange_strong(expSwap, shared_ptr_array_type(nullptr));
		}
	}
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::force_store_to_producer_slot(shared_ptr_slot_type producer, std::uint16_t slot)
{
	shared_ptr_array_type activeArray(nullptr);
	shared_ptr_array_type swapArray(nullptr);

	do {
		// First, get active array view. We know this can hold our item.
		activeArray = m_producerSlots.load(std::memory_order_acquire);

		// Then, get swap array view, this might be null or of higher capacity. At this point, activeArray might be outdated.
		swapArray = m_producerSlotsSwap.load(std::memory_order_acquire);

		if (activeArray[slot] != producer) {
			activeArray[slot].store(producer, std::memory_order_release);
		}
		if (slot < swapArray.item_count() && swapArray[slot] != producer) {
			swapArray[slot].store(producer, std::memory_order_release);
		}

		// Now, just repeat until we know our view of the arrays are up-to-date
	} while (m_producerSlotsSwap != swapArray || m_producerSlots != activeArray);
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::push_producer_buffer(shared_ptr_slot_type buffer)
{
	// reserve slot and ensure producer array capacity
	const std::uint16_t bufferSlot(claim_producer_slot());

	// re-store to both swap array and producer array until their relationship has stabilized
	force_store_to_producer_slot(std::move(buffer), bufferSlot);

	// Synchronize with other producers to ensure all producers up until that of producer count have been written
	const std::uint16_t postIterator(m_producerSlotPostReservation.fetch_add(1, std::memory_order_acq_rel) + 1);
	const std::uint16_t reservedSlots(m_producerSlotReservation.load(std::memory_order_relaxed));

	// Check if we are sync'd
	if (postIterator == reservedSlots) {
		try_swap_producer_count(postIterator);
	}
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::try_swap_producer_count(std::uint16_t toValue)
{
	std::uint16_t exp(m_producerCount.load(std::memory_order_acquire));
	while (exp < toValue && !m_producerCount.compare_exchange_strong(exp, toValue, std::memory_order_release));
}
template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::buffer_type* concurrent_queue<T, Allocator>::this_producer_cached()
{
	if ((t_cachedAccesses.m_lastProducer.m_addrBlock & cq_detail::PtrMask) ^ reinterpret_cast<std::uintptr_t>(this)) {
		refresh_cached_producer();
	}
	return t_cachedAccesses.m_lastProducer.m_buffer;
}

template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::buffer_type* concurrent_queue<T, Allocator>::this_consumer_cached()
{
	if ((t_cachedAccesses.m_lastConsumer.m_addrBlock & cq_detail::PtrMask) ^ reinterpret_cast<std::uintptr_t>(this)) {
		refresh_cached_consumer();
	}
	return t_cachedAccesses.m_lastConsumer.m_buffer;
}

template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::refresh_cached_consumer()
{
	t_cachedAccesses.m_lastConsumer.m_buffer = t_consumer.get().m_buffer.get();
	t_cachedAccesses.m_lastConsumer.m_addr = this;
	t_cachedAccesses.m_lastConsumer.m_counter = t_consumer.get().m_popCounter;
}

template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::refresh_cached_producer()
{
	t_cachedAccesses.m_lastProducer.m_buffer = t_producer.get().get();
	t_cachedAccesses.m_lastProducer.m_addr = this;
}

namespace cq_detail {

enum class item_state : std::uint8_t
{
	empty,
	valid,
	dummy
};

template <class T, class Allocator>
class producer_buffer
{
private:
	using atomic_shared_ptr_slot_type = typename concurrent_queue<T, Allocator>::atomic_shared_ptr_slot_type;
	using shared_ptr_slot_type = typename concurrent_queue<T, Allocator>::shared_ptr_slot_type;

public:
	typedef typename concurrent_queue<T, Allocator>::size_type size_type;
	typedef typename concurrent_queue<T, Allocator>::allocator_type allocator_type;
	typedef producer_buffer<T, Allocator> buffer_type;

	producer_buffer(size_type capacity, std::atomic<std::uint8_t>* stateBlock, T* dataBlock);
	~producer_buffer();

	template<class In>
	inline bool try_push(In&& in);
	inline bool try_pop(T& out);

	inline size_type size() const;
	inline size_type capacity() const noexcept;

	// Contains entries and / or has no next buffer
	inline bool is_active() const;

	// Is this a dummybuffer or one from a destroyed structure?
	inline bool is_valid() const;

	// Used to signal a producer that it needs to be re-initialized.
	inline void invalidate();

	// Searches the buffer list towards the front for
	// the first buffer contining entries
	inline shared_ptr_slot_type find_back();

	// Pushes a newly allocated buffer buffer to the front of the
	// buffer list
	inline void push_front(shared_ptr_slot_type newBuffer);

	inline void unsafe_clear();

private:

	std::atomic<size_type> m_preReadSync;
	GDUL_ATOMIC_WITH_VIEW(size_type, m_readSlot);

	GDUL_CQ_PADDING((std::hardware_destructive_interference_size * 2) - sizeof(size_type) * 2);

	GDUL_ATOMIC_WITH_VIEW(size_type, m_written);
	size_type m_writeSlot;

	GDUL_CQ_PADDING(std::hardware_destructive_interference_size - sizeof(size_type) * 2);
	atomic_shared_ptr_slot_type m_next;

	// Capacity pow2 aligned, so we can do away with modulus and use AND instead
	const size_type m_capacityMask;

	std::atomic<item_state>* const m_stateBlock;

	T* const m_dataBlock;
};

template<class T, class Allocator>
inline producer_buffer<T, Allocator>::producer_buffer(typename producer_buffer<T, Allocator>::size_type capacity, std::atomic<std::uint8_t>* stateBlock, T* dataBlock)
	: m_preReadSync(0)
	, m_readSlot(0)
	, m_written(0)
	, m_writeSlot(0)
	, m_next(nullptr)
	, m_capacityMask(capacity - 1)
	, m_stateBlock(stateBlock)
	, m_dataBlock(dataBlock)
{
}

template<class T, class Allocator>
inline producer_buffer<T, Allocator>::~producer_buffer()
{
	for (size_type i = 0; i < capacity(); ++i) {
		std::destroy_at(&m_dataBlock[i]);
	}
}
template<class T, class Allocator>
inline bool producer_buffer<T, Allocator>::is_active() const
{
	return (!m_next || (m_readSlot.load(std::memory_order_acquire) != m_written.load(std::memory_order_acquire)));
}
template<class T, class Allocator>
inline bool producer_buffer<T, Allocator>::is_valid() const
{
	return m_dataBlock[m_writeSlot & m_capacityMask].get_state_local() != item_state::dummy;
}
template<class T, class Allocator>
inline void producer_buffer<T, Allocator>::invalidate()
{
	m_dataBlock[m_writeSlot & m_capacityMask].set_state(item_state::dummy);
	if (m_next) {
		m_next.unsafe_get()->invalidate();
	}
}
template<class T, class Allocator>
inline typename producer_buffer<T, Allocator>::shared_ptr_slot_type producer_buffer<T, Allocator>::find_back()
{
	shared_ptr_slot_type back(nullptr);
	producer_buffer<T, allocator_type>* inspect(this);

	while (inspect) {
		const size_type readSlot(inspect->m_readSlot.load(std::memory_order_relaxed));
		const size_type writeSlot(inspect->m_written.load(std::memory_order_acquire));

		const bool empty(readSlot == writeSlot);

		if (!empty) {
			break;
		}

		back = inspect->m_next.load(std::memory_order_acquire);
		inspect = back.get();
	}
	return back;
}

template<class T, class Allocator>
inline typename producer_buffer<T, Allocator>::size_type producer_buffer<T, Allocator>::size() const
{
	const size_type readSlot(m_readSlot.load(std::memory_order_relaxed));
	size_type accumulatedSize(m_written.load(std::memory_order_relaxed));
	accumulatedSize -= readSlot;

	if (m_next)
		accumulatedSize += m_next.unsafe_get()->size();

	return accumulatedSize;
}

template<class T, class Allocator>
inline typename producer_buffer<T, Allocator>::size_type producer_buffer<T, Allocator>::capacity() const noexcept
{
	return m_capacityMask + 1;
}

template<class T, class Allocator>
inline void producer_buffer<T, Allocator>::push_front(shared_ptr_slot_type newBuffer)
{
	producer_buffer<T, allocator_type>* last(this);

	while (last->m_next) {
		last = last->m_next.unsafe_get();
	}

	last->m_next.store(std::move(newBuffer), std::memory_order_release);
}

template<class T, class Allocator>
inline void producer_buffer<T, Allocator>::unsafe_clear()
{
	const size_type written(m_written.load(std::memory_order_relaxed));

	m_preReadSync.store(written, std::memory_order_relaxed);
	m_readSlot.store(written, std::memory_order_relaxed);

	if (m_next) {
		m_next.unsafe_get()->unsafe_clear();
	}
}
template<class T, class Allocator>
template<class In>
inline bool producer_buffer<T, Allocator>::try_push(In&& in)
{
	const size_type slotTotal(m_writeSlot++);
	const size_type slot(slotTotal & m_capacityMask);

	if (m_dataBlock[slot].get_state_local() != item_state::empty) {
		--m_writeSlot;
		return false;
	}

	write_in(slot, std::forward<In>(in));

	m_dataBlock[slot].set_state_local(item_state::valid);

	std::atomic_thread_fence(std::memory_order_release);

	m_written.store(slotTotal + 1, std::memory_order_relaxed);

	return true;
}
template<class T, class Allocator>
inline bool producer_buffer<T, Allocator>::try_pop(T& out)
{
	const size_type lastWritten(m_written.load(std::memory_order_relaxed));

	std::atomic_thread_fence(std::memory_order_acquire);

	const size_type slotReserved(m_preReadSync.fetch_add(1, std::memory_order_relaxed) + 1);
	const size_type avaliable(lastWritten - slotReserved);

	if (capacity() < avaliable) {
		m_preReadSync.fetch_sub(1, std::memory_order_relaxed);
		return false;
	}
	const size_type readSlotTotal(m_readSlot.fetch_add(1, std::memory_order_relaxed));
	const size_type readSlot(readSlotTotal & m_capacityMask);

	write_out(readSlot, out);

	post_pop_cleanup(readSlot);

	return true;
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
		if (!m_address) {
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
	producer_buffer<T, Allocator>* m_buffer = nullptr;

	union
	{
		void* m_addr = nullptr;
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
	using shared_ptr_slot_type = typename concurrent_queue<T, Allocator>::shared_ptr_slot_type;
	using allocator_adapter_type = typename concurrent_queue<T, Allocator>::allocator_adapter_type;
	using buffer_type = typename concurrent_queue<T, Allocator>::buffer_type;
	using allocator_type = typename concurrent_queue<T, Allocator>::allocator_type;
	~dummy_container()
	{

	}

	shared_ptr_slot_type m_dummyBuffer;
	buffer_type m_dummyRawBuffer;
	item_state m_dummyState;
};
template<class T, class Allocator>
inline dummy_container<T, Allocator>::dummy_container()
	: m_dummyRawBuffer(1, &m_dummyState, nullptr)
	, m_dummyState(item_state::dummy)
{
	Allocator alloc;
	m_dummyBuffer = shared_ptr_slot_type(&m_dummyRawBuffer, alloc, [](buffer_type*, Allocator&) {});
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
thread_local typename concurrent_queue<T, Allocator>::cache_container concurrent_queue<T, Allocator>::t_cachedAccesses;
}
#pragma warning(pop)
