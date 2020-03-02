# gdul
A collection of (mainly concurrency related) data structures, created with game programming in mind


-------------------------------------------------------------------------------------------------------------------------------------------

## atomic_shared_ptr
My take on an atomic shared pointer.

* Lock-Free (if used with a lock-free allocator)
* Uses an interface resembling that of an std::atomic type
* Uses internal versioning to make it resistant to ABA problems
* Uses a shared_ptr similar to that of std::shared_ptr
* Single header include atomic_shared_ptr.h, optionally atomic_shared_ptr.natvis for better debug viewing in Visual Studio

There are eight different versions of compare_exchange_strong, four of which take raw_ptr as expected value. These are non-reference counted representations of shared_ptr/atomic_shared_ptr. raw_ptr may be used if shared ownership is not needed of the expected out value, as the shared_ptr variants incur extra costs upon failure.
 
-------------------------------------------------------------------------------------------------------------------------------------------

## atomic_128
utility wrapper class for 128 bit atomic operations. 

-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_queue
Multi producer multi consumer unbounded lock-free queue. FIFO is respected within the context of single producers. Basic exception safety may be enabled at the price of a slight performance decrease.

Depends on atomic_shared_ptr.h, thread_local_member.h

-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_object_pool
Allocates chunks of objects and makes them avaliable for usage via get_object. Return objects using recycle_object. Concurrency safe & lock-free.

Depends on concurrent_queue.h, atomic_shared_ptr.h, thread_local_member.h

-------------------------------------------------------------------------------------------------------------------------------------------

## thread_local_member
Abstraction to enable members to be thread local. Internally, fast path contains only 1 integer comparison before returning object reference. Fast path is potentially invalidated when the accessed object is not-before seen by the accessing thread (Frequently
recreating and destroying tlm objects may yield poor performance).

If the number of tlm instances of one type does not exceed gdul::tlm_detail::Static_Alloc_Size, objects will be located in the corresponding thread's thread_local storage block and thus contain one level of indirection(tlm->thread_local) else it will be mapped to an external array and contain two levels of indirection(tlm->thread_local->external array). 
 
For sanity's sake, use alias tlm<T> instead of thread_local_member<T>

New operators may be added to the interface using the get() accessors.

Depends on atomic_shared_ptr.h

-------------------------------------------------------------------------------------------------------------------------------------------
## delegate
A simple delegate class

Supports (partial or full) binding of arguments in its constructor. The amount of of local storage (used to avoid allocations) may be changed by altering the Delegate_Storage variable

-------------------------------------------------------------------------------------------------------------------------------------------
## job_handler
####  -- Still fairly new, and may not be the most stable --
A job system

Main features would be:
* Supports (multiple) job dependencies. (if job 'first' depends on job 'second' then 'first' will not be enqueued for consumption until 'second' has completed) 
* Keeps multiple internal job queues (defined in gdul::jh_detail::Num_Job_Queues), with workers consuming from the further-back queues less frequently
* Has three types of batch_job (splits an array of items combined with a processing delegate over multiple jobs). 
* Job spawn graph may be dumped to file for viewing

Job tracking instructions: 
- make sure GDUL_JOB_DEBUG is defined in globals.h
- for each job taking part in the tracking, call activate_job_tracking(name)
- if making a msvc debug build, override /ZI (edit-and-continue) with /Zi to make sure __ LINE__ macro is seen as a compile time constant
- dump job graph using job_tracker::dump_job_tree(location)

A quick usage example for job:
```
#include <iostream>
#include <thread>
#include <gdul/job_handler/job_handler.h>

int main()
{	
	gdul::job_handler jh;
	jh.init();

	for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
		gdul::worker wrk(jh.make_worker());
		wrk.enable();
	}

	gdul::job jbA(jh.make_job([]() {std::cout << "Ran A" << std::endl; }));
	gdul::job jbB(jh.make_job([]() {std::cout << "...then B" << std::endl; }));
	jbB.add_dependency(jbA);
	jbB.enable();
	jbA.enable();

	jbB.wait_until_finished();

	jh.retire_workers();
}
```
And with batch_job:
```
#include <thread>
#include <gdul/job_handler/job_handler.h>
#include <vector>

int main()
{
	gdul::job_handler jh;
	jh.init();

	for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
	{
		gdul::worker wrk(jh.make_worker());
		wrk.enable();
	}

	std::vector<std::size_t> inputs;
	std::vector<std::size_t> outputs;

	for (std::size_t i = 0; i < 500; ++i)
	{
		inputs.push_back(i);
	}

	outputs.resize(inputs.size());

	gdul::delegate<bool(std::size_t&, std::size_t&)> process([](std::size_t& inputItem, std::size_t& outputItem)
	{
		// Output only the items that mod 5 == 0
		if (inputItem % 5 == 0)
		{
			outputItem = inputItem;
			return true;
		}
		return false;
	});

	gdul::batch_job bjb(jh.make_batch_job(inputs, outputs, process, 30/*batch size*/));

	bjb.enable();
	bjb.wait_until_finished();

	outputs.resize(bjb.get_output_size());

	// Here outputs should contain 0, 5, 10, 15...

	jh.retire_workers();
}
```
