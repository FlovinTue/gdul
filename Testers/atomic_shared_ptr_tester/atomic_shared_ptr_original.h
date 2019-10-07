// Copyright(c) 2019 Flovin Michaelsen
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

#include <atomic>
#include <functional>
#include <stdint.h>
#include <atomic_oword.h>


#undef max

namespace gdul {

namespace aspdetail {

typedef std::allocator<uint8_t> default_allocator;

static const uint64_t Tag_Mask = 1;
static const uint64_t Ptr_Mask = (std::numeric_limits<uint64_t>::max() >> 16) & ~aspdetail::Tag_Mask;

template <class T>
class default_deleter;

template <class T>
struct disable_deduction;

template <class T, class Allocator>
class control_block;

template <class StorageType, class T, class Allocator = default_allocator>
class ptr_base;

}
template <class T, class Allocator = aspdetail::default_allocator>
class shared_ptr;

template <class T, class Allocator = aspdetail::default_allocator>
class versioned_raw_ptr;


template <class T, class ...Args>
inline shared_ptr<T> make_shared(Args&&...);
template <class T, class ...Args>
inline shared_ptr<T, aspdetail::default_allocator> make_shared(Args&&...);
template <class T, class Allocator, class ...Args>
inline shared_ptr<T, Allocator> make_shared(Allocator&, Args&&...);

#pragma warning(push)
#pragma warning(disable : 4201)

template <class T, class Allocator = aspdetail::default_allocator>
class alignas(64) atomic_shared_ptr : public aspdetail::ptr_base<atomic_oword, T, Allocator>
{
public:
	typedef typename aspdetail::ptr_base<atomic_oword, T, Allocator>::size_type size_type;

	inline constexpr atomic_shared_ptr();
	inline constexpr atomic_shared_ptr(std::nullptr_t);

	inline atomic_shared_ptr(const shared_ptr<T, Allocator>& from);
	inline atomic_shared_ptr(shared_ptr<T, Allocator>&& from);

	inline ~atomic_shared_ptr();

	inline bool compare_exchange_strong(versioned_raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired);
	inline bool compare_exchange_strong(versioned_raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired);

	inline bool compare_exchange_strong(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired);
	inline bool compare_exchange_strong(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired);

	inline atomic_shared_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& from);
	inline atomic_shared_ptr<T, Allocator>& operator=(shared_ptr<T, Allocator>&& from);

	inline shared_ptr<T, Allocator> load();
	inline shared_ptr<T, Allocator> load_and_tag();

	inline void store(const shared_ptr<T, Allocator>& from);
	inline void store(shared_ptr<T, Allocator>&& from);

	inline shared_ptr<T, Allocator> exchange(const shared_ptr<T, Allocator>& with);
	inline shared_ptr<T, Allocator> exchange(shared_ptr<T, Allocator>&& with);

	inline shared_ptr<T, Allocator> unsafe_load();

	inline shared_ptr<T, Allocator> unsafe_exchange(const shared_ptr<T, Allocator>& with);
	inline shared_ptr<T, Allocator> unsafe_exchange(shared_ptr<T, Allocator>&& with);

	inline void unsafe_store(const shared_ptr<T, Allocator>& from);
	inline void unsafe_store(shared_ptr<T, Allocator>&& from);

private:

	inline oword copy_internal();
	inline oword unsafe_copy_internal();
	inline oword unsafe_exchange_internal(const oword& with);

	inline void unsafe_store_internal(const oword& from);
	inline void store_internal(const oword& from);

	inline oword exchange_internal(const oword& to, bool decrementPrevious);

