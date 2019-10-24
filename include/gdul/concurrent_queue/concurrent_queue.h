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

#include <atomic>
#include <vector>
#include <limits>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <cassert>

// Exception handling may be enabled for basic exception safety at the cost of 
// a slight performance decrease


 /* #define CQ_ENABLE_EXCEPTIONHANDLING */

// In the event an exception is thrown during a pop operation, some entries may
// be dequeued out-of-order as some consumers may already be halfway through a 
// pop operation before reintegration efforts are started.

#ifdef CQ_ENABLE_EXCEPTIONHANDLING 
#define GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(type) (std::is_nothrow_move_assignable<type>::value)
#define GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(type) (!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(type) && (std::is_nothrow_assignable<type&, type>::value))
#define GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(type) (std::is_nothrow_move_assignable<type>::value)
#define GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(type) (std::is_nothrow_assignable<type&, type>::value)
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

namespace cqdetail
{

class producer_overflow : public std::runtime_error
{
public:
	producer_overflow(const char* errorMessage) : runtime_error(errorMessage) {}
};

static constexpr std::uint64_t Ptr_Mask = (std::uint64_t(std::numeric_limits<std::uint32_t>::max()) << 16 | std::uint64_t(std::numeric_limits<std::uint16_t>::max()));

template <class T, class Allocator>
struct accessor_cache;

template <class SharedPtrSlotType, class SharedPtrArrayType>
struct consumer_wrapper;

template <class T, class Allocator>
class producer_buffer;

template <class T>
class item_container;

template <class T, class Allocator>
class dummy_container;

template <class IndexType, class Allocator>
class index_pool;

template <class T, class Allocator>
class buffer_deleter;
template <class T, class Allocator>
class store_array_deleter;

enum class item_state : std::uint8_t;

template <class Dummy>
std::size_t log2_align(std::size_t from, std::size_t clamp);

template <class T, class Allocator>
std::size_t calc_block_size(std::size_t fromCapacity);

template <class Dummy>
inline std::uint8_t to_store_array_slot(std::uint16_t producerIndex);

template <class Dummy>
inline std::uint16_t to_store_array_capacity(std::uint16_t storeSlot);

template <class Dummy>
constexpr std::size_t next_aligned_to(std::size_t addr, std::size_t align);

template <class Dummy>
constexpr std::size_t aligned_size(std::size_t byteSize, std::size_t align);

template <class T, class Allocator>
class shared_ptr_allocator_adaptor;

// The maximum allowed consumption from a producer per visit
static const std::uint16_t Consumer_Force_Relocation_Pop_Count = 24;
// Maximum number of times the producer slot array can grow
static constexpr std::uint8_t Producer_Slots_Max_Growth_Count = 15;

static constexpr std::uint16_t Max_Producers = std::numeric_limits<int16_t>::max() - 1;

typedef std::size_t size_type;

// Not quite size_type max because we need some leaway in case we
// need to throw consumers out of a buffer whilst repairing it
static constexpr size_type Buffer_Capacity_Max = ~(std::numeric_limits<size_type>::max() >> 3) / 2;
}
// The WizardLoaf MPMC unbounded lock-free queue.
// FIFO is respected within the context of single producers. 
// Basic exception safety may be enabled via define CQ_ENABLE_EXCEPTIONHANDLING 
// at the price of a slight performance decrease.
template <class T, class Allocator = std::allocator<std::uint8_t>>
class concurrent_queue
{
public:
	typedef cqdetail::size_type size_type;

	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	inline concurrent_queue();
	inline concurrent_queue(Allocator allocator);
	inline concurrent_queue(size_type initProducerCapacity);
	inline concurrent_queue(size_type initProducerCapacity, Allocator allocator);
	inline ~concurrent_queue() noexcept;

	inline void push(const T& in);
	inline void push(T&& in);

	bool try_pop(T& out);

	// reserves a minimum capacity for the calling producer
	inline void reserve(size_type capacity);

	// size hint
	inline size_type size();

	// fast unsafe size hint
	inline size_type unsafe_size();

	// logically remove entries
	void unsafe_clear();

	// reset the structure to its initial state
	void unsafe_reset();

private:
	friend class cqdetail::producer_buffer<T, allocator_type>;
	friend class cqdetail::dummy_container<T, allocator_type>;

	typedef cqdetail::producer_buffer<T, allocator_type> buffer_type;
	typedef cqdetail::shared_ptr_allocator_adaptor<std::uint8_t, allocator_type> allocator_adapter_type;
	typedef shared_ptr<buffer_type, allocator_adapter_type> shared_ptr_slot_type;
	typedef atomic_shared_ptr<buffer_type, allocator_adapter_type> atomic_shared_ptr_slot_type;
	typedef atomic_shared_ptr<atomic_shared_ptr_slot_type, allocator_type> atomic_shared_ptr_array_type;
	typedef shared_ptr<atomic_shared_ptr_slot_type, allocator_type> shared_ptr_array_type;

	using consumer_vector_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<cqdetail::consumer_wrapper<shared_ptr_slot_type, shared_ptr_array_type>>;
	using producer_vector_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<shared_ptr_slot_type>;

	template <class ...Arg>
	void push_internal(Arg&&... in);

	inline void init_producer(size_type withCapacity);

	inline bool relocate_consumer();

	inline shared_ptr_slot_type create_producer_buffer(std::size_t withSize);
	inline void push_producer_buffer(shared_ptr_slot_type buffer);
	inline void try_alloc_produer_store_slot(std::uint8_t storeArraySlot);
	inline void try_swap_producer_array(std::uint8_t aromStoreArraySlot);
	inline void try_swap_producer_count(std::uint16_t toValue);
	inline void try_swap_producer_array_capacity(std::uint16_t toCapacity);
	inline bool has_producer_array_been_superceeded(std::uint16_t arraySlot);
	inline std::uint16_t claim_store_slot();
	inline shared_ptr_slot_type fetch_from_store(std::uint16_t bufferSlot);
	inline bool insert_to_store(shared_ptr_slot_type buffer, std::uint16_t bufferSlot, std::uint16_t storeSlot);
	inline void kill_store_array_below(std::uint16_t belowSlot, bool doInvalidate = false);

	inline shared_ptr_slot_type& this_producer();
	inline cqdetail::consumer_wrapper<shared_ptr_slot_type, shared_ptr_array_type>& this_consumer();

	inline buffer_type* this_producer_cached();
	inline buffer_type* this_consumer_cached();

	inline void refresh_cached_consumer();
	inline void refresh_cached_producer();

	const size_type myInitBufferCapacity;
	const size_type myInstanceIndex;

	static thread_local std::vector<shared_ptr_slot_type, producer_vector_allocator> ourProducers;
	static thread_local std::vector<cqdetail::consumer_wrapper<shared_ptr_slot_type, shared_ptr_array_type>, consumer_vector_allocator> ourConsumers;

	static thread_local cqdetail::accessor_cache<T, allocator_type> ourLastConsumerCache;
	static thread_local cqdetail::accessor_cache<T, allocator_type> ourLastProducerCache;

	static cqdetail::index_pool<size_type, allocator_type> ourIndexPool;
	static cqdetail::dummy_container<T, allocator_type> ourDummyContainer;

