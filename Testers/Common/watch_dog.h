#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <iostream>
#include <mutex>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "timer.h"
#include <string>

namespace gdul {
template <class T = void>
class watch_dog_impl;

using watch_dog = watch_dog_impl<>;

template <class T>
class watch_dog_impl
{
public:
	watch_dog_impl();
	~watch_dog_impl();

	void give_instruction(std::uint32_t sleepFor, std::function<void()> whenWoken = []() { if(IsDebuggerPresent()) __debugbreak(); });
	void pet();

	bool sleepy() const;

private:
	void guard();
	std::size_t get_ms() const;

	std::mutex m_mtx;
	std::atomic<std::size_t> m_lastPet;
	std::atomic_bool m_alive;
	std::uint32_t m_threshHold;
	std::thread m_thread;
	std::function<void()> m_whenWoken;
	std::chrono::high_resolution_clock m_clock;

	gdul::timer<float> m_totalTime;
};
template<class T>
inline watch_dog_impl<T>::watch_dog_impl()
	: m_lastPet(0)
	, m_alive(false)
	, m_threshHold(0)
{
	m_totalTime.reset();
}
template<class T>
inline watch_dog_impl<T>::~watch_dog_impl()
{
	m_alive = false;
	m_thread.join();
}
template<class T>
inline void watch_dog_impl<T>::give_instruction(std::uint32_t sleepFor, std::function<void()> whenWoken)
{
	std::unique_lock<std::mutex> lock(m_mtx);

	m_whenWoken = whenWoken;
	m_threshHold = sleepFor;
	m_alive = true;

	pet();

	if (m_thread.get_id() == std::thread().get_id()) {
		m_thread = std::thread(&watch_dog_impl<T>::guard, this);
	}
}
template<class T>
inline void watch_dog_impl<T>::pet()
{
	m_lastPet = get_ms();
}
template<class T>
inline bool watch_dog_impl<T>::sleepy() const
{
	return (get_ms() - m_lastPet) < m_threshHold;
}
template<class T>
inline std::size_t watch_dog_impl<T>::get_ms() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(m_clock.now().time_since_epoch()).count();
}

template<class T>
inline void watch_dog_impl<T>::guard()
{
	const std::size_t sleepMs(std::max<std::size_t>(1, (m_threshHold / 4)));

	while (m_alive) {
		bool continueSleep(0);

		{
			const std::unique_lock<std::mutex> cs(m_mtx);

			if (sleepy()) {
				continueSleep = true;
			}
			else {
				std::cout << "WOOF, WOOF" << std::endl;
				MessageBeep(MB_ICONERROR);

				std::wstring str(L"Watchdog triggered at " + std::to_wstring(m_totalTime.get()) + L" seconds");
				MessageBox(NULL, str.c_str(), L"Watchdog alert", MB_OK);
				m_whenWoken();
				pet();
			}
			
		}
		if (continueSleep) {
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
		}
	}
}



}