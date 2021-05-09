// Copyright(c) 2021 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <gdul/utility/type_traits.h>
#include <gdul/math/math.h>
#include <xpolymorphic_allocator.h>

#include <atomic>
#include <memory>
#include <cassert>

namespace gdul {
/// <summary>
/// Small local memory pool
/// </summary>
/// <typeparam name="Bytes">Bytes of storage</typeparam>
/// <typeparam name="DefaultAlign">Default alignment of internal storage</typeparam>
/// <typeparam name="Allocator">Parent allocator for allocations in excess of local storage</typeparam>
template <std::size_t Bytes, class Allocator = std::allocator<std::uint8_t>>
class scratch_pad : public std::pmr::memory_resource
{
public:
	using least_size_type = least_unsigned_integer_t<Bytes>;
	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint8_t>;

	scratch_pad();
	scratch_pad(Allocator alloc);
	~scratch_pad() noexcept;

	std::size_t unused_storage() const noexcept;

	bool owns(const void* p) const;

	void reset();

	allocator_type get_parent_allocator() const noexcept;

private:
	bool do_is_equal(const memory_resource& that) const noexcept override final;

	void* do_allocate(std::size_t count, std::size_t align = 1) override final;
	void do_deallocate(void* p, std::size_t count, std::size_t align) override final;

	struct sizes
	{
		least_size_type at;
		least_size_type used;
	};

	std::uint8_t alignas(alignof(std::max_align_t)) m_bytes[Bytes];

	std::atomic<sizes> m_sizes;

	allocator_type m_allocator;
};
template<std::size_t Bytes, class Allocator>
inline scratch_pad<Bytes, Allocator>::scratch_pad()
	: scratch_pad(Allocator())
{
}
template<std::size_t Bytes, class Allocator>
inline scratch_pad<Bytes, Allocator>::scratch_pad(Allocator alloc)
	: m_bytes{}
	, m_sizes({ 0, 0 })
	, m_allocator(alloc)
{
}
template<std::size_t Bytes, class Allocator>
inline scratch_pad<Bytes, Allocator>::~scratch_pad() noexcept
{
	assert(m_sizes.load(std::memory_order_acquire).at == 0 && "scratch_pad destroyed with items in use");
}
template<std::size_t Bytes, class Allocator>
inline bool scratch_pad<Bytes, Allocator>::do_is_equal(const memory_resource& that) const noexcept
{
	return this == &that;
}
template<std::size_t Bytes, class Allocator>
inline void* scratch_pad<Bytes, Allocator>::do_allocate(std::size_t count, std::size_t align)
{
	sizes expected(m_sizes.load(std::memory_order_acquire));
	sizes desired;
	std::size_t alignedLocalBegin(0);

	do {
		const std::size_t localBegin(expected.at);
		const void* const addr(&m_bytes[localBegin]);
		const std::uintptr_t addrValue(reinterpret_cast<std::size_t>(addr));
		const std::uintptr_t addrAligned(align_value(addrValue, align));

		const std::uintptr_t offset(addrAligned - addrValue);
		alignedLocalBegin = localBegin + offset;
		const std::size_t avaliable(Bytes - alignedLocalBegin);

		if (avaliable < count) {
			return m_allocator.allocate(count);
		}

		if (Bytes < count) {
			assert(false);
		}

		const std::size_t alignedEnd = alignedLocalBegin + count;

		desired.at = static_cast<least_size_type>(alignedEnd);
		desired.used = expected.used + static_cast<least_size_type>(count);;

	} while (!m_sizes.compare_exchange_weak(expected, desired, std::memory_order_release, std::memory_order_relaxed));

	return &m_bytes[alignedLocalBegin];
}
template<std::size_t Bytes, class Allocator>
inline void scratch_pad<Bytes, Allocator>::do_deallocate(void* p, std::size_t count, std::size_t /*align*/)
{
	if (owns(p)) {
		sizes expected(m_sizes.load(std::memory_order_acquire));
		sizes desired;
		do {
			desired = expected;
			desired.used -= static_cast<least_size_type>(count);

			if (desired.used == 0) {
				desired.at = 0;
			}

		} while (!m_sizes.compare_exchange_weak(expected, desired, std::memory_order_release, std::memory_order_relaxed));
	}
	else {
		m_allocator.deallocate(static_cast<std::uint8_t*>(p), count);
	}
}
template<std::size_t Bytes, class Allocator>
inline std::size_t scratch_pad<Bytes, Allocator>::unused_storage() const noexcept
{
	return Bytes - m_sizes.load(std::memory_order_acquire).at;
}
template<std::size_t Bytes, class Allocator>
inline bool scratch_pad<Bytes, Allocator>::owns(const void* p) const
{
	return !(p < &m_bytes[0]) && p < &m_bytes[Bytes];
}
template<std::size_t Bytes, class Allocator>
inline void scratch_pad<Bytes, Allocator>::reset()
{
	m_sizes.store({ 0, 0 }, std::memory_order_release);
}
template<std::size_t Bytes, class Allocator>
inline typename scratch_pad<Bytes, Allocator>::allocator_type scratch_pad<Bytes, Allocator>::get_parent_allocator() const noexcept
{
	return m_allocator;
}
}