	atomic_shared_ptr_array_type myProducerArrayStore[cqdetail::Producer_Slots_Max_Growth_Count];
	atomic_shared_ptr_array_type myProducerSlots;

	GDUL_ATOMIC_WITH_VIEW(std::uint16_t, myProducerCount);
	GDUL_ATOMIC_WITH_VIEW(std::uint16_t, myProducerCapacity);

	std::atomic<std::uint16_t> myRelocationIndex;
	std::atomic<std::uint16_t> myProducerSlotReservation;
	std::atomic<std::uint16_t> myProducerSlotPostIterator;

	allocator_type myAllocator;
};

template<class T, class Allocator>
inline concurrent_queue<T, Allocator>::concurrent_queue()
	: concurrent_queue<T, Allocator>(allocator_type())
{
}
template<class T, class Allocator>
inline concurrent_queue<T, Allocator>::concurrent_queue(Allocator allocator)
	: concurrent_queue<T, Allocator>(2, allocator)
{
}
template<class T, class Allocator>
inline concurrent_queue<T, Allocator>::concurrent_queue(typename concurrent_queue<T, Allocator>::size_type initProducerCapacity)
	: concurrent_queue<T, Allocator>(initProducerCapacity, Allocator())
{
}
template<class T, class Allocator>
inline concurrent_queue<T, Allocator>::concurrent_queue(size_type initProducerCapacity, Allocator allocator)
	: myInstanceIndex(ourIndexPool.get())
	, myProducerCapacity(0)
	, myProducerCount(0)
	, myProducerSlotPostIterator(0)
	, myProducerSlotReservation(0)
	, myProducerSlots(nullptr)
	, myInitBufferCapacity(cqdetail::log2_align<void>(initProducerCapacity, cqdetail::Buffer_Capacity_Max))
	, myProducerArrayStore{ nullptr }
	, myAllocator(allocator)
	, myRelocationIndex(0)
{
}
template<class T, class Allocator>
inline concurrent_queue<T, Allocator>::~concurrent_queue() noexcept
{
	unsafe_reset();

	ourIndexPool.add(myInstanceIndex);
}

