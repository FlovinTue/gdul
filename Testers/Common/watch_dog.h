#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <iostream>

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

	void start(std::uint32_t ms, std::function<void()> whenWoken = []() {__debugbreak(); });
	void stop();
	void pet();


private:
	void guard();
	std::size_t get_ms() const;

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
	stop();
}
template<class T>
inline void watch_dog_impl<T>::start(std::uint32_t ms, std::function<void()> whenWoken)
{
	if (m_thread.get_id() != std::thread().get_id()) {
		m_thread.join();
	}

	m_whenWoken = whenWoken;
	m_threshHold = ms;
	m_alive = true;

	pet();

	m_thread = std::thread(&watch_dog_impl<T>::guard, this);
}

template<class T>
inline void watch_dog_impl<T>::stop()
{
	m_alive = false;
	m_thread.join();
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
		if ((get_ms() - m_lastPet) < m_threshHold) {
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
		}
		else {
			if (IsDebuggerPresent()) {
				pet();
			}
			else {
				std::cout << "WOOF, WOOF" << std::endl;
				m_whenWoken();
			}
		}
	}
}



}