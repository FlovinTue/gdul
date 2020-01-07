#pragma once

#include <chrono>

namespace gdul
{
template <class T>
class timer
{
public:
	timer();

	T get() const;

	void reset();

private:
	mutable std::chrono::high_resolution_clock myClock;
	mutable std::chrono::high_resolution_clock::time_point myFromTime;
};
template<class T>
inline timer<T>::timer()
	: myFromTime(myClock.now())
{
}
template<class T>
inline T timer<T>::get() const
{
	return std::chrono::duration_cast<std::chrono::duration<T>>(myClock.now() - myFromTime).count();
}
template<class T>
inline void timer<T>::reset()
{
	myFromTime = myClock.now();
}
}