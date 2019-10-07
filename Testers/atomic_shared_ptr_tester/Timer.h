#pragma once

#include <chrono>

class Timer
{
public:
	Timer();

	void Update();

	float GetDeltaTime() const;
	float GetTotalTime() const;

	void Reset();

private:
	std::chrono::high_resolution_clock::time_point myFirstTimePoint;
	std::chrono::high_resolution_clock::time_point myLastTimePoint;

	float myDeltaTime;
};
