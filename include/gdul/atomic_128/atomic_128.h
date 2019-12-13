#pragma once

//Copyright(c) 2019 Flovin Michaelsen
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
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


#if  defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#include <intrin.h>
#endif
#include <cstdint>
#include <type_traits>
#include <assert.h>

#pragma warning(push)
#pragma warning(disable : 4324)

#define GDUL_ENABLE_IF_U128 template <class U = T, std::enable_if_t<std::is_same_v<U, gdul::u128>>* = nullptr>
#define GDUL_ENABLE_IF_U128_IMP template <class U, std::enable_if_t<std::is_same_v<U, gdul::u128>>*>

namespace gdul{

template <class T>
class atomic_128;

union alignas(16) u128
{
	constexpr u128() : m_u64{ 0 } {}
	constexpr u128(std::uint64_t low) : m_u64{ low ,  } {}
	constexpr u128(std::uint64_t low, std::uint64_t high) : m_u64{ low,high } {}

	inline constexpr bool operator==(const u128& other) const {
		return (m_u64[0] == other.m_u64[0]) & (m_u64[1] == other.m_u64[1]);
	}
	inline constexpr bool operator!=(const u128& other) const {
		return !operator==(other);
	}

	constexpr operator bool() const{
		return m_u64[0] | m_u64[1];
	}

	std::uint64_t m_u64[2];
	std::uint32_t m_u32[4];
	std::uint16_t m_u16[8];
	std::uint8_t m_u8[16];

private:
	friend class atomic_128<u128>;
	std::int64_t m_s64[2];
};

using atomic_u128 = atomic_128<u128>;

// Utility wrapper class for atomic operations on 128 bit types. 
// Supplies extra integer utility functionality if used with gdul::u128 as value type
template <class T>
class alignas(16) atomic_128
{
private:
	template <class T>
	struct disable_deduction
	{
		using type = T;
	};
public:
	inline constexpr atomic_128();
	inline constexpr atomic_128(const T& value) noexcept;

	inline bool compare_exchange_strong(T& expected, const T& desired) noexcept;

	inline T exchange(const T& desired) noexcept;

	inline void store(const T& desired) noexcept;
	inline T load() const noexcept;

	constexpr const T& my_val() const noexcept;
	constexpr T& my_val() noexcept;

