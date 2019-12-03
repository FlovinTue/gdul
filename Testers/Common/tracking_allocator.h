#pragma once

#include <atomic>
#include <iostream>

namespace gdul{
	
static std::atomic_flag s_spin{ ATOMIC_FLAG_INIT };
static std::atomic_bool doPrint(true);
static std::atomic<int64_t> s_allocated(0);
template <class T>
class tracking_allocator : public std::allocator<T>
{
public:
	tracking_allocator() = default;

	template <typename U>
	struct rebind
	{
		using other = tracking_allocator<U>;
	};
	tracking_allocator(const tracking_allocator<T>&) = default;
	tracking_allocator& operator=(const tracking_allocator<T>&) = default;

	T* allocate(std::size_t count)
	{
		T* ret = std::allocator<T>::allocate(count);
		s_allocated += count * sizeof(T);
		if (doPrint) {
			spin();
			std::cout << "allocated " << count * sizeof(T) << "--------------" << " new value: " << s_allocated << std::endl;
			release();
		}
		return ret;
	}
	void deallocate(T* obj, std::size_t count)
	{
		std::allocator<T>::deallocate(obj, count);
		s_allocated -= count * sizeof(T);
		if (doPrint) {
			spin();
			std::cout << "deallocated " << count * sizeof(T) << "--------------" << " new value: " << s_allocated << std::endl;
			release();
		}
	}
	void spin()
	{
		while (s_spin.test_and_set()) {
			std::this_thread::sleep_for(std::chrono::microseconds(10));
		}
	}
	void release()
	{
		s_spin.clear();
	}
	template <class Other>
	tracking_allocator(const tracking_allocator<Other>&) {};
	template <class Other>
	const tracking_allocator<Other>& operator=(const tracking_allocator<Other>&) {};
};

}