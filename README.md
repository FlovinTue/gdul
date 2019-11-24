# gdul
 A collection of threading related utilities 


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

Includes needed are concurrent_queue.h and atomic_shared_ptr.h. concurrent_queue.natvis and atomic_shared_ptr.natvis may be added to projects for additional debug information in Visual Studio

-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_object_pool
Allocates chunks of objects and makes them avaliable for usage via get_object. Return objects using recycle_object. Concurrency safe & lock-free.

Includes needed are concurrent_object_pool.h, concurrent_queue_.h

-------------------------------------------------------------------------------------------------------------------------------------------

## thread_local_member
####  -- Still fairly new, and may not be the most stable --

Abstraction to enable members to be thread local. Internally, fast path contains only 1 integer comparison before returning object reference. Fast path is potentially invalidated when the accessed object is not-before seen by the accessing thread (Frequently
recreating and destroying tlm objects may yield poor performance).

If the number of tlm instances of one type does not exceed gdul::tlm_detail::Static_Alloc_Size, objects will be located in the corresponding thread's thread_local storage block and thus contain one level of redirect(tlm->thread_local) else it will be mapped to an external array and contain two redirects(tlm->thread_local->array). 
 
For sanity's sake, use alias tlm<T> instead of thread_local_member<T>

New operators may be added to the interface using the implicit conversion operators as accessors.
