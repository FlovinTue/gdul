

#include "../Common/thread_pool.h"
#include "../Common/tracking_allocator.h"

class Tester
{
	public:

	void execute();

	gdul::thread_pool m_worker;

	gdul::tracking_allocator<uint8_t> m_alloc;

	using alloc_t = decltype(m_alloc);
};
