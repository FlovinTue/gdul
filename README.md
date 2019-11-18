# gdul
 A collection of threading related utilities 
 
 
 
-------------------------------------------------------------------------------------------------------------------------------------------

## atomic_uint128
utilityclass for 128 bit atomic operations

-------------------------------------------------------------------------------------------------------------------------------------------

## concurrent_queue
Multi producer multi consumer unbounded lock-free queue. FIFO is respected within the context of single producers. Basic exception safety may be enabled at the price of a slight performance decrease.

Includes needed are concurrent_queue.h and atomic_shared_ptr.h. concurrent_queue.natvis and atomic_shared_ptr.natvis may be added to projects for additional debug information in Visual Studio
