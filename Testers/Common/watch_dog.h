#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <iostream>
#include <mutex>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

	void give_instruction(std::uint32_t sleepFor, std::function<void()> whenWoken = []() {__debugbreak(); });
	void pet();


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
};
template<class T>
inline watch_dog_impl<T>::watch_dog_impl()
	: m_lastPet(0)
	, m_alive(false)
	, m_threshHold(0)
{
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
	std::unique_lock lock(m_mtx);

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
			std::unique_lock lock(m_mtx);

			if ((get_ms() - m_lastPet) < m_threshHold) {
				continueSleep = true;
			}
			else {
				std::cout << "WOOF, WOOF" << std::endl;
				MessageBeep(MB_ICONERROR);
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