template<class T, class Allocator>
void concurrent_queue<T, Allocator>::push(const T & in)
{
	push_internal<const T&>(in);
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::push(T && in)
{
	push_internal<T&&>(std::move(in));
}
template<class T, class Allocator>
template<class ...Arg>
inline void concurrent_queue<T, Allocator>::push_internal(Arg&& ...in)
{
	buffer_type* const cachedProducer(this_producer_cached());

	if (!cachedProducer->try_push(std::forward<Arg>(in)...)) {
		if (cachedProducer->is_valid()) {
			shared_ptr_slot_type next(create_producer_buffer(std::size_t(cachedProducer->get_capacity()) * 2));
			cachedProducer->push_front(next);
			this_producer() = std::move(next);
		}
		else {
			init_producer(myInitBufferCapacity);
		}

		refresh_cached_producer();

		this_producer_cached()->try_push(std::forward<Arg>(in)...);
	}
}
template<class T, class Allocator>
bool concurrent_queue<T, Allocator>::try_pop(T & out)
{
	const std::uint16_t producers(myProducerCount.load(std::memory_order_relaxed));

	if (!this_consumer_cached()->try_pop(out)) {
		std::uint16_t retry(0);
		do {
			if (!(retry++ < producers)) {
				return false;
			}
			if (!relocate_consumer()) {
				return false;
			}
		} while (!this_consumer_cached()->try_pop(out));
	}

	if ((1 < producers) & !(++ourLastConsumerCache.myCounter < cqdetail::Consumer_Force_Relocation_Pop_Count)) {
		relocate_consumer();
		ourLastConsumerCache.myCounter = 0;
	}

	return true;
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::reserve(typename concurrent_queue<T, Allocator>::size_type capacity)
{
	shared_ptr_slot_type& producer(this_producer());

	if (!producer->is_valid()) {
		init_producer(capacity);
	}
	else if (producer->get_capacity() < capacity) {
		const size_type log2Capacity(cqdetail::log2_align<void>(capacity, cqdetail::Buffer_Capacity_Max));
		shared_ptr_slot_type buffer(create_producer_buffer(log2Capacity));
		producer->push_front(buffer);
		producer = std::move(buffer);
	}
	refresh_cached_producer();
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::unsafe_clear()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	shared_ptr_array_type producerArray(myProducerSlots.unsafe_load(std::memory_order_relaxed));
	for (std::uint16_t i = 0; i < myProducerCount.load(std::memory_order_relaxed); ++i) {
		producerArray[i].unsafe_get_owned()->unsafe_clear();
	}

	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::unsafe_reset()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	const std::uint16_t producerCount(myProducerCount.load(std::memory_order_relaxed));
	const std::uint16_t slots(cqdetail::to_store_array_slot<void>(producerCount - static_cast<bool>(producerCount)) + static_cast<bool>(producerCount));

	kill_store_array_below(slots, true);

	myRelocationIndex.store(0, std::memory_order_relaxed);
	myProducerCapacity.store(0, std::memory_order_relaxed);
	myProducerCount.store(0, std::memory_order_relaxed);
	myProducerSlotPostIterator.store(0, std::memory_order_relaxed);
	myProducerSlotReservation.store(0, std::memory_order_relaxed);

	for (std::uint16_t i = 0; i < producerCount; ++i) {
		myProducerSlots.unsafe_get_owned()[i].unsafe_store(nullptr, std::memory_order_relaxed);
	}

	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::size_type concurrent_queue<T, Allocator>::size()
{
	const std::uint16_t producerCount(myProducerCount.load(std::memory_order_acquire));

	shared_ptr_array_type producerArray(myProducerSlots.load(std::memory_order_relaxed));

	size_type accumulatedSize(0);
	for (std::uint16_t i = 0; i < producerCount; ++i) {
		shared_ptr_slot_type slot(producerArray[i].load(std::memory_order_relaxed));
		accumulatedSize += slot->size();
	}
	return accumulatedSize;
}
template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::size_type concurrent_queue<T, Allocator>::unsafe_size()
{
	const std::uint16_t producerCount(myProducerCount.load(std::memory_order_acquire));

	atomic_shared_ptr_slot_type* const producerArray(myProducerSlots.unsafe_get_owned(std::memory_order_relaxed));

	size_type accumulatedSize(0);
	for (std::uint16_t i = 0; i < producerCount; ++i) {
		buffer_type* const slot(producerArray[i].unsafe_get_owned());
		accumulatedSize += slot->size();
	}
	return accumulatedSize;
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::init_producer(typename concurrent_queue<T, Allocator>::size_type withCapacity)
{
	shared_ptr_slot_type newBuffer(create_producer_buffer(withCapacity));
	push_producer_buffer(newBuffer);
	this_producer() = std::move(newBuffer);
}
template<class T, class Allocator>
inline bool concurrent_queue<T, Allocator>::relocate_consumer()
{
	const std::uint16_t producers(myProducerCount.load(std::memory_order_acquire));
	if (producers == 1) {
		buffer_type* const cached(this_consumer_cached());
		if (cached->is_valid() & cached->is_active()) {
			return false;
		}
	}

	const std::uint16_t relocation(myRelocationIndex.fetch_add(1, std::memory_order_relaxed));
	const std::uint16_t maxVisited(producers - static_cast<bool>(producers) + !(producers - static_cast<bool>(producers)));

	cqdetail::consumer_wrapper<shared_ptr_slot_type, shared_ptr_array_type>& consumer(this_consumer());
	if (consumer.myLastKnownArray != myProducerSlots) {
		consumer.myLastKnownArray = myProducerSlots.load(std::memory_order_relaxed);
	}

	for (std::uint16_t i = 0, j = relocation; i < maxVisited; ++i, ++j) {
		const std::uint16_t entry(j % producers);
		shared_ptr_slot_type producerBuffer(consumer.myLastKnownArray[entry].load(std::memory_order_relaxed));

		if (!producerBuffer) {
			consumer.myLastKnownArray = myProducerSlots.load(std::memory_order_relaxed);
			--i; --j;
			continue;
		}

		if (!producerBuffer->is_valid() | !producerBuffer->size()) {
			assert(producerBuffer->is_valid() && "An invalid buffer should not exist in the queue structure. Rogue 'unsafe_reset()' ?");
			continue;
		}

		if (!producerBuffer->is_active()) {
			shared_ptr_slot_type successor = producerBuffer->find_back();
			if (successor) {
				if (producerBuffer->verify_successor(successor)) {
					producerBuffer = std::move(successor);
					consumer.myLastKnownArray[entry].store(producerBuffer, std::memory_order_relaxed);
				}
			}
		}

		consumer.myBuffer = std::move(producerBuffer);
		consumer.myPopCounter = 0;

		refresh_cached_consumer();

		return true;
	}
	return false;
}
template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::shared_ptr_slot_type concurrent_queue<T, Allocator>::create_producer_buffer(std::size_t withSize)
{
	const std::size_t log2size(cqdetail::log2_align<void>(withSize, cqdetail::Buffer_Capacity_Max));

	const std::size_t alignOfData(alignof(T));

	const std::size_t bufferByteSize(sizeof(buffer_type));
	const std::size_t dataBlockByteSize(sizeof(cqdetail::item_container<T>) * log2size);
	constexpr std::size_t controlBlockByteSize(shared_ptr_slot_type::alloc_size_claim());

	constexpr std::size_t controlBlockSize(cqdetail::aligned_size<void>(controlBlockByteSize, 8));
	constexpr std::size_t bufferSize(cqdetail::aligned_size<void>(bufferByteSize, 8));
	const std::size_t dataBlockSize(dataBlockByteSize);

	const std::size_t totalBlockSize(controlBlockSize + bufferSize + dataBlockSize + alignOfData);

	std::uint8_t* totalBlock(nullptr);

	buffer_type* buffer(nullptr);
	cqdetail::item_container<T>* data(nullptr);

#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	try {
#endif
		totalBlock = myAllocator.allocate(totalBlockSize);

		const std::size_t totalBlockBegin(reinterpret_cast<std::size_t>(totalBlock));
		const std::size_t controlBlockBegin(totalBlockBegin);
		const std::size_t bufferBegin(controlBlockBegin + controlBlockSize);

		const std::size_t bufferEnd(bufferBegin + bufferSize);
		const std::size_t dataBeginOffset((bufferEnd % alignOfData ? (alignOfData - (bufferEnd % alignOfData)) : 0));
		const std::size_t dataBegin(bufferEnd + dataBeginOffset);

		const std::size_t bufferOffset(bufferBegin - totalBlockBegin);
		const std::size_t dataOffset(dataBegin - totalBlockBegin);

		// new (addr) (arr[n]) is unreliable...
		data = reinterpret_cast<cqdetail::item_container<T>*>(totalBlock + dataOffset);
		for (std::size_t i = 0; i < log2size; ++i) {
			cqdetail::item_container<T>* const item(&data[i]);
			new (item) (cqdetail::item_container<T>);
		}

		buffer = new(totalBlock + bufferOffset) buffer_type(static_cast<size_type>(log2size), data);
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	}
	catch (...) {
		myAllocator.deallocate(totalBlock, totalBlockSize);
		throw;
	}
#endif

	allocator_adapter_type allocAdaptor(totalBlock, totalBlockSize);


	shared_ptr_slot_type returnValue(buffer, cqdetail::buffer_deleter<buffer_type, allocator_adapter_type>(), allocAdaptor);

	return returnValue;
}
// Find a slot for the buffer in the producer store. Also, update the active producer 
// array, capacity and producer count as is necessary. In the event a new producer array 
// needs to be allocated, threads will compete to do so.
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::push_producer_buffer(shared_ptr_slot_type buffer)
{
	const std::uint16_t bufferSlot(claim_store_slot());
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	if (!(bufferSlot < cqdetail::Max_Producers)) {
		throw cqdetail::producer_overflow("Max producers exceeded");
	}
#endif
	insert_to_store(std::move(buffer), bufferSlot, cqdetail::to_store_array_slot<void>(bufferSlot));

	const std::uint16_t postIterator(myProducerSlotPostIterator.fetch_add(1, std::memory_order_seq_cst) + 1);
	const std::uint16_t numReserved(myProducerSlotReservation.load(std::memory_order_seq_cst));

	// If postIterator and numReserved match, that means all indices below postIterator have been properly inserted
	if (postIterator == numReserved) {

		for (std::uint16_t i = 0; i < postIterator; ++i) {
			if (!insert_to_store(fetch_from_store(i), i, cqdetail::to_store_array_slot<void>(postIterator - 1))) {
				return;
			}
		}

		try_swap_producer_array(cqdetail::to_store_array_slot<void>(postIterator - 1));
		try_swap_producer_count(postIterator);

		kill_store_array_below(cqdetail::to_store_array_slot<void>(postIterator - 1));
	}
}
// Allocate a buffer array of capacity appropriate to the slot
// and attempt to swap the current value for the new one
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::try_alloc_produer_store_slot(std::uint8_t storeArraySlot)
{
	const std::uint16_t producerCapacity(static_cast<std::uint16_t>(powf(2.f, static_cast<float>(storeArraySlot + 1))));

	const std::size_t blockSize(sizeof(atomic_shared_ptr_slot_type) * producerCapacity);

	std::uint8_t* const block(myAllocator.allocate(blockSize));

	cqdetail::store_array_deleter<atomic_shared_ptr_slot_type, allocator_type> deleter(producerCapacity);
	shared_ptr_array_type desired(reinterpret_cast<atomic_shared_ptr_slot_type*>(block), std::move(deleter), myAllocator);

	for (std::size_t i = 0; i < producerCapacity; ++i) {
		atomic_shared_ptr_slot_type* const item(&desired[i]);
		new (item) (atomic_shared_ptr_slot_type);
	}

	shared_ptr_array_type expected(nullptr, myProducerArrayStore[storeArraySlot].get_version());

	myProducerArrayStore[storeArraySlot].compare_exchange_strong(expected, std::move(desired), std::memory_order_acq_rel, std::memory_order_relaxed);
}
// Try swapping the current producer array for one from the store, and follow up
// with an attempt to swap the capacity value for the one corresponding to the slot
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::try_swap_producer_array(std::uint8_t fromStoreArraySlot)
{
	shared_ptr_array_type expectedProducerArray(myProducerSlots.load(std::memory_order_acquire));
	for (;;) {
		if (has_producer_array_been_superceeded(fromStoreArraySlot)) {
			break;
		}

		shared_ptr_array_type desired(myProducerArrayStore[fromStoreArraySlot].load(std::memory_order_relaxed));

		if (myProducerSlots.compare_exchange_strong(expectedProducerArray, desired, std::memory_order_release, std::memory_order_relaxed)) {
			try_swap_producer_array_capacity(cqdetail::to_store_array_capacity<void>(fromStoreArraySlot));
			break;
		}
	}
}
// Attempt to swap the producer count value for the arg value if the
// existing one is lower
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::try_swap_producer_count(std::uint16_t toValue)
{
	const std::uint16_t desired(toValue);
	for (std::uint16_t i = myProducerCount.load(std::memory_order_relaxed); i < desired;) {

		std::uint16_t& expected(i);
		if (myProducerCount.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed)) {
			break;
		}
	}
}
template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::try_swap_producer_array_capacity(std::uint16_t toCapacity)
{
	const std::uint16_t desiredCapacity(toCapacity);
	std::uint16_t expectedCapacity(myProducerCapacity.load(std::memory_order_relaxed));

	for (; expectedCapacity < desiredCapacity;) {
		if (myProducerCapacity.compare_exchange_strong(expectedCapacity, desiredCapacity, std::memory_order_release, std::memory_order_relaxed)) {
			break;
		}
	}
}
template<class T, class Allocator>
inline bool concurrent_queue<T, Allocator>::has_producer_array_been_superceeded(std::uint16_t arraySlot)
{
	shared_ptr_array_type activeArray(myProducerSlots.load(std::memory_order_relaxed));

	if (!activeArray) {
		return false;
	}

	for (std::uint8_t higherStoreSlot = arraySlot + 1; higherStoreSlot < cqdetail::Producer_Slots_Max_Growth_Count; ++higherStoreSlot) {
		if (myProducerArrayStore[higherStoreSlot].load(std::memory_order_relaxed).get_owned() == activeArray.get_owned()) {
			return true;
		}
	}

	return false;
}
template<class T, class Allocator>
inline std::uint16_t concurrent_queue<T, Allocator>::claim_store_slot()
{
	std::uint16_t reservedSlot(std::numeric_limits<std::uint16_t>::max());
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	for (bool foundSlot(false); !foundSlot;) {
		std::uint16_t expectedSlot(myProducerSlotReservation.load(std::memory_order_relaxed));
		const std::uint8_t storeArraySlot(cqdetail::to_store_array_slot<void>(expectedSlot));
		try_alloc_produer_store_slot(storeArraySlot);
		do {
			if (myProducerSlotReservation.compare_exchange_strong(expectedSlot, expectedSlot + 1, std::memory_order_release, std::memory_order_relaxed)) {
				reservedSlot = expectedSlot;
				foundSlot = true;
				break;
			}
		} while (cqdetail::to_store_array_slot<void>(expectedSlot) == storeArraySlot);
	}
#else
	reservedSlot = myProducerSlotReservation.fetch_add(1, std::memory_order_relaxed);
	const std::uint8_t storeArraySlot(cqdetail::to_store_array_slot<void>(reservedSlot));
	if (!myProducerArrayStore[storeArraySlot]) {
		try_alloc_produer_store_slot(storeArraySlot);
	}
#endif
	return reservedSlot;
}
template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::shared_ptr_slot_type concurrent_queue<T, Allocator>::fetch_from_store(std::uint16_t bufferSlot)
{
	const std::uint16_t nativeArray(cqdetail::to_store_array_slot<void>(bufferSlot));
	shared_ptr_array_type producerArray(nullptr);
	shared_ptr_slot_type producerBuffer(nullptr);
	for (std::uint8_t i = nativeArray; i < cqdetail::Producer_Slots_Max_Growth_Count; ++i) {
		if (!myProducerArrayStore[i]) {
			continue;
		}

		producerArray = myProducerArrayStore[i].load(std::memory_order_relaxed);

		if (!producerArray) {
			continue;
		}
		shared_ptr_slot_type buffer(producerArray[bufferSlot].load(std::memory_order_relaxed));
		if (!buffer) {
			continue;
		}
		producerBuffer = std::move(buffer);
	}
	if (!producerBuffer)
		throw std::runtime_error("fetch_from_store found no entry when an existing entry should be guaranteed");

	return producerBuffer;
}
template<class T, class Allocator>
inline bool concurrent_queue<T, Allocator>::insert_to_store(shared_ptr_slot_type buffer, std::uint16_t bufferSlot, std::uint16_t storeSlot)
{
	shared_ptr_array_type producerArray(myProducerArrayStore[storeSlot].load(std::memory_order_relaxed));

	if (!producerArray) {
		return false;
	}
	producerArray[bufferSlot].store(std::move(buffer), std::memory_order_relaxed);

	return true;
}

template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::kill_store_array_below(std::uint16_t belowSlot, bool doInvalidate)
{
	for (std::uint16_t i = 0; i < belowSlot; ++i) {
		const std::uint16_t slotSize(static_cast<std::uint16_t>(std::pow(2, i + 1)));
		shared_ptr_array_type storeSlot(myProducerArrayStore[i].exchange(nullptr, std::memory_order_release));
		if (!storeSlot) {
			continue;
		}

		for (std::uint16_t slotIndex = 0; slotIndex < slotSize; ++slotIndex) {
			if (!storeSlot[slotIndex]) {
				continue;
			}
			shared_ptr_slot_type slot(storeSlot[slotIndex].exchange(nullptr));

			if (slot && doInvalidate) {
				slot->unsafe_clear();
				slot->invalidate();
			}
		}
	}
}

template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::shared_ptr_slot_type & concurrent_queue<T, Allocator>::this_producer()
{
	if (!(myInstanceIndex < ourProducers.size())) {
		ourProducers.resize(myInstanceIndex + 1, ourDummyContainer.myDummyBuffer);
	}
	return ourProducers[myInstanceIndex];
}
template<class T, class Allocator>
inline cqdetail::consumer_wrapper<typename concurrent_queue<T, Allocator>::shared_ptr_slot_type, typename concurrent_queue<T, Allocator>::shared_ptr_array_type>& concurrent_queue<T, Allocator>::this_consumer()
{
	if (!(myInstanceIndex < ourConsumers.size())) {
		ourConsumers.resize(myInstanceIndex + 1, cqdetail::consumer_wrapper<shared_ptr_slot_type, shared_ptr_array_type>(ourDummyContainer.myDummyBuffer));
	}
	return ourConsumers[myInstanceIndex];
}

template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::buffer_type* concurrent_queue<T, Allocator>::this_producer_cached()
{
	if ((ourLastProducerCache.myAddrBlock & cqdetail::Ptr_Mask) ^ reinterpret_cast<std::uint64_t>(this)) {
		refresh_cached_producer();
	}
	return ourLastProducerCache.myBuffer;
}

template<class T, class Allocator>
inline typename concurrent_queue<T, Allocator>::buffer_type* concurrent_queue<T, Allocator>::this_consumer_cached()
{
	if ((ourLastConsumerCache.myAddrBlock & cqdetail::Ptr_Mask) ^ reinterpret_cast<std::uint64_t>(this)) {
		refresh_cached_consumer();
	}
	return ourLastConsumerCache.myBuffer;
}

template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::refresh_cached_consumer()
{
	ourLastConsumerCache.myBuffer = static_cast<buffer_type*>(this_consumer().myBuffer);
	ourLastConsumerCache.myAddr = this;
	ourLastConsumerCache.myCounter = this_consumer().myPopCounter;
}

template<class T, class Allocator>
inline void concurrent_queue<T, Allocator>::refresh_cached_producer()
{
	ourLastProducerCache.myBuffer = static_cast<buffer_type*>(this_producer());
	ourLastProducerCache.myAddr = this;
}

namespace cqdetail
{

template <class T, class Allocator>
class producer_buffer
{
private:
	typedef typename concurrent_queue<T, Allocator>::shared_ptr_slot_type shared_ptr_slot_type;

public:
	typedef typename concurrent_queue<T, Allocator>::size_type size_type;
	typedef typename concurrent_queue<T, Allocator>::allocator_type allocator_type;

	producer_buffer(size_type capacity, item_container<T>* dataBlock);
	~producer_buffer();

	template<class ...Arg>
	inline bool try_push(Arg&&... in);
	inline bool try_pop(T& out);

	inline size_type size() const;
	inline size_type get_capacity() const;

	// Makes sure that it is entirely safe to replace this buffer with a successor
	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>* = nullptr>
	inline bool verify_successor(const shared_ptr_slot_type&);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>* = nullptr>
	inline bool verify_successor(const shared_ptr_slot_type& successor);

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

	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)>* = nullptr>
	inline void write_in(size_type slot, U&& in);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)>* = nullptr>
	inline void write_in(size_type slot, U&& in);
	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)>* = nullptr>
	inline void write_in(size_type slot, const U& in);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)>* = nullptr>
	inline void write_in(size_type slot, const U& in);

	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U)>* = nullptr>
	inline void write_out(size_type slot, U& out);
	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>* = nullptr>
	inline void write_out(size_type slot, U& out);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>* = nullptr>
	inline void write_out(size_type slot, U& out);

	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>* = nullptr>
	inline void post_pop_cleanup(size_type readSlot);
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>* = nullptr>
	inline void post_pop_cleanup(size_type readSlot);

	template <class U = T, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>* = nullptr>
	inline void check_for_damage();
	template <class U = T, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>* = nullptr>
	inline void check_for_damage();

	inline void reintegrate_failed_entries(size_type failCount);

	static constexpr size_type Buffer_Lock_Offset = cqdetail::Buffer_Capacity_Max + Max_Producers;

	std::atomic<size_type> myPreReadIterator;
	GDUL_ATOMIC_WITH_VIEW(size_type, myReadSlot);

