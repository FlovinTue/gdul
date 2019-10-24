#pragma once

#include <chrono>

template <class T>
class timer
{
public:
	timer();

	T get() const;

	void reset();

private:
	std::chrono::high_resolution_clock myClock;
	std::chrono::high_resolution_clock::time_point myFromTime;
};
template<class T>
inline timer<T>::timer()
	: myFromTime(myClock.now())
{
}
template<class T>
inline T timer<T>::get() const
{
	std::chrono::duration_cast<std::chrono::duration<T>>(myClock.now() - myFromTime);
}
template<class T>
inline void timer<T>::reset()
{
	myFromTime = myClock.now();
}
