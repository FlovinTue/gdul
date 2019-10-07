#pragma once

#include <chrono>

class Timer
{
public:
	Timer();
	Timer(const Timer &aTimer) = delete;
	Timer& operator=(const Timer &aTimer) = delete;

	void Update();

	float GetDeltaTime() const;
	float GetTotalTime() const;

	void Reset();

private:
	std::chrono::high_resolution_clock::time_point myFirstTimePoint;
	std::chrono::high_resolution_clock::time_point myLastTimePoint;

	float myDeltaTime;
};