#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	std::atomic<size_type> myPostReadIterator;
	std::atomic<std::uint16_t> myFailiureCount;
	std::atomic<std::uint16_t> myFailiureIndex;
	GDUL_CQ_PADDING((GDUL_CACHELINE_SIZE * 2) - ((sizeof(size_type) * 3) + (sizeof(std::uint16_t) * 2)));

#else
	GDUL_CQ_PADDING((GDUL_CACHELINE_SIZE * 2) - sizeof(size_type) * 2);
#endif
	size_type myWriteSlot;
	GDUL_ATOMIC_WITH_VIEW(size_type, myPostWriteIterator);
	GDUL_CQ_PADDING(GDUL_CACHELINE_SIZE - sizeof(size_type) * 2);
	shared_ptr_slot_type myNext;

	const size_type myCapacity;
	item_container<T>* const myDataBlock;

	std::atomic<bool> myNextState;
};
template<class T, class Allocator>
inline producer_buffer<T, Allocator>::producer_buffer(typename producer_buffer<T, Allocator>::size_type capacity, item_container<T>* dataBlock)
	: myNext(nullptr)
	, myDataBlock(dataBlock)
	, myCapacity(capacity)
	, myReadSlot(0)
	, myPreReadIterator(0)
	, myWriteSlot(0)
	, myPostWriteIterator(0)
	, myNextState(false)
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	, myFailiureIndex(0)
	, myFailiureCount(0)
	, myPostReadIterator(0)
