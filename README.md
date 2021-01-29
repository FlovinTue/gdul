# gdul
A collection of (mainly concurrency related) data structures, created with game programming in mind


-------------------------------------------------------------------------------------------------------------------------------------------

## atomic_shared_ptr

* Lock-free
* Uses an interface resembling that of an std::atomic type
* Uses internal versioning to make it resistant to ABA problems
* Uses a shared_ptr similar to that of std::shared_ptr
* Single header include atomic_shared_ptr.h, optionally atomic_shared_ptr.natvis for better debug viewing in Visual Studio

There are eight different versions of compare_exchange_strong, four of which take raw_ptr as expected value. These are non-reference counted representations of shared_ptr/atomic_shared_ptr. raw_ptr may be used if shared ownership is not needed of the expected out value, as the shared_ptr variants incur extra costs upon failure.
 
-------------------------------------------------------------------------------------------------------------------------------------------

## atomic_128
utility wrapper class for 128 bit atomic operations. 
```
#include <gdul/memory/atomic_128.h>

struct custom_struct
{
	std::uint64_t data[2];
};

int main()
{
	gdul::atomic_128<custom_struct> custom;
	const custom_struct in{ 5, 10 };

	custom.store(in);

	const custom_struct out(custom.load());

	custom_struct badExpected{ 5, 9 };
	custom_struct goodExpected{ 5, 10 };
	const custom_struct desired{ 15, 25 };

	const bool resultA(custom.compare_exchange_strong(badExpected, desired));
	const bool resultB(custom.compare_exchange_strong(goodExpected, desired));

	gdul::atomic_128<gdul::u128> integer;

	const gdul::u128 addTo64(integer.fetch_add_u64(1, 0));
	const gdul::u128 exchange8(integer.exchange_u8(17, 4));
	const gdul::u128 subTo32(integer.fetch_sub_u32(15, 1));

	return 0;
}
```
-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_queue
Multi producer multi consumer unbounded lock-free queue. FIFO is respected within the context of single producers. Basic exception safety may be enabled at the price of a slight performance decrease.

-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_priority_queue
#### --new--
Concurrency safe, lock free priority queue based around skip list design. Provides 3 different methods of node allocation:
* Pool based. Fast, without need for external application support -Just works
* Scratch based. Faster, but needs periodic resetting of internal scratch pool
* External. Fastest (potentially), node allocation and reclamation is handled externally by the user

Ex. Usage
```
#include <gdul/containers/concurrent_priority_queue.h>

int main()
{
	gdul::concurrent_priority_queue<int, int> defaultPool;
	defaultPool.push(std::make_pair(0, 0));

	gdul::concurrent_priority_queue<int, int, 512, gdul::cpq_allocation_strategy_pool<std::allocator<std::uint8_t>>> poolBased;
	poolBased.push(std::make_pair(1, 1));

	gdul::concurrent_priority_queue<int, int, 512, gdul::cpq_allocation_strategy_scratch<std::allocator<std::uint8_t>>> scratchBased;
	scratchBased.push(std::make_pair(2, 2));
	scratchBased.unsafe_reset_scratch_pool();

	gdul::concurrent_priority_queue<int, int, 512, gdul::cpq_allocation_strategy_external> external;

	decltype(external)::node_type n;
	n.m_kv = std::make_pair(3, 3);

	external.push(&n);
}
```

-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_object_pool
Concurrency safe, lock free object pool built around concurrent_queue

-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_scratch_pool
#### --new--
Concurrency safe, lock free scratch pool. Usage like an object pool but requires periodic calls to unsafe_reset

-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_guard_pool
#### --new--
Concurrency safe, lock free object pool with an additional guard method for use in shared-data scenarios where object reclamation only may
happen when all references are dead. 

Ex usage.
```
#include <gdul/memory/concurrent_guard_pool.h>

struct item{};

struct some_data_structure
{

	bool fetch_item(item& out)
	{
		return m_pool.guard(&some_data_structure::fetch_item_internal, this, out);
	}

private:
	struct node
	{
		item m_item;
	};

	bool fetch_item_internal(item& out)
	{
		// Critical section, reference shared nodes
		// ...
	}

	gdul::concurrent_guard_pool<node> m_pool;
};

int main()
{
	some_data_structure ds;
	
	item i;

	if (ds.fetch_item(i)) {
		// ...
	}
}
```

