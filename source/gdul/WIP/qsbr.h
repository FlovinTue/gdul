#pragma once

#include <cstddef>
#include <cstdint>

namespace gdul::qsb {

constexpr std::uint8_t MaxThreads = 64;

class qs_item;

namespace qsb_detail {
bool update_item(qs_item&);
}

class qs_item
{
public:
	qs_item();
	
private:
	friend bool qsb_detail::update_item(qs_item&);

	std::size_t m_mask;
};

class critical_section
{
public:
	critical_section();
	~critical_section();
};

void register_thread();
void unregister_thread();

bool update_item(qs_item& item);

}