#endif
{
}

template<class T, class Allocator>
inline producer_buffer<T, Allocator>::~producer_buffer()
{
	for (size_type i = 0; i < myCapacity; ++i) {
		myDataBlock[i].~item_container<T>();
	}
}
template<class T, class Allocator>
inline bool producer_buffer<T, Allocator>::is_active() const
{
	return (!myNext || (myReadSlot.load(std::memory_order_relaxed) != myPostWriteIterator.load(std::memory_order_relaxed)));
}
template<class T, class Allocator>
inline bool producer_buffer<T, Allocator>::is_valid() const
{
	return myDataBlock[myWriteSlot % myCapacity].get_state_local() != item_state::Dummy;
}
template<class T, class Allocator>
inline void producer_buffer<T, Allocator>::invalidate()
{
	myDataBlock[myWriteSlot % myCapacity].set_state(item_state::Dummy);
	if (myNext) {
		myNext->invalidate();
	}
}
template<class T, class Allocator>
inline typename producer_buffer<T, Allocator>::shared_ptr_slot_type producer_buffer<T, Allocator>::find_back()
{
	bool nextState(myNextState.load(std::memory_order_seq_cst));
	shared_ptr_slot_type back(nullptr);
	producer_buffer<T, allocator_type>* inspect(this);

	while (nextState) {
		const size_type readSlot(inspect->myReadSlot.load(std::memory_order_relaxed));
		const size_type postWrite(inspect->myPostWriteIterator.load(std::memory_order_relaxed));

		const bool thisBufferEmpty(readSlot == postWrite);
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
		const bool veto(inspect->myFailiureCount.load(std::memory_order_relaxed) != inspect->myFailiureIndex.load(std::memory_order_relaxed));
		const bool valid(!thisBufferEmpty | veto);
#else
		const bool valid(!thisBufferEmpty);
#endif
		if (valid) {
			break;
		}
		std::atomic_thread_fence(std::memory_order_seq_cst);

		back = inspect->myNext;
		inspect = back.get_owned();
		nextState = back->myNextState.load(std::memory_order_relaxed);
	}
	return back;
}

template<class T, class Allocator>
inline typename producer_buffer<T, Allocator>::size_type producer_buffer<T, Allocator>::size() const
{
	const size_type readSlot(myReadSlot.load(std::memory_order_relaxed));
	size_type accumulatedSize(myPostWriteIterator.load(std::memory_order_relaxed));
	accumulatedSize -= readSlot;

	if (myNextState.load(std::memory_order_seq_cst))
		accumulatedSize += myNext->size();

	return accumulatedSize;
}