-------------------------------------------------------------------------------------------------------------------------------------------

## thread_local_member
Abstraction to enable members to be thread local. Internally, fast path contains only 1 integer comparison before returning object reference. Fast path is potentially invalidated when the accessed object is not-before seen by the accessing thread (Frequently
recreating and destroying tlm objects may yield poor performance).

If the number of tlm instances of one type does not exceed gdul::tlm_detail::Static_Alloc_Size, objects will be located in the corresponding thread's thread_local storage block and thus contain one level of indirection(tlm->thread_local) else it will be mapped to an external array and contain two levels of indirection(tlm->thread_local->external array). 
 
For sanity's sake, use alias tlm<T> instead of thread_local_member<T>

New operators may be added to the interface using the get() accessors.

-------------------------------------------------------------------------------------------------------------------------------------------
## delegate
A simple delegate class

Supports (partial or full) binding of arguments in its constructor. The amount of of local storage (used to avoid allocations) may be changed by altering the Delegate_Storage variable

-------------------------------------------------------------------------------------------------------------------------------------------
## job_handler

* Features a built in mechanism to take advantage of the finite nature of frame-bound jobs, continusly promoting parallelism.
* Supports (multiple) job dependencies. (if job 'first' depends on job 'second' then 'first' will not be enqueued for consumption until 'second' has completed) 
* Workers are flexibly assigned to user-declared job queues
* Has three types of batch_job (splits an array of items combined with a processing delegate over multiple jobs). 
* Job relationship graph may be dumped to file for viewing
* Job profiling info may be dumped for viewing

Job tracking instructions: 
- make sure GDUL_JOB_DEBUG is defined in globals.h

- dump job graph using job_handler::dump_job_graph(location)
- dump job time sets using job_handler::dump_job_time_sets(location) 

- graph may be viewed using the Visual Studio dgml extension. 
- job time sets may be viewed in the small C# app job_time_set_view located in the source folder

To take advantage of predictive scheduling:

Users may declare job queues of two types: job_async_queue and job_sync_queue. To allow for job reordering, simply post jobs to an instance of job_sync_queue. Do note that this should not be used for asynchronous jobs like file loading etc, as this may cause undesired reordering. 
This will help promote parallelism in scenarios where complex dependency graps exist with multiple jobs waiting for execution.

A quick usage example for job:
```
#include <iostream>
#include <thread>
#include <gdul/execution/job_handler_master.h>

int main()
{
	gdul::job_handler jh;
	gdul::job_async_queue q;

	jh.init();

	for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
		gdul::worker wrk(jh.make_worker());
		wrk.add_assignment(&q);
		wrk.enable();
	}

	gdul::job jbA(jh.make_job([]() {std::cout << "Ran A" << std::endl; }, &q));
	gdul::job jbB(jh.make_job([]() {std::cout << "...then B" << std::endl; }, &q));
	jbB.depends_on(jbA);
	jbB.enable();
	jbA.enable();

	jbB.wait_until_finished();

	jh.shutdown();
}
```
And with batch_job:
```
#include <thread>
#include <gdul/execution/job_handler_master.h>
#include <vector>

int main()
{
	gdul::job_handler jh;
	gdul::job_async_queue q;

	jh.init();

	for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
		gdul::worker wrk(jh.make_worker());
		wrk.add_assignment(&q);
		wrk.enable();
	}

	std::vector<std::size_t> inputs;
	std::vector<std::size_t> outputs;

	for (std::size_t i = 0; i < 500; ++i) {
		inputs.push_back(i);
	}

	gdul::delegate<bool(std::size_t&, std::size_t&)> process([](std::size_t& inputItem, std::size_t& outputItem) {
		// Output only the items that mod 5 == 0
		if (inputItem % 5 == 0) {
			outputItem = inputItem;
			return true;
		}
		return false;
		});

	gdul::batch_job bjb(jh.make_batch_job(inputs, outputs, process, &q));

	bjb.enable();
	bjb.wait_until_finished();

	// Here outputs should contain 0, 5, 10, 15...

	jh.shutdown();
}
```
