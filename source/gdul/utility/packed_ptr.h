//Copyright(c) 2021 Flovin Michaelsen
//
//Permission is hereby granted, free of charge, to any person obtining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>


namespace gdul {

namespace pp_detail {

constexpr std::uint8_t max_bottom_bits()
{
	std::uint8_t i = 0, align(alignof(std::max_align_t));
	for (; align; ++i, align >>= 1);
	return i - 1;
}

constexpr std::uintptr_t Lower_Mask = alignof(std::max_align_t) - 1;
constexpr std::uintptr_t Upper_Mask = ~(std::numeric_limits<std::uintptr_t>::max() >> 16);
constexpr std::uintptr_t Ptr_Mask = ~Upper_Mask & ~Lower_Mask;

class packed_ptr_base
{
public:
	static constexpr std::uint8_t Max_Extra_Bits = 16 + max_bottom_bits();
};
}

/// <summary>
/// Packed ptr wrapper to store value in Max_Extra_Bits (16 + whatever bottom bits are avaliable). Assumes pointer at least std::max_align_t alignment
/// </summary>
/// <typeparam name="P">Pointer type</typeparam>
/// <typeparam name="ExtraBits">Extra value type, enum int etc</typeparam>
template <class P, class ExtraBits = std::uint32_t>
class packed_ptr : public pp_detail::packed_ptr_base
{
public:
	using pointer = P*;
	using extra_type = ExtraBits;

	packed_ptr() noexcept;
	packed_ptr(P* p) noexcept;
	packed_ptr(P* p, ExtraBits extraValue) noexcept;

	P* ptr() noexcept;
	const P* ptr() const noexcept;

	ExtraBits extra() const noexcept;

	void set_extra(ExtraBits value) noexcept;
	void set_ptr(P* p) noexcept;

	P* operator->() noexcept;
	const P* operator->() const noexcept;

	P& operator*();
	const P& operator*() const;

private:
	std::uintptr_t m_storage;
};
template<class P, class ExtraBits>
inline packed_ptr<P, ExtraBits>::packed_ptr() noexcept
	: m_storage(0)
{
}
template<class P, class ExtraBits>
inline packed_ptr<P, ExtraBits>::packed_ptr(P* p) noexcept
	: packed_ptr()
{
	set_ptr(p);
}
template<class P, class ExtraBits>
inline packed_ptr<P, ExtraBits>::packed_ptr(P* p, ExtraBits extraValue) noexcept
	: packed_ptr(p)
{
	set_extra(extraValue);
}
template<class P, class ExtraBits>
inline P* packed_ptr<P, ExtraBits>::ptr() noexcept
{
	return (P*)(m_storage & pp_detail::Ptr_Mask);
}
template<class P, class ExtraBits>
inline const P* packed_ptr<P, ExtraBits>::ptr() const noexcept
{
	return (const P*)(m_storage & pp_detail::Ptr_Mask);
}
template<class P, class ExtraBits>
inline ExtraBits packed_ptr<P, ExtraBits>::extra() const noexcept
{
	const std::uintptr_t lower(m_storage & pp_detail::Lower_Mask);
	const std::uintptr_t upper(m_storage & pp_detail::Upper_Mask);
	const std::uintptr_t upperShift(upper >> (48 - pp_detail::max_bottom_bits()));

	return (ExtraBits)(lower | upperShift);
}
template<class P, class ExtraBits>
inline void packed_ptr<P, ExtraBits>::set_extra(ExtraBits value) noexcept
{
	const std::uintptr_t ivalue((std::uintptr_t)value);
	const std::uintptr_t lower(ivalue & pp_detail::Lower_Mask);
	const std::uintptr_t upper(ivalue << (48 - pp_detail::max_bottom_bits()));
	const std::uintptr_t upperMasked(upper & pp_detail::Upper_Mask);
	const std::uintptr_t ptr(m_storage & pp_detail::Ptr_Mask);

	m_storage = (lower | ptr | upperMasked);
}
template<class P, class ExtraBits>
inline void packed_ptr<P, ExtraBits>::set_ptr(P* p) noexcept
{
	const std::uintptr_t iptr((std::uintptr_t)p);
	const std::uintptr_t existing(m_storage);
	const std::uintptr_t existingCleaned(existing & ~pp_detail::Ptr_Mask);

	m_storage = existingCleaned | iptr;
}
template<class P, class ExtraBits>
inline P* packed_ptr<P, ExtraBits>::operator->() noexcept
{
	return ptr();
}
template<class P, class ExtraBits>
inline const P* packed_ptr<P, ExtraBits>::operator->() const noexcept
{
	return ptr();
}
template<class P, class ExtraBits>
inline P& packed_ptr<P, ExtraBits>::operator*()
{
	return *ptr();
}
template<class P, class ExtraBits>
inline const P& packed_ptr<P, ExtraBits>::operator*() const
{
	return *ptr();
}
}