template<class T, class Allocator>
inline typename producer_buffer<T, Allocator>::size_type producer_buffer<T, Allocator>::get_capacity() const
{
	return myCapacity;
}
template<class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline bool producer_buffer<T, Allocator>::verify_successor(const shared_ptr_slot_type&)
{
	return true;
}
template<class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline bool producer_buffer<T, Allocator>::verify_successor(const shared_ptr_slot_type& successor)
{
	if (!myNextState.load(std::memory_order_seq_cst)) {
		return false;
	}

	shared_ptr_slot_type next(nullptr);
	producer_buffer<T, Allocator>* inspect(this);

	do {
		const size_type preRead(inspect->myPreReadIterator.load(std::memory_order_relaxed));
		for (size_type i = 0; i < inspect->myCapacity; ++i) {
			const size_type index((preRead - i) % inspect->myCapacity);

			if (inspect->myDataBlock[index].get_state_local() != item_state::Empty) {
				return false;
			}
		}

		std::atomic_thread_fence(std::memory_order_seq_cst);

		next = inspect->myNext;
		inspect = next.get_owned();

		if (inspect == successor.get_owned()) {
			break;
		}
	} while (next->myNextState.load(std::memory_order_relaxed));

	return true;
}
template<class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void producer_buffer<T, Allocator>::check_for_damage()
{
}
template<class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void producer_buffer<T, Allocator>::check_for_damage()
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	const size_type preRead(myPreReadIterator.load(std::memory_order_relaxed));
	const size_type preReadLockOffset(preRead - Buffer_Lock_Offset);
	if (preReadLockOffset != myPostReadIterator.load(std::memory_order_relaxed)) {
		return;
	}

	const std::uint16_t failiureIndex(myFailiureIndex.load(std::memory_order_relaxed));
	const std::uint16_t failiureCount(myFailiureCount.load(std::memory_order_relaxed));
	const std::uint16_t difference(failiureCount - failiureIndex);

	const bool failCheckA(0 == difference);
	const bool failCheckB(!(difference < cqdetail::Max_Producers));
	if (failCheckA | failCheckB) {
		return;
	}

	const size_type toReintegrate(failiureCount - failiureIndex);

	std::uint16_t expected(failiureIndex);
	const std::uint16_t desired(failiureCount);
	if (myFailiureIndex.compare_exchange_strong(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed)) {
		reintegrate_failed_entries(toReintegrate);

		myPostReadIterator.fetch_sub(toReintegrate);
		myReadSlot.fetch_sub(toReintegrate);
		myPreReadIterator.fetch_sub(Buffer_Lock_Offset + toReintegrate);
	}
#endif
}
template<class T, class Allocator>
inline void producer_buffer<T, Allocator>::push_front(shared_ptr_slot_type newBuffer)
{
	producer_buffer<T, allocator_type>* last(this);
	raw_ptr<typename shared_ptr_slot_type::value_type, typename shared_ptr_slot_type::allocator_type> verLast(nullptr);

	while (last->myNext) {
		verLast = last->myNext;
		last = verLast.get_owned();
	}
	last->myNext = std::move(newBuffer);

	last->myNextState.store(true, std::memory_order_seq_cst);
}
template<class T, class Allocator>
inline void producer_buffer<T, Allocator>::unsafe_clear()
{
	const size_type postWrite(myPostWriteIterator.load(std::memory_order_relaxed));
	myPreReadIterator.store(postWrite, std::memory_order_relaxed);
	myReadSlot.store(postWrite, std::memory_order_relaxed);

	for (size_type i = 0; i < myCapacity; ++i) {
		myDataBlock[i].set_state_local(item_state::Empty);
	}

#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	myFailiureCount.store(0, std::memory_order_relaxed);
	myFailiureIndex.store(0, std::memory_order_relaxed);

	myPostReadIterator.store(postWrite, std::memory_order_relaxed);
#endif
	if (myNext) {
		myNext->unsafe_clear();
	}
}
template<class T, class Allocator>
template<class ...Arg>
inline bool producer_buffer<T, Allocator>::try_push(Arg && ...in)
{
	const size_type slotTotal(myWriteSlot++);
	const size_type slot(slotTotal % myCapacity);

	std::atomic_thread_fence(std::memory_order_acquire);

	if (myDataBlock[slot].get_state_local() != item_state::Empty) {
		--myWriteSlot;
		return false;
	}

	write_in(slot, std::forward<Arg>(in)...);

	myDataBlock[slot].set_state_local(item_state::Valid);

	myPostWriteIterator.fetch_add(1, std::memory_order_release);

	return true;
}
template<class T, class Allocator>
inline bool producer_buffer<T, Allocator>::try_pop(T & out)
{
	const size_type lastWritten(myPostWriteIterator.load(std::memory_order_relaxed));
	const size_type slotReserved(myPreReadIterator.fetch_add(1, std::memory_order_relaxed) + 1);
	const size_type avaliable(lastWritten - slotReserved);

	if (myCapacity < avaliable) {
		myPreReadIterator.fetch_sub(1, std::memory_order_relaxed);
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
		check_for_damage();
#endif
		return false;
	}
	const size_type readSlotTotal(myReadSlot.fetch_add(1, std::memory_order_relaxed));
	const size_type readSlot(readSlotTotal % myCapacity);

	write_out(readSlot, out);

	post_pop_cleanup(readSlot);

	return true;
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)>*>
inline void producer_buffer<T, Allocator>::write_in(typename producer_buffer<T, Allocator>::size_type slot, U&& in)
{
	myDataBlock[slot].store(std::move(in));
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_MOVE(U)>*>
inline void producer_buffer<T, Allocator>::write_in(typename producer_buffer<T, Allocator>::size_type slot, U&& in)
{
	try {
		myDataBlock[slot].store(std::move(in));
	}
	catch (...) {
		--myWriteSlot;
		throw;
	}
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)>*>
inline void producer_buffer<T, Allocator>::write_in(typename producer_buffer<T, Allocator>::size_type slot, const U& in)
{
	myDataBlock[slot].store(in);
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_PUSH_ASSIGN(U)>*>
inline void producer_buffer<T, Allocator>::write_in(typename producer_buffer<T, Allocator>::size_type slot, const U& in)
{
	try {
		myDataBlock[slot].store(in);
	}
	catch (...) {
		--myWriteSlot;
		throw;
	}
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U)>*>
inline void producer_buffer<T, Allocator>::write_out(typename producer_buffer<T, Allocator>::size_type slot, U& out)
{
	myDataBlock[slot].move(out);
}
template<class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) || GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void producer_buffer<T, Allocator>::post_pop_cleanup(typename producer_buffer<T, Allocator>::size_type readSlot)
{
	myDataBlock[readSlot].set_state(item_state::Empty);
	std::atomic_thread_fence(std::memory_order_release);
}
template<class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void producer_buffer<T, Allocator>::post_pop_cleanup(typename producer_buffer<T, Allocator>::size_type readSlot)
{
	myDataBlock[readSlot].set_state(item_state::Empty);
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	myDataBlock[readSlot].reset_ref();
	myPostReadIterator.fetch_add(1, std::memory_order_release);
#else
	std::atomic_thread_fence(std::memory_order_release);
#endif
}
template <class T, class Allocator>
template <class U, std::enable_if_t<GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void producer_buffer<T, Allocator>::write_out(typename producer_buffer<T, Allocator>::size_type slot, U& out)
{
	myDataBlock[slot].assign(out);
}
template <class T, class Allocator>
template <class U, std::enable_if_t<!GDUL_CQ_BUFFER_NOTHROW_POP_MOVE(U) && !GDUL_CQ_BUFFER_NOTHROW_POP_ASSIGN(U)>*>
inline void producer_buffer<T, Allocator>::write_out(typename producer_buffer<T, Allocator>::size_type slot, U& out)
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	try {
#endif
		myDataBlock[slot].try_move(out);
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	}
	catch (...) {
		if (myFailiureCount.fetch_add(1, std::memory_order_relaxed) == myFailiureIndex.load(std::memory_order_relaxed)) {
			myPreReadIterator.fetch_add(Buffer_Lock_Offset, std::memory_order_release);
		}
		myDataBlock[slot].set_state(item_state::Failed);
		myPostReadIterator.fetch_add(1, std::memory_order_release);
		throw;
	}
#endif
}
template<class T, class Allocator>
inline void producer_buffer<T, Allocator>::reintegrate_failed_entries(typename producer_buffer<T, Allocator>::size_type failCount)
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	const size_type readSlotTotal(myReadSlot.load(std::memory_order_relaxed));
	const size_type readSlotTotalOffset(readSlotTotal + myCapacity);

	const size_type startIndex(readSlotTotalOffset - 1);

	size_type numRedirected(0);
	for (size_type i = 0, j = startIndex; numRedirected != failCount; ++i, --j) {
		const size_type currentIndex((startIndex - i) % myCapacity);
		item_container<T>& currentItem(myDataBlock[currentIndex]);
		const item_state currentState(currentItem.get_state_local());

		if (currentState == item_state::Failed) {
			const size_type toRedirectIndex((startIndex - numRedirected) % myCapacity);
			item_container<T>& toRedirect(myDataBlock[toRedirectIndex]);

			toRedirect.redirect(currentItem);
			currentItem.set_state_local(item_state::Valid);
			++numRedirected;
		}
	}
