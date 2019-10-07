#include "stdafx.h"
#include <iostream>
#include "Timer.h"

#define SECONDS std::chrono::duration<float>

Timer::Timer() :
myDeltaTime(0.f)
{
	myFirstTimePoint = std::chrono::high_resolution_clock::now();
	myLastTimePoint = std::chrono::high_resolution_clock::now();
}
void Timer::Update()
{
	std::chrono::high_resolution_clock::time_point current = std::chrono::high_resolution_clock::now();

	myDeltaTime = std::chrono::duration_cast<SECONDS>(current - myLastTimePoint).count();

	myLastTimePoint = current;
}
float Timer::GetDeltaTime() const
{
	return myDeltaTime;
}
float Timer::GetTotalTime() const
{
	return std::chrono::duration_cast<SECONDS>(std::chrono::high_resolution_clock::now() - myFirstTimePoint).count();
}
void Timer::Reset()
{
	myFirstTimePoint = std::chrono::high_resolution_clock::now();
}