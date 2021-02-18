#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

namespace gdul::qsbr {

constexpr std::uint8_t MaxThreads = 64;

class item;

namespace qsbr_detail {
bool initialize_item(item&);
bool update_item(item&);
bool check_item(const item&);
}

class item
{
public:
	item();
		
private:
	friend bool qsbr_detail::initialize_item(item&);
	friend bool qsbr_detail::update_item(item&);
	friend bool qsbr_detail::check_item(const item&);

	std::atomic<std::size_t> m_mask;
};

class critical_section
{
public:
	critical_section();
	~critical_section();
};

void register_thread();
void unregister_thread();

bool initialize_item(item& item);
bool update_item(item& item);
bool check_item(const item& item);

}