	template <class PtrType>
	inline bool compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type& expected, shared_ptr<T, Allocator>&& desired);

	inline bool increment_and_try_swap(oword& expected, const oword& desired);
	inline bool cas_internal(oword& expected, const oword& desired, bool decrementPrevious, bool captureOnFailure = false);
	inline void try_increment(oword& expected);

	enum STORAGE_QWORD : uint8_t
	{
		STORAGE_QWORD_CONTROLBLOCKPTR = 0,
		STORAGE_QWORD_OBJECTPTR = 1
	};
	enum STORAGE_WORD : uint8_t
	{
		STORAGE_WORD_COPYREQUEST = 3,
		STORAGE_WORD_VERSION = 7
	};

	using aspdetail::ptr_base<atomic_oword, T, Allocator>::to_control_block;

	friend class shared_ptr<T, Allocator>;
	friend class versioned_raw_ptr<T, Allocator>;
	friend class aspdetail::ptr_base<oword, T, Allocator>;
};
template <class T, class Allocator>
inline constexpr atomic_shared_ptr<T, Allocator>::atomic_shared_ptr()
{
	static_assert(std::is_same<Allocator::value_type, uint8_t>(), "value_type for allocator must be uint8_t");
}
template<class T, class Allocator>
inline constexpr atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(std::nullptr_t)
	: atomic_shared_ptr<T, Allocator>()
{
}
template <class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(shared_ptr<T, Allocator>&& other)
	: atomic_shared_ptr<T, Allocator>()
{
	unsafe_store(std::move(other));
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::atomic_shared_ptr(const shared_ptr<T, Allocator>& from)
	: atomic_shared_ptr<T, Allocator>()
{
	unsafe_store(from);
}
template <class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>::~atomic_shared_ptr()
{
	unsafe_store_internal(oword());
}

template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(versioned_raw_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired)
{
	return compare_exchange_strong(expected, shared_ptr<T, Allocator>(desired));
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(versioned_raw_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired)
{
	return compare_exchange_strong<decltype(expected)>(expected, std::move(desired));
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(shared_ptr<T, Allocator>& expected, const shared_ptr<T, Allocator>& desired)
{
	return compare_exchange_strong(expected, shared_ptr<T, Allocator>(desired));
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(shared_ptr<T, Allocator>& expected, shared_ptr<T, Allocator>&& desired)
{
	return compare_exchange_strong<decltype(expected)>(expected, std::move(desired));
}
template<class T, class Allocator>
template<class PtrType>
inline bool atomic_shared_ptr<T, Allocator>::compare_exchange_strong(typename aspdetail::disable_deduction<PtrType>::type & expected, shared_ptr<T, Allocator>&& desired)
{
	const oword desired_(desired.my_val());
	oword expected_(aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val());
	expected_.myU64[STORAGE_QWORD_OBJECTPTR] = expected.my_val().myU64[STORAGE_QWORD_OBJECTPTR];

	typedef typename std::remove_reference<PtrType>::type raw_type;

	do {
		if (cas_internal(expected_, desired_, true, std::is_same<raw_type, shared_ptr<T, Allocator>>())) {

			desired.my_val() = oword();

			return true;
		}

	} while (expected.my_val().myU64[STORAGE_QWORD_OBJECTPTR] == expected_.myU64[STORAGE_QWORD_OBJECTPTR]);

	expected = raw_type(expected_);

	return false;
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>& atomic_shared_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& from)
{
	store(from);
	return *this;
}
template<class T, class Allocator>
inline atomic_shared_ptr<T, Allocator>& atomic_shared_ptr<T, Allocator>::operator=(shared_ptr<T, Allocator>&& from)
{
	store(std::move(from));
	return *this;
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::load()
{
	return shared_ptr<T, Allocator>(copy_internal());
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store(const shared_ptr<T, Allocator>& from)
{
	store(shared_ptr<T, Allocator>(from));
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store(shared_ptr<T, Allocator>&& from)
{
	store_internal(from.my_val());
	from.my_val() = oword();
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::exchange(const shared_ptr<T, Allocator>& with)
{
	return exchange(shared_ptr<T, Allocator>(with));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::exchange(shared_ptr<T, Allocator>&& with)
{
	const oword next(with.my_val());
	with.my_val() = oword();
	return shared_ptr<T, Allocator>(exchange_internal(next, false));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_load()
{
	return shared_ptr<T, Allocator>(unsafe_copy_internal());
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_exchange(const shared_ptr<T, Allocator>& with)
{
	return unsafe_exchange(shared_ptr<T, Allocator>(with));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::unsafe_exchange(shared_ptr<T, Allocator>&& with)
{
	const shared_ptr<T, Allocator> returnValue(unsafe_exchange_internal(with.my_val()));
	with.my_val() = oword();
	return returnValue;
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store(const shared_ptr<T, Allocator>& from)
{
	unsafe_store(shared_ptr<T, Allocator>(from));
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store(shared_ptr<T, Allocator>&& from)
{
	unsafe_store_internal(from.my_val());
	from.my_val() = oword();
}

template<class T, class Allocator>
inline shared_ptr<T, Allocator> atomic_shared_ptr<T, Allocator>::load_and_tag()
{
	oword expected(aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val());
	oword desired;
	do {
		desired = expected;
		desired.myU16[STORAGE_WORD_COPYREQUEST] += 1;
		desired.myU64[STORAGE_QWORD_OBJECTPTR] |= aspdetail::Tag_Mask;

	} while (!aspdetail::ptr_base<atomic_oword, T, Allocator>::myStorage.compare_exchange_strong(expected, desired));

	try_increment(desired);

	return shared_ptr<T, Allocator>(expected);
}

// ------------------------------------------------------------------------------------

// -------------------------Internals--------------------------------------------------

// ------------------------------------------------------------------------------------
template <class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::store_internal(const oword& from)
{
	exchange_internal(from, true);
}
template <class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::increment_and_try_swap(oword & expected, const oword & desired)
{
	const uint64_t initialVersionedPtr(expected.myU64[STORAGE_QWORD_OBJECTPTR]);

	aspdetail::control_block<T, Allocator>* const controlBlock(to_control_block(expected));

	oword desired_(desired);
	desired_.myU16[STORAGE_WORD_COPYREQUEST] = 0;

	do {
		const uint16_t copyRequests(expected.myU16[STORAGE_WORD_COPYREQUEST]);

		if (controlBlock)
			(*controlBlock) += copyRequests;

		if (aspdetail::ptr_base<atomic_oword, T, Allocator>::myStorage.compare_exchange_strong(expected, desired_)) {
			return true;
		}
		else {
			if (controlBlock)
				(*controlBlock) -= copyRequests;
		}

	} while (expected.myU64[STORAGE_QWORD_OBJECTPTR] == initialVersionedPtr);

	return false;
}
template<class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::try_increment(oword & expected)
{
	const uint64_t initialVersionedPtr(expected.myU64[STORAGE_QWORD_OBJECTPTR]);

	aspdetail::control_block<T, Allocator>* const controlBlock(to_control_block(expected));

	if (!controlBlock) {
		return;
	}

	oword desired(expected);
	desired.myU16[STORAGE_WORD_COPYREQUEST] = 0;

	do {
		const uint16_t copyRequests(expected.myU16[STORAGE_WORD_COPYREQUEST]);

		(*controlBlock) += copyRequests;

		if (aspdetail::ptr_base<atomic_oword, T, Allocator>::myStorage.compare_exchange_strong(expected, desired)) {
			return;
		}
		(*controlBlock) -= copyRequests;

	} while (
		expected.myU64[STORAGE_QWORD_OBJECTPTR] == initialVersionedPtr &&
		expected.myU16[STORAGE_WORD_COPYREQUEST]);
}
template<class T, class Allocator>
inline bool atomic_shared_ptr<T, Allocator>::cas_internal(oword & expected, const oword & desired, bool decrementPrevious, bool captureOnFailure)
{
	bool success(false);

	aspdetail::control_block<T, Allocator>* controlBlock(to_control_block(expected));

	oword desired_(desired);
	desired_.myU16[STORAGE_WORD_VERSION] = expected.myU16[STORAGE_WORD_VERSION] + 1;
	desired_.myU16[STORAGE_WORD_COPYREQUEST] = 0;

	if (expected.myU16[STORAGE_WORD_COPYREQUEST]) {

		oword expected_(aspdetail::ptr_base<atomic_oword, T, Allocator>::myStorage.fetch_add_to_word(1, STORAGE_WORD_COPYREQUEST));
		expected_.myU16[STORAGE_WORD_COPYREQUEST] += 1;

		controlBlock = to_control_block(expected_);

		const uint64_t oldObjectBlock(expected.myU64[STORAGE_QWORD_OBJECTPTR]);
		expected = expected_;

		if (expected_.myU64[STORAGE_QWORD_OBJECTPTR] == oldObjectBlock) {
			success = increment_and_try_swap(expected, desired_);
		}
		else{
			try_increment(expected_);
		}

		if (controlBlock) {
			(*controlBlock) -= static_cast<size_type>(!(captureOnFailure & !success)) + static_cast<size_type>(decrementPrevious & success);
		}
	}
	else {
		success = aspdetail::ptr_base<atomic_oword, T, Allocator>::myStorage.compare_exchange_strong(expected, desired_);

		if (static_cast<bool>(controlBlock) & decrementPrevious & success) {
			--(*controlBlock);
		}
		if (!success & captureOnFailure) {
			expected = copy_internal();
		}
	}
	return success;
}

template <class T, class Allocator>
inline oword atomic_shared_ptr<T, Allocator>::copy_internal()
{
	oword initial(aspdetail::ptr_base<atomic_oword, T, Allocator>::myStorage.fetch_add_to_word(1, STORAGE_WORD_COPYREQUEST));
	initial.myU16[STORAGE_WORD_COPYREQUEST] += 1;

	if (to_control_block(initial)) {
		oword expected(initial);
		try_increment(expected);
	}

	initial.myU16[STORAGE_WORD_COPYREQUEST] = 0;

	return initial;
}
template <class T, class Allocator>
inline oword atomic_shared_ptr<T, Allocator>::unsafe_copy_internal()
{
	std::atomic_thread_fence(std::memory_order_acquire);

	if (aspdetail::ptr_base<atomic_oword, T, Allocator>::operator bool()) {
		++(*aspdetail::ptr_base<atomic_oword, T, Allocator>::get_control_block());
	}

	return aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val();
}
template<class T, class Allocator>
inline oword atomic_shared_ptr<T, Allocator>::unsafe_exchange_internal(const oword & with)
{
	std::atomic_thread_fence(std::memory_order_acquire);

	const oword old(aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val());

	aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val() = with;
	aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val().myU16[STORAGE_WORD_VERSION] = old.myU16[STORAGE_WORD_VERSION] + 1;

	std::atomic_thread_fence(std::memory_order_release);

	return old;
}
template <class T, class Allocator>
inline void atomic_shared_ptr<T, Allocator>::unsafe_store_internal(const oword & from)
{
	std::atomic_thread_fence(std::memory_order_acquire);

	const oword previous(aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val());
	aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val() = from;

	if (to_control_block(previous)) {
		--(*to_control_block(previous));
	}
	else {
		std::atomic_thread_fence(std::memory_order_release);
	}
}
template<class T, class Allocator>
inline oword atomic_shared_ptr<T, Allocator>::exchange_internal(const oword & to, bool decrementPrevious)
{
	oword expected(aspdetail::ptr_base<atomic_oword, T, Allocator>::my_val());
	while (!cas_internal(expected, to, decrementPrevious));
	return expected;
}
namespace aspdetail {
template <class T, class Allocator>
class control_block
{
public:
	typedef typename atomic_shared_ptr<T>::size_type size_type;

	control_block(std::size_t blockSize, T* object, Allocator& allocator);
	template <class Deleter>
	control_block(std::size_t blockSize, T* object, Deleter&& deleter, Allocator& allocator);

	T* get_owned();
	const T* get_owned() const;

	size_type use_count() const;

	size_type operator--();
	size_type operator++();
	size_type operator-=(size_type decrement);
	size_type operator+=(size_type increment);

private:
	friend class atomic_shared_ptr<T>;

	void destroy();

	std::atomic<size_type> myUseCount;
	std::function<void(T*)> myDeleter;
	const std::size_t myBlockSize;
	T* const myPtr;
	Allocator myAllocator;
};

template<class T, class Allocator>
inline aspdetail::control_block<T, Allocator>::control_block(std::size_t blockSize, T* object, Allocator& allocator)
	: myUseCount(1)
	, myDeleter([](T* object) { object->~T(); })
	, myPtr(object)
	, myBlockSize(blockSize)
	, myAllocator(allocator)
{
}
template <class T, class Allocator>
template<class Deleter>
inline aspdetail::control_block<T, Allocator>::control_block(std::size_t blockSize, T* object, Deleter&& deleter, Allocator& allocator)
	: myUseCount(1)
	, myDeleter(std::forward<Deleter&&>(deleter))
	, myPtr(object)
	, myBlockSize(blockSize)
	, myAllocator(allocator)
{
}
template <class T, class Allocator>
inline typename control_block<T, Allocator>::size_type control_block<T, Allocator>::operator--()
{
	const size_type useCount(myUseCount.fetch_sub(1, std::memory_order_acq_rel) - 1);
	if (!useCount) {
		destroy();
	}
	return useCount;
}
template <class T, class Allocator>
inline typename control_block<T, Allocator>::size_type control_block<T, Allocator>::operator++()
{
	return myUseCount.fetch_add(1, std::memory_order_relaxed) + 1;
}
template <class T, class Allocator>
inline typename control_block<T, Allocator>::size_type control_block<T, Allocator>::operator-=(size_type decrement)
{
	const size_type useCount(myUseCount.fetch_sub(decrement, std::memory_order_acq_rel) - decrement);
	if (!useCount) {
		destroy();
	}
	return useCount;
}
template <class T, class Allocator>
inline typename control_block<T, Allocator>::size_type control_block<T, Allocator>::operator+=(size_type increment)
{
	return myUseCount.fetch_add(increment, std::memory_order_relaxed) + increment;
}
template <class T, class Allocator>
inline T* control_block<T, Allocator>::get_owned()
{
	return myPtr;
}
template<class T, class Allocator>
inline const T* control_block<T, Allocator>::get_owned() const
{
	return myPtr;
}
template <class T, class Allocator>
inline typename control_block<T, Allocator>::size_type control_block<T, Allocator>::use_count() const
{
	return myUseCount.load(std::memory_order_acquire);
}
template <class T, class Allocator>
inline void control_block<T, Allocator>::destroy()
{
	myDeleter(get_owned());
	(*this).~control_block<T, Allocator>();
	myAllocator.deallocate(reinterpret_cast<uint8_t*>(this), myBlockSize);
}
template <class T>
class default_deleter
{
public:
	void operator()(T* object);
};
template<class T>
inline void default_deleter<T>::operator()(T* object)
{
	delete object;
}
template <class T>
struct disable_deduction
{
	using type = T;
};
template <class StorageType, class T, class Allocator>
class ptr_base {
public:
	typedef std::size_t size_type;

	inline constexpr operator bool() const;

	inline constexpr bool operator==(const ptr_base<StorageType, T, Allocator>& other) const;
	inline constexpr bool operator!=(const ptr_base<StorageType, T, Allocator>& other) const;

	inline constexpr explicit operator T*();
	inline constexpr explicit operator const T*() const;

	inline constexpr const control_block<T, Allocator>* get_control_block() const;
	inline constexpr const T* get_owned() const;

	inline constexpr control_block<T, Allocator>* get_control_block();
	inline constexpr T* get_owned();

	template <class U = StorageType, std::enable_if_t<std::is_same<U, atomic_oword>::value>* = nullptr>
	inline const versioned_raw_ptr<T, Allocator> get_versioned_raw_ptr();

	template <class U = StorageType, std::enable_if_t<std::is_same<U, oword>::value>* = nullptr>
	inline constexpr const versioned_raw_ptr<T, Allocator> get_versioned_raw_ptr() const;

	template <class U = StorageType, std::enable_if_t<std::is_same<U, atomic_oword>::value > * = nullptr>
	inline constexpr bool get_tag() const;
	template <class U = StorageType, std::enable_if_t<std::is_same<U, oword>::value > * = nullptr>
	inline constexpr bool get_tag() const;

	template <class U = StorageType, std::enable_if_t<std::is_same<U, atomic_oword>::value > * = nullptr>
	inline constexpr void set_tag();
	template <class U = StorageType, std::enable_if_t<std::is_same<U, oword>::value > * = nullptr>
	inline constexpr void set_tag();

	template <class U = StorageType, std::enable_if_t<std::is_same<U, atomic_oword>::value>* = nullptr>
	inline constexpr void clear_tag();
	template <class U = StorageType, std::enable_if_t<std::is_same<U, oword>::value>* = nullptr>
	inline constexpr void clear_tag();


	inline constexpr const uint16_t get_version() const;

	/*	Concurrency UNSAFE	*/

	// These methods may be used safely so long as no other 
	// thread is reassigning or otherwise altering the state of 
	// this object
	//--------------------------------------------------------------//
	inline size_type use_count() const;

	inline constexpr T* operator->();
	inline constexpr T& operator*();

	inline constexpr const T* operator->() const;
	inline constexpr const T& operator*() const;

	inline const T& operator[](size_type index) const;
	inline T& operator[](size_type index);

	//--------------------------------------------------------------//

protected:
	inline constexpr ptr_base();
	inline constexpr ptr_base(const oword& from);

	inline constexpr const oword& my_val() const;
	inline constexpr oword& my_val();

	constexpr control_block<T, Allocator>* to_control_block(const oword& from);
	constexpr T* to_object(const oword& from);

	constexpr const control_block<T, Allocator>* to_control_block(const oword& from) const;
	constexpr const T* to_object(const oword& from) const;


	enum STORAGE_QWORD : uint8_t
	{
		STORAGE_QWORD_CONTROLBLOCKPTR = 0,
		STORAGE_QWORD_OBJECTPTR = 1
	};
	enum STORAGE_WORD : uint8_t
	{
		STORAGE_WORD_COPYREQUEST = 3,
		STORAGE_WORD_VERSION = 7
	};

	friend class atomic_shared_ptr<T, Allocator>;

	union {
		StorageType myStorage;

		oword myDebugView;
	};
};
template <class StorageType, class T, class Allocator>
inline constexpr ptr_base<StorageType, T, Allocator>::ptr_base()
	: myStorage()
{
}
template <class StorageType, class T, class Allocator>
inline constexpr ptr_base<StorageType, T, Allocator>::ptr_base(const oword & from)
	: ptr_base<StorageType, T, Allocator>()
{
	my_val() = from;
	my_val().myU16[STORAGE_WORD_COPYREQUEST] = 0;
}
template <class StorageType, class T, class Allocator>
inline constexpr ptr_base<StorageType, T, Allocator>::operator bool() const
{
	return get_owned();
}
template <class StorageType, class T, class Allocator>
inline constexpr bool ptr_base<StorageType, T, Allocator>::operator==(const ptr_base<StorageType, T, Allocator>& other) const
{
	return (my_val().myU64[STORAGE_QWORD_OBJECTPTR]) == (other.my_val().myU64[STORAGE_QWORD_OBJECTPTR]);
}
template <class StorageType, class T, class Allocator>
inline constexpr bool ptr_base<StorageType, T, Allocator>::operator!=(const ptr_base<StorageType, T, Allocator>& other) const
{
	return !operator==(other);
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline typename ptr_base<StorageType, T, Allocator>::size_type ptr_base<StorageType, T, Allocator>::use_count() const
{
	if (!operator bool()) {
		return 0;
	}

	return get_control_block()->use_count();
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline constexpr ptr_base<StorageType, T, Allocator>::operator T*()
{
	return get_owned();
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline constexpr ptr_base<StorageType, T, Allocator>::operator const T*() const
{
	return get_owned();
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline constexpr T* ptr_base<StorageType, T, Allocator>::operator->()
{
	return get_owned();
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline constexpr T & ptr_base<StorageType, T, Allocator>::operator*()
{
	return *get_owned();
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline constexpr const T* ptr_base<StorageType, T, Allocator>::operator->() const
{
	return get_owned();
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline constexpr const T & ptr_base<StorageType, T, Allocator>::operator*() const
{
	return *get_owned();
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline const T & ptr_base<StorageType, T, Allocator>::operator[](size_type index) const
{
	return(*get_owned())[index];
}
// Concurrency UNSAFE
// May be used safely so long as no other thread is reassigning or
// otherwise altering the state of this object
template <class StorageType, class T, class Allocator>
inline T & ptr_base<StorageType, T, Allocator>::operator[](size_type index)
{
	return(*get_owned())[index];
}
// Concurrency SAFE
// Safe to use, however, may yield fleeting results if this object is reassigned 
// during use
template <class StorageType, class T, class Allocator>
inline constexpr bool operator==(std::nullptr_t /*aNullptr*/, const ptr_base<StorageType, T, Allocator>& ptr)
{
	return !ptr;
}
// Concurrency SAFE
// Safe to use, however, may yield fleeting results if this object is reassigned 
// during use
template <class StorageType, class T, class Allocator>
inline constexpr bool operator!=(std::nullptr_t /*aNullptr*/, const ptr_base<StorageType, T, Allocator>& ptr)
{
	return ptr;
}
// Concurrency SAFE
// Safe to use, however, may yield fleeting results if this object is reassigned 
// during use
template <class StorageType, class T, class Allocator>
inline constexpr bool operator==(const ptr_base<StorageType, T, Allocator>& ptr, std::nullptr_t /*aNullptr*/)
{
	return !ptr;
}
// Concurrency SAFE
// Safe to use, however, may yield fleeting results if this object is reassigned 
// during use
template <class StorageType, class T, class Allocator>
inline constexpr bool operator!=(const ptr_base<StorageType, T, Allocator>& ptr, std::nullptr_t /*aNullptr*/)
{
	return ptr;
}
template<class StorageType, class T, class Allocator>
inline constexpr const oword & ptr_base<StorageType, T, Allocator>::my_val() const
{
	return reinterpret_cast<const oword&>(*(&myStorage));
}
template<class StorageType, class T, class Allocator>
inline constexpr oword & ptr_base<StorageType, T, Allocator>::my_val()
{
	return reinterpret_cast<oword&>(*(&myStorage));
}
template <class StorageType, class T, class Allocator>
inline constexpr control_block<T, Allocator>* ptr_base<StorageType, T, Allocator>::to_control_block(const oword & from)
{
	return reinterpret_cast<control_block<T, Allocator>*>(from.myU64[STORAGE_QWORD_CONTROLBLOCKPTR] & Ptr_Mask);
}
template <class StorageType, class T, class Allocator>
inline constexpr T* ptr_base<StorageType, T, Allocator>::to_object(const oword & from)
{
	return reinterpret_cast<T*>(from.myU64[STORAGE_QWORD_OBJECTPTR] & Ptr_Mask);
}
template <class StorageType, class T, class Allocator>
inline constexpr const control_block<T, Allocator>* ptr_base<StorageType, T, Allocator>::to_control_block(const oword & from) const
{
	return reinterpret_cast<const control_block<T, Allocator>*>(from.myU64[STORAGE_QWORD_CONTROLBLOCKPTR] & Ptr_Mask);
}
template <class StorageType, class T, class Allocator>
inline constexpr const T* ptr_base<StorageType, T, Allocator>::to_object(const oword & from) const
{
	return reinterpret_cast<const T*>(from.myU64[STORAGE_QWORD_OBJECTPTR] & Ptr_Mask);
}
template <class StorageType, class T, class Allocator>
inline constexpr const control_block<T, Allocator>* ptr_base<StorageType, T, Allocator>::get_control_block() const
{
	return to_control_block(my_val());
}
template <class StorageType, class T, class Allocator>
inline constexpr const T* ptr_base<StorageType, T, Allocator>::get_owned() const
{
	return to_object(my_val());
}
template <class StorageType, class T, class Allocator>
inline constexpr control_block<T, Allocator>* ptr_base<StorageType, T, Allocator>::get_control_block()
{
	return to_control_block(my_val());
}
template <class StorageType, class T, class Allocator>
inline constexpr T* ptr_base<StorageType, T, Allocator>::get_owned()
{
	return to_object(my_val());
}
template<class StorageType, class T, class Allocator>
inline constexpr const uint16_t ptr_base<StorageType, T, Allocator>::get_version() const
{
	return my_val().myU16[STORAGE_WORD_VERSION];
}
template<class StorageType, class T, class Allocator>
template<class U, std::enable_if_t<std::is_same<U, atomic_oword>::value>*>
inline const versioned_raw_ptr<T, Allocator> ptr_base<StorageType, T, Allocator>::get_versioned_raw_ptr()
{
	return versioned_raw_ptr<T, Allocator>(myStorage.load());
}
template<class StorageType, class T, class Allocator>
template<class U, std::enable_if_t<std::is_same<U, oword>::value>*>
inline constexpr const versioned_raw_ptr<T, Allocator> ptr_base<StorageType, T, Allocator>::get_versioned_raw_ptr() const
{
	return versioned_raw_ptr<T, Allocator>(my_val());
}
template<class StorageType, class T, class Allocator>
template<class U, std::enable_if_t<std::is_same<U, atomic_oword>::value > *>
inline constexpr bool ptr_base<StorageType, T, Allocator>::get_tag() const
{
	std::atomic_thread_fence(std::memory_order_acquire);
	return my_val().myU64[STORAGE_QWORD_OBJECTPTR] & Tag_Mask;
}
template<class StorageType, class T, class Allocator>
template<class U, std::enable_if_t<std::is_same<U, oword>::value > *>
inline constexpr bool ptr_base<StorageType, T, Allocator>::get_tag() const
{
	return my_val().myU64[STORAGE_QWORD_OBJECTPTR] & Tag_Mask;
}
template<class StorageType, class T, class Allocator>
template<class U, std::enable_if_t<std::is_same<U, atomic_oword>::value > *>
inline constexpr void ptr_base<StorageType, T, Allocator>::set_tag()
{
	my_val().myU64[STORAGE_QWORD_OBJECTPTR] |= Tag_Mask;
	std::atomic_thread_fence(std::memory_order_release);
}
template<class StorageType, class T, class Allocator>
template<class U, std::enable_if_t<std::is_same<U, atomic_oword>::value>*>
inline constexpr void ptr_base<StorageType, T, Allocator>::clear_tag()
{
	my_val().myU64[STORAGE_QWORD_OBJECTPTR] &= ~Tag_Mask;
	std::atomic_thread_fence(std::memory_order_release);
}
template<class StorageType, class T, class Allocator>
template<class U, std::enable_if_t<std::is_same<U, oword>::value > *>
inline constexpr void ptr_base<StorageType, T, Allocator>::clear_tag()
{
	my_val().myU64[STORAGE_QWORD_OBJECTPTR] &= ~Tag_Mask;
}
template<class StorageType, class T, class Allocator>
template<class U, std::enable_if_t<std::is_same<U, oword>::value > *>
inline constexpr void ptr_base<StorageType, T, Allocator>::set_tag()
{
	my_val().myU64[STORAGE_QWORD_OBJECTPTR] |= Tag_Mask;
}
}
template <class T, class Allocator>
class shared_ptr : public aspdetail::ptr_base<oword, T, Allocator> {
public:
	inline constexpr shared_ptr();

	inline shared_ptr(const shared_ptr<T, Allocator>& other);
	inline shared_ptr(shared_ptr<T, Allocator>&& other);
	inline shared_ptr(std::nullptr_t);

	inline explicit shared_ptr(T* object);
	template <class Deleter>
	inline explicit shared_ptr(T* object, Deleter&& deleter);
	template <class Deleter>
	inline explicit shared_ptr(T* object, Deleter&& deleter, Allocator& allocator);

	~shared_ptr();

	shared_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& other);
	shared_ptr<T, Allocator>& operator=(shared_ptr<T, Allocator>&& other);

	// The amount of memory requested from the allocator when calling
	// make_shared
	static constexpr std::size_t alloc_size_make_shared();

	// The amount of memory requested from the allocator when taking 
	// ownership of an object
	static constexpr std::size_t alloc_size_claim();

private:
	shared_ptr(const oword& from);

	template<class Deleter>
	inline oword create_control_block(T* object, Deleter&& deleter, Allocator& allocator);

	template <class T, class Allocator, class ...Args>
	friend shared_ptr<T, Allocator> make_shared<T, Allocator>(Allocator&, Args&&...);

	friend class versioned_raw_ptr<T, Allocator>;
	friend class atomic_shared_ptr<T, Allocator>;
};
template<class T, class Allocator>
inline constexpr shared_ptr<T, Allocator>::shared_ptr()
	: aspdetail::ptr_base<oword, T, Allocator>()
{
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::~shared_ptr()
{
	if (aspdetail::ptr_base<oword, T, Allocator>::get_control_block()) {
		--(*aspdetail::ptr_base<oword, T, Allocator>::get_control_block());
	}
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(shared_ptr<T, Allocator>&& other)
	: shared_ptr<T, Allocator>()
{
	operator=(std::move(other));
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(std::nullptr_t)
	: shared_ptr<T, Allocator>()
{
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(const shared_ptr<T, Allocator>& other)
	: shared_ptr<T, Allocator>()
{
	operator=(other);
}
template <class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(T * object)
	: shared_ptr<T, Allocator>()
{
	Allocator alloc;
	aspdetail::ptr_base<oword, T, Allocator>::my_val() = create_control_block(object, aspdetail::default_deleter<T>(), alloc);
}
// The Deleter callable has signature void(T* arg)
template <class T, class Allocator>
template<class Deleter>
inline shared_ptr<T, Allocator>::shared_ptr(T* object, Deleter&& deleter)
	: shared_ptr<T, Allocator>()
{
	Allocator alloc;
	aspdetail::ptr_base<oword, T, Allocator>::my_val() = create_control_block(object, std::forward<Deleter&&>(deleter), alloc);
}
// The Deleter callable has signature void(T* arg)
template<class T, class Allocator>
template<class Deleter>
inline shared_ptr<T, Allocator>::shared_ptr(T* object, Deleter && deleter, Allocator & allocator)
	: shared_ptr<T, Allocator>()
{
	aspdetail::ptr_base<oword, T, Allocator>::my_val() = create_control_block(object, std::forward<Deleter&&>(deleter), allocator);
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>::shared_ptr(const oword & from)
	: aspdetail::ptr_base<oword, T, Allocator>(from)
{
}
template <class T, class Allocator>
template<class Deleter>
inline oword shared_ptr<T, Allocator>::create_control_block(T* object, Deleter&& deleter, Allocator& allocator)
{
	aspdetail::control_block<T, Allocator>* controlBlock(nullptr);

	const std::size_t blockSize(sizeof(aspdetail::control_block<T, Allocator>));

	void* block(nullptr);

	try {
		block = allocator.allocate(blockSize);
		controlBlock = new (block) aspdetail::control_block<T, Allocator>(blockSize, object, std::forward<Deleter&&>(deleter), allocator);
	}
	catch (...) {
		allocator.deallocate(static_cast<uint8_t*>(block), blockSize);
		deleter(object);
		throw;
	}

	oword returnValue;
	returnValue.myU64[aspdetail::ptr_base<oword, T, Allocator>::STORAGE_QWORD_CONTROLBLOCKPTR] = reinterpret_cast<uint64_t>(controlBlock);
	returnValue.myU64[aspdetail::ptr_base<oword, T, Allocator>::STORAGE_QWORD_OBJECTPTR] = reinterpret_cast<uint64_t>(object);

	return returnValue;
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>& shared_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& other)
{
	const oword otherValue(other.my_val());

	if (aspdetail::ptr_base<oword, T, Allocator>::to_control_block(otherValue)) {
		++(*aspdetail::ptr_base<oword, T, Allocator>::to_control_block(otherValue));
	}
	if (aspdetail::ptr_base<oword, T, Allocator>::get_control_block()) {
		--(*aspdetail::ptr_base<oword, T, Allocator>::get_control_block());
	}

	aspdetail::ptr_base<oword, T, Allocator>::my_val() = otherValue;

	return *this;
}
template<class T, class Allocator>
inline shared_ptr<T, Allocator>& shared_ptr<T, Allocator>::operator=(shared_ptr<T, Allocator>&& other)
{
	std::swap(aspdetail::ptr_base<oword, T, Allocator>::my_val(), other.my_val());
	return *this;
}
template<class T, class Allocator>
inline constexpr std::size_t shared_ptr<T, Allocator>::alloc_size_make_shared()
{
	return sizeof(aspdetail::control_block<T, Allocator>) + (1 < alignof(T) ? alignof(T) : 2) + sizeof(T);
}
template<class T, class Allocator>
inline constexpr std::size_t shared_ptr<T, Allocator>::alloc_size_claim()
{
	return sizeof(aspdetail::control_block<T, Allocator>);
}
// versioned_raw_ptr does not share in ownership of the object
template <class T, class Allocator>
class versioned_raw_ptr : public aspdetail::ptr_base<oword, T, Allocator> {
public:
	constexpr versioned_raw_ptr();
	constexpr versioned_raw_ptr(std::nullptr_t);

	constexpr versioned_raw_ptr(versioned_raw_ptr<T, Allocator>&& other);
	constexpr versioned_raw_ptr(const versioned_raw_ptr<T, Allocator>& other);

	explicit constexpr versioned_raw_ptr(const shared_ptr<T, Allocator>& from);

	explicit versioned_raw_ptr(const atomic_shared_ptr<T, Allocator>& from);

	constexpr versioned_raw_ptr<T, Allocator>& operator=(const versioned_raw_ptr<T, Allocator>& other);
	constexpr versioned_raw_ptr<T, Allocator>& operator=(versioned_raw_ptr<T, Allocator>&& other);
	constexpr versioned_raw_ptr<T, Allocator>& operator=(const shared_ptr<T, Allocator>& from);
	constexpr versioned_raw_ptr<T, Allocator>& operator=(const atomic_shared_ptr<T, Allocator>& from);

private:
	explicit constexpr versioned_raw_ptr(const oword& from);

	friend class aspdetail::ptr_base<atomic_oword, T, Allocator>;
	friend class aspdetail::ptr_base<oword, T, Allocator>;
	friend class atomic_shared_ptr<T, Allocator>;
};
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr()
	: aspdetail::ptr_base<oword, T, Allocator>()
{
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(std::nullptr_t)
	: versioned_raw_ptr<T, Allocator>()
{
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(versioned_raw_ptr<T, Allocator>&& other)
	: versioned_raw_ptr()
{
	operator=(std::move(other));
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(const versioned_raw_ptr<T, Allocator>& other)
	: versioned_raw_ptr()
{
	operator=(other);
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(const shared_ptr<T, Allocator>& from)
	: aspdetail::ptr_base<oword, T, Allocator>(from.my_val())
{
}
template<class T, class Allocator>
inline versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(const atomic_shared_ptr<T, Allocator>& from)
	: aspdetail::ptr_base<oword, T, Allocator>(from.myStorage.load())
{
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>& versioned_raw_ptr<T, Allocator>::operator=(const versioned_raw_ptr<T, Allocator>& other) {
	aspdetail::ptr_base<oword, T, Allocator>::my_val() = other.my_val();

	return *this;
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>& versioned_raw_ptr<T, Allocator>::operator=(versioned_raw_ptr<T, Allocator>&& other) {
	std::swap(aspdetail::ptr_base<oword, T, Allocator>::my_val(), other.my_val());

	return *this;
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>& versioned_raw_ptr<T, Allocator>::operator=(const shared_ptr<T, Allocator>& from)
{
	*this = versioned_raw_ptr<T, Allocator>(from);
	return *this;
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>& versioned_raw_ptr<T, Allocator>::operator=(const atomic_shared_ptr<T, Allocator>& from)
{
	*this = versioned_raw_ptr<T, Allocator>(from);
	return *this;
}
template<class T, class Allocator>
inline constexpr versioned_raw_ptr<T, Allocator>::versioned_raw_ptr(const oword & from)
	: aspdetail::ptr_base<oword, T, Allocator>(from)
{
}
template<class T, class ...Args>
inline shared_ptr<T, aspdetail::default_allocator> make_shared(Args&& ...args)
{
	aspdetail::default_allocator alloc;
	return make_shared<T>(alloc, std::forward<Args&&>(args)...);
}
template<class T, class Allocator, class ...Args>
inline shared_ptr<T, Allocator> make_shared(Args&& ...args)
{
	Allocator alloc;
	return make_shared<T, Allocator>(alloc, std::forward<Args&&>(args)...);
}
template<class T, class Allocator, class ...Args>
inline shared_ptr<T, Allocator> make_shared(Allocator& allocator, Args&& ...args)
{
	shared_ptr<T, Allocator> returnValue;

	const std::size_t controlBlockSize(sizeof(aspdetail::control_block<T, Allocator>));
	const std::size_t objectSize(sizeof(T));
	const std::size_t alignment(1 < alignof(T) ? alignof(T) : 2);
	const std::size_t blockSize(controlBlockSize + objectSize + alignment);

	uint8_t* block(allocator.allocate(blockSize));

	const std::size_t controlBlockOffset(0);
	const std::size_t controlBlockEndAddr(reinterpret_cast<std::size_t>(block + controlBlockSize));
	const std::size_t alignmentRemainder(controlBlockEndAddr % alignment);
	const std::size_t alignmentOffset(alignment - (alignmentRemainder ? alignmentRemainder : alignment));
	const std::size_t objectOffset(controlBlockOffset + controlBlockSize + alignmentOffset);

	aspdetail::control_block<T, Allocator> * controlBlock(nullptr);
	T* object(nullptr);
	try {
		object = new (reinterpret_cast<uint8_t*>(block) + objectOffset) T(std::forward<Args&&>(args)...);
		controlBlock = new (reinterpret_cast<uint8_t*>(block) + controlBlockOffset) aspdetail::control_block<T, Allocator>(blockSize, object, allocator);
	}
	catch (...) {
		if (object) {
			(*object).~T();
		}
		allocator.deallocate(block, blockSize);
		throw;
	}

	returnValue.my_val().myU64[returnValue.STORAGE_QWORD_CONTROLBLOCKPTR] = reinterpret_cast<uint64_t>(controlBlock);
	returnValue.my_val().myU64[returnValue.STORAGE_QWORD_OBJECTPTR] = reinterpret_cast<uint64_t>(object);

	return returnValue;
};
}
#pragma warning(pop)