#else
	failCount;
#endif
}

// used to be able to redirect access to data in the event
// of an exception being thrown
template <class T>
class item_container
{
public:
	item_container<T>(const item_container<T>&) = delete;
	item_container<T>& operator=(const item_container&) = delete;

	inline item_container();
	inline item_container(item_state state);

	inline void store(const T& in);
	inline void store(T&& in);
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	inline void redirect(item_container<T>& to);
#endif
	template<class U = T, std::enable_if_t<std::is_move_assignable<U>::value>* = nullptr>
	inline void try_move(U& out);
	template<class U = T, std::enable_if_t<!std::is_move_assignable<U>::value>* = nullptr>
	inline void try_move(U& out);

	inline void assign(T& out);
	inline void move(T& out);

	inline item_state get_state_local() const;
	inline void set_state(item_state state);
	inline void set_state_local(item_state state);

	inline void reset_ref();

private:
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	// May or may not reference this continer
	inline item_container<T>& reference() const;

#endif
	T myData;
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	union
	{
		std::uint64_t myStateBlock;
		item_container<T>* myReference;
		struct
		{
			std::uint16_t trash[3];
			item_state myState;
		};
	};
#else
	item_state myState;
#endif
};
template<class T>
inline item_container<T>::item_container()
	: myData()
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	, myReference(this)
#else
	, myState(item_state::Empty)
#endif
{
}
template<class T>
inline item_container<T>::item_container(item_state state)
	: myData()
	, myState(state)
{
}
template<class T>
inline void item_container<T>::store(const T & in)
{
	myData = in;
	reset_ref();
}
template<class T>
inline void item_container<T>::store(T && in)
{
	myData = std::move(in);
	reset_ref();
}
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
template<class T>
inline void item_container<T>::redirect(item_container<T>& to)
{
	const std::uint64_t otherPtrBlock(to.myStateBlock & Ptr_Mask);
	myStateBlock &= ~Ptr_Mask;
	myStateBlock |= otherPtrBlock;
}
#endif
template<class T>
inline void item_container<T>::assign(T & out)
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	out = reference().myData;
#else
	out = myData;
#endif
}
template<class T>
inline void item_container<T>::move(T & out)
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	out = std::move(reference().myData);
#else
	out = std::move(myData);
#endif
}
template<class T>
inline void item_container<T>::set_state(item_state state)
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	reference().myState = state;
#else
	myState = state;
#endif
}

template<class T>
inline void item_container<T>::set_state_local(item_state state)
{
	myState = state;
}
template<class T>
inline void item_container<T>::reset_ref()
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	myReference = this;
#endif
}
template<class T>
inline item_state item_container<T>::get_state_local() const
{
	return myState;
}
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
template<class T>
inline item_container<T>& item_container<T>::reference() const
{
	return *reinterpret_cast<item_container<T>*>(myStateBlock & Ptr_Mask);
}
#endif
template<class T>
template<class U, std::enable_if_t<std::is_move_assignable<U>::value>*>
inline void item_container<T>::try_move(U& out)
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	out = std::move(reference().myData);
#else
	out = std::move(myData);
#endif
}
template<class T>
template<class U, std::enable_if_t<!std::is_move_assignable<U>::value>*>
inline void item_container<T>::try_move(U& out)
{
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	out = reference().myData;
#else
	out = myData;
#endif
}

enum class item_state : std::uint8_t
{
	Empty,
	Valid,
	Failed,
	Dummy
};
template <class Dummy>
std::size_t log2_align(std::size_t from, std::size_t clamp)
{
	const std::size_t from_(from < 2 ? 2 : from);

	const float flog2(std::log2f(static_cast<float>(from_)));
	const float nextLog2(std::ceil(flog2));
	const float fNextVal(std::powf(2.f, nextLog2));

	const std::size_t nextVal(static_cast<size_t>(fNextVal));
	const std::size_t clampedNextVal((clamp < nextVal) ? clamp : nextVal);

	return clampedNextVal;
}
template<class T, class Allocator>
std::size_t calc_block_size(std::size_t fromCapacity)
{
	typedef typename concurrent_queue<T, Allocator>::buffer_type buffer_type;
	typedef typename concurrent_queue<T, Allocator>::allocator_adaptor_type allocator_adaptor_type;
	typedef buffer_deleter<buffer_type, allocator_adaptor_type> deleter_type;

	const std::size_t log2size(cqdetail::log2_align(fromCapacity, cqdetail::Buffer_Capacity_Max));

	const std::size_t alignOfControlBlock(alignof(aspdetail::control_block_claim_custom_delete<buffer_type, allocator_adaptor_type, deleter_type>));
	const std::size_t alignOfData(alignof(T));
	const std::size_t alignOfBuffer(alignof(buffer_type));

	const std::size_t maxAlignBuffData(alignOfBuffer < alignOfData ? alignOfData : alignOfBuffer);
	const std::size_t maxAlign(maxAlignBuffData < alignOfControlBlock ? alignOfControlBlock : maxAlignBuffData);

	const std::size_t bufferByteSize(sizeof(buffer_type));
	const std::size_t dataBlockByteSize(sizeof(cqdetail::item_container<T>) * log2size);
	const std::size_t controlBlockByteSize(shared_ptr_slot_type::Alloc_Size_Claim);

	const std::size_t controlBlockSize(cqdetail::aligned_size<void>(controlBlockByteSize, maxAlign));
	const std::size_t bufferSize(cqdetail::aligned_size<void>(bufferByteSize, maxAlign));
	const std::size_t dataBlockSize(cqdetail::aligned_size<void>(dataBlockByteSize, maxAlign));

	const std::size_t totalBlockSize(controlBlockSize + bufferSize + dataBlockSize + maxAlign);

	return totalBlockSize;
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
	typedef typename concurrent_queue<std::uint8_t>::size_type size_type;

	consumer_wrapper(SharedPtrSlotType buffer)
		: myBuffer(std::move(buffer))
		, myPopCounter(0)
	{
	}

	SharedPtrSlotType myBuffer;
	SharedPtrArrayType myLastKnownArray;
	std::uint16_t myPopCounter;
};
template <class T, class Allocator>
class shared_ptr_allocator_adaptor : public Allocator
{
public:
	shared_ptr_allocator_adaptor()
		: myAddress(nullptr)
		, mySize(0)
	{
	};
	shared_ptr_allocator_adaptor(T* retAddr, std::size_t size)
		: myAddress(retAddr)
		, mySize(size)
	{
	};

