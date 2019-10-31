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

namespace gdul{

union uint128
{
	constexpr uint128() : myU64{ 0 } {}
	constexpr uint128(std::uint64_t low) : myU64{ low ,  } {}
	constexpr uint128(std::uint64_t low, std::uint64_t high) : myU64{ low,high } {}

	inline constexpr bool operator==(const uint128& other) const {
		return (myU64[0] == other.myU64[0]) & (myU64[1] == other.myU64[1]);
	}
	inline constexpr bool operator!=(const uint128& other) const {
		return !operator==(other);
	}

	constexpr operator bool() const{
		return myU64[0] | myU64[1];
	}

	std::uint64_t myU64[2];
	std::uint32_t myU32[4];
	std::uint16_t myU16[8];
	std::uint8_t myU8[16];

private:
	friend class atomic_uint128;
	std::int64_t myS64[2];
};

class alignas(16) atomic_uint128
{
private:
	template <class T>
	struct disable_deduction
	{
		using type = T;
	};
public:
	inline constexpr atomic_uint128() = default;
	inline constexpr atomic_uint128(const uint128& value);

	bool compare_exchange_strong(uint128& expected, const uint128& desired);

	uint128 exchange(const uint128& desired);
	uint128 exchange_u64(std::uint64_t value, std::uint8_t atIndex);
	uint128 exchange_u32(std::uint32_t value, std::uint8_t atIndex);
	uint128 exchange_u16(std::uint16_t value, std::uint8_t atIndex);
	uint128 exchange_u8(std::uint8_t value, std::uint8_t atIndex);

	void store(const uint128& desired);
	uint128 load();

	uint128 fetch_add_to_u64(std::uint64_t value, std::uint8_t atIndex);
	uint128 fetch_add_to_u32(std::uint32_t value, std::uint8_t atIndex);
	uint128 fetch_add_to_u16(std::uint16_t value, std::uint8_t atIndex);
	uint128 fetch_add_to_u8(std::uint8_t value, std::uint8_t atIndex);
	uint128 fetch_sub_to_u64(std::uint64_t value, std::uint8_t atIndex);
	uint128 fetch_sub_to_u32(std::uint32_t value, std::uint8_t atIndex);
	uint128 fetch_sub_to_u16(std::uint16_t value, std::uint8_t atIndex);
	uint128 fetch_sub_to_u8(std::uint8_t value, std::uint8_t atIndex);

	constexpr const uint128& my_val() const;
	constexpr uint128& my_val();

private:
	template <class IntegerType>
	uint128 fetch_add_to_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex);
	template <class IntegerType>
	uint128 fetch_sub_to_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex);
	template <class IntegerType>
	uint128 exchange_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex);

	bool cas_internal(std::int64_t* expected, const std::int64_t* desired);

	uint128 myStorage;
};

template<class IntegerType>
inline uint128 atomic_uint128::fetch_add_to_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex)
{
	static_assert(std::is_integral<IntegerType>(), "Only integers allowed as value type");

	typedef typename std::remove_const<IntegerType>::type int_type_no_const;

	const std::uint8_t index(atIndex);
	const std::uint8_t scaledIndex(index * sizeof(IntegerType));

	assert((scaledIndex < 16) && "Index out of bounds");

	uint128 expected(my_val());
	uint128 desired;

	union
	{
		std::uint8_t* int8Target;
		int_type_no_const* target;
	};
	int8Target = &desired.myU8[scaledIndex];

	do {
		desired = expected;
		*target += value;
	} while (!cas_internal(expected.myS64, desired.myS64));

	return expected;
}
template<class IntegerType>
inline uint128 atomic_uint128::fetch_sub_to_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex)
{
	static_assert(std::is_integral<IntegerType>(), "Only integers allowed as value type");

	typedef typename std::remove_const<IntegerType>::type int_type_no_const;

	const std::uint8_t index(atIndex);
	const std::uint8_t scaledIndex(index * sizeof(IntegerType));

	assert((scaledIndex < 16) && "Index out of bounds");

	uint128 expected(my_val());
	uint128 desired;
	union
	{
		std::uint8_t* int8Target;
		int_type_no_const* target;
	};
	int8Target = &desired.myU8[scaledIndex];

	do {
		desired = expected;
		*target -= value;
	} while (!cas_internal(expected.myS64, desired.myS64));

	return expected;
}
template<class IntegerType>
inline uint128 atomic_uint128::exchange_integer(const typename disable_deduction<IntegerType>::type& value, std::uint8_t atIndex)
{
	static_assert(std::is_integral<IntegerType>(), "Only integers allowed as value type");

	typedef typename std::remove_const<IntegerType>::type int_type_no_const;

	const std::uint8_t index(atIndex);
	const std::uint8_t scaledIndex(index * sizeof(IntegerType));

	assert((scaledIndex < 16) && "Index out of bounds");

	uint128 expected(my_val());
	uint128 desired;
	union
	{
		std::uint8_t* int8Target;
		int_type_no_const* target;
	};
	int8Target = &desired.myU8[scaledIndex];

	do {
		desired = expected;
		*target = value;
	} while (!cas_internal(expected.myS64, desired.myS64));

	return expected;
}
inline constexpr atomic_uint128::atomic_uint128(const uint128 & value)
	: myStorage(value)
{
}
bool atomic_uint128::compare_exchange_strong(uint128 & expected, const uint128& desired)
{
	return cas_internal(expected.myS64, desired.myS64);
}
uint128 atomic_uint128::exchange(const uint128& desired)
{
	uint128 expected(my_val());
	while (!compare_exchange_strong(expected, desired));
	return expected;
}
uint128 atomic_uint128::exchange_u64(std::uint64_t value, std::uint8_t atIndex)
{
	return exchange_integer<decltype(value)>(value, atIndex);
}
uint128 atomic_uint128::exchange_u32(std::uint32_t value, std::uint8_t atIndex)
{
	return exchange_integer<decltype(value)>(value, atIndex);
}

