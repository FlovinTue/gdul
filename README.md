# gdul
 A collection of threading related utilities 


-------------------------------------------------------------------------------------------------------------------------------------------

## atomic_shared_ptr
My take on an atomic shared pointer.

* Lock-Free (if used with a lock-free allocator)
* Uses an interface resembling that of an std::atomic type
* Uses internal versioning to protect against ABA problems
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