	GDUL_ENABLE_IF_U128 inline u128 exchange_u64(std::uint64_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 exchange_u32(std::uint32_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 exchange_u16(std::uint16_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 exchange_u8(std::uint8_t value, std::uint8_t atIndex) noexcept;

	GDUL_ENABLE_IF_U128 inline u128 fetch_add_to_u64(std::uint64_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 fetch_add_to_u32(std::uint32_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 fetch_add_to_u16(std::uint16_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 fetch_add_to_u8(std::uint8_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 fetch_sub_to_u64(std::uint64_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 fetch_sub_to_u32(std::uint32_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 fetch_sub_to_u16(std::uint16_t value, std::uint8_t atIndex) noexcept;
	GDUL_ENABLE_IF_U128 inline u128 fetch_sub_to_u8(std::uint8_t value, std::uint8_t atIndex) noexcept;

private:
	template <class IntegerType>
	inline u128 fetch_add_to_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex) noexcept;
	template <class IntegerType>
	inline u128 fetch_sub_to_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex) noexcept;
	template <class IntegerType>
	inline u128 exchange_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex) noexcept;

	inline bool cas_internal(T* expected, const T* desired) const noexcept;

	mutable T m_storage;
};
template <class T>
template<class IntegerType>
inline u128 atomic_128<T>::fetch_add_to_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex) noexcept
{
	static_assert(std::is_integral<IntegerType>(), "Only integers allowed as value type");

	typedef typename std::remove_const<IntegerType>::type int_type_no_const;

	const std::uint8_t index(atIndex);
	const std::uint8_t scaledIndex(index * sizeof(IntegerType));

	assert((scaledIndex < 16) && "Index out of bounds");

	u128 expected(my_val());
	u128 desired;

	union
	{
		std::uint8_t* int8Target;
		int_type_no_const* target;
	};
	int8Target = &desired.m_u8[scaledIndex];

	do {
		desired = expected;
		*target += value;
	} while (!cas_internal(&expected, &desired));

	return expected;
}
template <class T>
template<class IntegerType>
inline u128 atomic_128<T>::fetch_sub_to_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex) noexcept
{
	static_assert(std::is_integral<IntegerType>(), "Only integers allowed as value type");

	typedef typename std::remove_const<IntegerType>::type int_type_no_const;

	const std::uint8_t index(atIndex);
	const std::uint8_t scaledIndex(index * sizeof(IntegerType));

	assert((scaledIndex < 16) && "Index out of bounds");

	u128 expected(my_val());
	u128 desired;
	union
	{
		std::uint8_t* int8Target;
		int_type_no_const* target;
	};
	int8Target = &desired.m_u8[scaledIndex];

	do {
		desired = expected;
		*target -= value;
	} while (!cas_internal(&expected, &desired));

	return expected;
}
template <class T>
template<class IntegerType>
inline u128 atomic_128<T>::exchange_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex) noexcept
{
	static_assert(std::is_integral<IntegerType>(), "Only integers allowed as value type");

	typedef typename std::remove_const<IntegerType>::type int_type_no_const;

	const std::uint8_t index(atIndex);
	const std::uint8_t scaledIndex(index * sizeof(IntegerType));

	assert((scaledIndex < 16) && "Index out of bounds");

	u128 expected(my_val());
	u128 desired;
	union
	{
		std::uint8_t* int8Target;
		int_type_no_const* target;
	};
	int8Target = &desired.m_u8[scaledIndex];

	do {
		desired = expected;
		*target = value;
	} while (!cas_internal(&expected, &desired));

	return expected;
}
template<class T>
inline constexpr atomic_128<T>::atomic_128()
	: atomic_128<T>(T())
{
}
template <class T>
inline constexpr atomic_128<T>::atomic_128(const T & value) noexcept
	: m_storage(value)
{
	static_assert(std::is_trivially_copyable_v<T>, "Value type must be trivially copyable");
	static_assert(sizeof(T) == 16, "Size of value type must be 16");
}
template <class T>
inline bool atomic_128<T>::compare_exchange_strong(T & expected, const T& desired) noexcept
{
	return cas_internal(&expected, &desired);
}
template <class T>
inline T atomic_128<T>::exchange(const T& desired) noexcept
{
	T expected(my_val());
	while (!compare_exchange_strong(expected, desired));
	return expected;
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::exchange_u64(std::uint64_t value, std::uint8_t atIndex) noexcept
{
	return exchange_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::exchange_u32(std::uint32_t value, std::uint8_t atIndex) noexcept
{
	return exchange_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::exchange_u16(std::uint16_t value, std::uint8_t atIndex) noexcept
{
	return exchange_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::exchange_u8(const std::uint8_t value, std::uint8_t atIndex) noexcept
{
	return exchange_integer<decltype(value)>(value, atIndex);
}
template <class T>
inline void atomic_128<T>::store(const T& desired) noexcept
{
	T expected(my_val());
	while (!compare_exchange_strong(expected, desired));
}
template <class T>
inline T atomic_128<T>::load() const noexcept
{
	T expectedDesired;
	cas_internal(&expectedDesired, &expectedDesired);
	return expectedDesired;
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::fetch_add_to_u64(std::uint64_t value, std::uint8_t atIndex) noexcept
{
	return fetch_add_to_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::fetch_add_to_u32(std::uint32_t value, std::uint8_t atIndex) noexcept
{
	return fetch_add_to_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::fetch_add_to_u16(std::uint16_t value, std::uint8_t atIndex) noexcept
{
	return fetch_add_to_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::fetch_add_to_u8(const std::uint8_t value, std::uint8_t atIndex) noexcept
{
	return fetch_add_to_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::fetch_sub_to_u64(std::uint64_t value, std::uint8_t atIndex) noexcept
{
	return fetch_sub_to_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::fetch_sub_to_u32(std::uint32_t value, std::uint8_t atIndex) noexcept
{
	return fetch_sub_to_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::fetch_sub_to_u16(std::uint16_t value, std::uint8_t atIndex) noexcept
{
	return fetch_sub_to_integer<decltype(value)>(value, atIndex);
}
template <class T>
GDUL_ENABLE_IF_U128_IMP inline u128 atomic_128<T>::fetch_sub_to_u8(const std::uint8_t value, std::uint8_t atIndex) noexcept
{
	return fetch_sub_to_integer<decltype(value)>(value, atIndex);
}
template <class T>
inline constexpr const T& atomic_128<T>::my_val() const noexcept
{
	return m_storage;
}
template <class T>
inline constexpr T & atomic_128<T>::my_val() noexcept
{
	return m_storage;
}
#if  defined(_MSC_VER) && !defined(__INTEL_COMPILER)
template <class T>
inline bool atomic_128<T>::cas_internal(T* const expected, const T* desired) const noexcept
{
	return _InterlockedCompareExchange128(reinterpret_cast<volatile std::int64_t*>(&m_storage), ((const std::int64_t*)desired)[1], ((const std::int64_t*)desired)[0], ((std::int64_t*)expected));
}
#elif defined(__GNUC__) || defined(__clang__)
template <class T>
inline bool atomic_128<T>::cas_internal(T* const expected, const T* desired) const noexcept
{
	bool result;
	__asm__ __volatile__
	(
		"lock cmpxchg16b %1\n\t"
		"setz %0"
		: "=q" (result)
		, "+m" (reinterpret_cast<volatile std::int64_t*>(&m_storage))
		, "+d" (((std::int64_t*)expected)[1])
		, "+a" (((std::int64_t*)expected)[0])
		: "c" (((const std::int64_t*)desired)[1])
		, "b" (((const std::int64_t*)desired)[0])
		: "cc", "memory"
	);
	return result;
}
#else
#error Unsupported platform
#endif
}

#pragma warning(pop)