uint128 atomic_uint128::exchange_u16(std::uint16_t value, std::uint8_t atIndex)
{
	return exchange_integer<decltype(value)>(value, atIndex);
}

uint128 atomic_uint128::exchange_u8(const std::uint8_t value, std::uint8_t atIndex)
{
	return exchange_integer<decltype(value)>(value, atIndex);
}

void atomic_uint128::store(const uint128& desired)
{
	uint128 expected(my_val());
	while (!compare_exchange_strong(expected, desired));
}

inline uint128 atomic_uint128::load()
{
	uint128 expectedDesired;
	cas_internal(expectedDesired.myS64, expectedDesired.myS64);
	return expectedDesired;
}
uint128 atomic_uint128::fetch_add_to_u64(std::uint64_t value, std::uint8_t atIndex)
{
	return fetch_add_to_integer<decltype(value)>(value, atIndex);
}
uint128 atomic_uint128::fetch_add_to_u32(std::uint32_t value, std::uint8_t atIndex)
{
	return fetch_add_to_integer<decltype(value)>(value, atIndex);
}
uint128 atomic_uint128::fetch_add_to_u16(std::uint16_t value, std::uint8_t atIndex)
{
	return fetch_add_to_integer<decltype(value)>(value, atIndex);
}
uint128 atomic_uint128::fetch_add_to_u8(const std::uint8_t value, std::uint8_t atIndex)
{
	return fetch_add_to_integer<decltype(value)>(value, atIndex);
}
uint128 atomic_uint128::fetch_sub_to_u64(std::uint64_t value, std::uint8_t atIndex)
{
	return fetch_sub_to_integer<decltype(value)>(value, atIndex);
}
uint128 atomic_uint128::fetch_sub_to_u32(std::uint32_t value, std::uint8_t atIndex)
{
	return fetch_sub_to_integer<decltype(value)>(value, atIndex);
}
uint128 atomic_uint128::fetch_sub_to_u16(std::uint16_t value, std::uint8_t atIndex)
{
	return fetch_sub_to_integer<decltype(value)>(value, atIndex);
}
uint128 atomic_uint128::fetch_sub_to_u8(const std::uint8_t value, std::uint8_t atIndex)
{
	return fetch_sub_to_integer<decltype(value)>(value, atIndex);
}
constexpr const uint128& atomic_uint128::my_val() const
{
	return myStorage;
}
constexpr uint128 & atomic_uint128::my_val()
{
	return myStorage;
}
#if  defined(_MSC_VER) && !defined(__INTEL_COMPILER)
bool atomic_uint128::cas_internal(std::int64_t* const expected, const std::int64_t* desired)
{
	return _InterlockedCompareExchange128(reinterpret_cast<volatile std::int64_t*>(&myStorage.myS64[0]), desired[1], desired[0], expected);
}
#elif defined(__GNUC__) || defined(__clang__)
bool atomic_int128::cas_internal(std::int64_t* const expected, const std::int64_t* desired)
{
	bool result;
	__asm__ __volatile__
	(
		"lock cmpxchg16b %1\n\t"
		"setz %0"
		: "=q" (result)
		, "+m" (myStorage[0])
		, "+d" (expected[1])
		, "+a" (expected[0])
		: "c" (desired[1])
		, "b" (desired[0])
		: "cc", "memory"
	);
	return result;
}
#else
#error Unsupported platform
#endif
}

#pragma warning(pop)