	T* allocate(std::size_t count)
	{
		if (!myAddress) {
			myAddress = this->Allocator::allocate(count);
			mySize = count;
		}
		return myAddress;
	};
	void deallocate(T* /*addr*/, std::size_t /*count*/)
	{
		T* const addr(myAddress);
		std::size_t size(mySize);

		myAddress = nullptr;
		mySize = 0;

		Allocator::deallocate(addr, size);
	}

private:
	T* myAddress;
	std::size_t mySize;
};
template <class T, class Allocator>
struct accessor_cache
{
	producer_buffer<T, Allocator>* myBuffer;

	union
	{
		void* myAddr;
		std::uint64_t myAddrBlock;
		struct
		{
			std::uint16_t padding[3];
			std::uint16_t myCounter;
		};
	};
};
template <class T, class Allocator>
class dummy_container
{
public:
	dummy_container();
	typedef typename concurrent_queue<T, Allocator>::shared_ptr_slot_type  dummy_type;
	typedef typename concurrent_queue<T, Allocator>::allocator_adapter_type allocator_adapter_type;
	typedef typename concurrent_queue<T, Allocator>::buffer_type buffer_type;
	typedef typename concurrent_queue<T, Allocator>::allocator_type allocator_type;


	dummy_type myDummyBuffer;
	item_container<T> myDummyItem;
	producer_buffer<T, allocator_type> myDummyRawBuffer;
};
template <class IndexType, class Allocator>
class index_pool
{
public:
	index_pool();
	~index_pool();

	IndexType get();
	void add(IndexType index);

	struct node
	{
		node(IndexType index, shared_ptr<node, Allocator> next)
			: myIndex(index)
			, myNext(std::move(next))
		{
		}
		IndexType myIndex;
		atomic_shared_ptr<node, Allocator> myNext;
	};
	atomic_shared_ptr<node, Allocator> myTop;

private:
	std::atomic<IndexType> myIterator;
};
template <class T, class Allocator>
class buffer_deleter
{
public:
	void operator()(T* obj, Allocator& /*alloc*/);
};
template<class T, class Allocator>
inline void buffer_deleter<T, Allocator>::operator()(T * obj, Allocator &)
{
	(*obj).~T();
}
template <class T, class Allocator>
class store_array_deleter
{
public:
	store_array_deleter(std::uint16_t capacity);
	store_array_deleter(store_array_deleter<T, Allocator>&& other) noexcept;
	void operator()(T* obj, Allocator& alloc);

private:
	std::uint16_t myCapacity;
};
template<class T, class Allocator>
inline store_array_deleter<T, Allocator>::store_array_deleter(std::uint16_t capacity)
	: myCapacity(capacity)
{
}
template<class T, class Allocator>
inline store_array_deleter<T, Allocator>::store_array_deleter(store_array_deleter<T, Allocator>&& other) noexcept
	: myCapacity(other.myCapacity)
{
}
template<class T, class Allocator>
inline void store_array_deleter<T, Allocator>::operator()(T * obj, Allocator & alloc)
{
	for (std::uint16_t i = 0; i < myCapacity; ++i) {
		obj[i].~T();
	}
	alloc.deallocate(reinterpret_cast<std::uint8_t*>(obj), myCapacity * sizeof(T));
}
template<class IndexType, class Allocator>
inline index_pool<IndexType, Allocator>::index_pool()
	: myTop(nullptr)
	, myIterator(0)
{
}
template<class IndexType, class Allocator>
inline index_pool<IndexType, Allocator>::~index_pool()
{
	shared_ptr<node, Allocator> top(myTop.unsafe_load());
	while (top) {
		shared_ptr<node, Allocator> next(top->myNext.unsafe_load());
		myTop.unsafe_store(next);
		top = std::move(next);
	}
}
template<class IndexType, class Allocator>
inline IndexType index_pool<IndexType, Allocator>::get()
{
	shared_ptr<node, Allocator> top(myTop.load());
	while (top) {
		raw_ptr<node, Allocator> expected(top);
		if (myTop.compare_exchange_strong(expected, top->myNext.load())) {
			return top->myIndex;
		}
	}

	return myIterator++;
}
template<class IndexType, class Allocator>
inline void index_pool<IndexType, Allocator>::add(IndexType index)
{
	shared_ptr<node, Allocator> entry(make_shared<node, Allocator>(index, nullptr));

	raw_ptr<node, Allocator> expected;
	do {
		shared_ptr<node, Allocator> top(myTop.load());
		expected = top.get_raw_ptr();
		entry->myNext = std::move(top);
	} while (!myTop.compare_exchange_strong(expected, std::move(entry)));
}
template<class T, class Allocator>
inline dummy_container<T, Allocator>::dummy_container()
	: myDummyItem(item_state::Dummy)
	, myDummyRawBuffer(1, &myDummyItem)
{
	myDummyBuffer = dummy_type{ &myDummyRawBuffer, [](buffer_type*, allocator_adapter_type&) {} };
}
}
template <class T, class Allocator>
cqdetail::index_pool<typename concurrent_queue<T, Allocator>::size_type, typename concurrent_queue<T, Allocator>::allocator_type> concurrent_queue<T, Allocator>::ourIndexPool;
template <class T, class Allocator>
thread_local std::vector<typename concurrent_queue<T, Allocator>::shared_ptr_slot_type, typename concurrent_queue<T, Allocator>::producer_vector_allocator> concurrent_queue<T, Allocator>::ourProducers(0);
template <class T, class Allocator>
thread_local std::vector<cqdetail::consumer_wrapper<typename concurrent_queue<T, Allocator>::shared_ptr_slot_type, typename concurrent_queue<T, Allocator>::shared_ptr_array_type>, typename concurrent_queue<T, Allocator>::consumer_vector_allocator> concurrent_queue<T, Allocator>::ourConsumers;
template <class T, class Allocator>
cqdetail::dummy_container<T, typename concurrent_queue<T, Allocator>::allocator_type> concurrent_queue<T, Allocator>::ourDummyContainer;
template <class T, class Allocator>
thread_local cqdetail::accessor_cache<T, typename concurrent_queue<T, Allocator>::allocator_type> concurrent_queue<T, Allocator>::ourLastConsumerCache{ &concurrent_queue<T, Allocator>::ourDummyContainer.myDummyRawBuffer, nullptr };
template <class T, class Allocator>
thread_local cqdetail::accessor_cache<T, typename concurrent_queue<T, Allocator>::allocator_type> concurrent_queue<T, Allocator>::ourLastProducerCache{ &concurrent_queue<T, Allocator>::ourDummyContainer.myDummyRawBuffer, nullptr };
}
#pragma warning(pop)
