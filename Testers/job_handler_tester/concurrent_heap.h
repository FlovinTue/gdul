
#include <assert.h>
#include <vector>
#include "spin_lock.h"
#include <atomic>

template <class T>
struct TinyLess;

template <class T, class Comparator = TinyLess<T>>
class concurrent_heap
{
public:
	typedef size_t SizeType;
	typedef uint64_t KeyType;

	concurrent_heap();
	concurrent_heap(const SizeType aInitCapacity);

	const SizeType size() const;

	void push(const T &aIn, const KeyType aKey);
	void push(T&& aIn, const KeyType aKey);

	bool try_pop(T& aOut);
	bool try_pop(T& aOutValue, KeyType& aOutKey);

	bool compare_try_pop(T& aOutValue, KeyType& aExpectedKey);

	bool try_peek_top_key(KeyType& aOut);

	void clear();

	void shrink_to_fit();

	void reserve(const size_t aCapacity);

private:
	void trickle(SizeType aIndex);
	void bubble(SizeType aIndex);

	void pop_internal(T& aOutValue, KeyType& aOutKey);

	std::vector<std::pair<KeyType, T>> myStorage;

	const Comparator myComparator;

	std::atomic<SizeType> mySize;

	spin_lock myLock;
};
template<class T, class Comparator>
concurrent_heap<T, Comparator>::concurrent_heap()
	: myComparator(Comparator())
	, mySize(0)
{
}
template<class T, class Comparator>
inline concurrent_heap<T, Comparator>::concurrent_heap(const SizeType aInitCapacity)
	: concurrent_heap()
{
	myStorage.reserve(aInitCapacity);
}
template<class T, class Comparator>
const inline typename concurrent_heap<T, Comparator>::SizeType concurrent_heap<T, Comparator>::size() const
{
	return mySize.load(std::memory_order_relaxed);
}
template<class T, class Comparator>
inline void concurrent_heap<T, Comparator>::push(const T & aIn, const KeyType aKey)
{
	myLock.lock();
	myStorage.emplace_back(std::pair<KeyType, T>( aKey, aIn));
	bubble(myStorage.size() - 1);
	mySize.fetch_add(1, std::memory_order_relaxed);
	myLock.unlock();
}
template<class T, class Comparator>
inline void concurrent_heap<T, Comparator>::push(T && aIn, const KeyType aKey)
{
	myLock.lock();
	myStorage.emplace_back(std::pair<KeyType, T>( aKey, std::move(aIn)));
	bubble(myStorage.size() - 1);
	mySize.fetch_add(1, std::memory_order_relaxed);
	myLock.unlock();
}
template<class T, class Comparator>
inline bool concurrent_heap<T, Comparator>::try_pop(T & aOut)
{
	KeyType dummy(0);
	return try_pop(aOut, dummy);
}
template<class T, class Comparator>
inline bool concurrent_heap<T, Comparator>::try_pop(T & aOutValue, KeyType & aOutKey)
{
	myLock.lock();

	if (!myStorage.size()) {
		myLock.unlock();
		return false;
	}

	pop_internal(aOutValue, aOutKey);

	myLock.unlock();

	return true;
}
template<class T, class Comparator>
inline void concurrent_heap<T, Comparator>::trickle(SizeType aIndex)
{
	const SizeType size(myStorage.size());

	if (aIndex == size - 1) {
		return;
	}

	SizeType index(aIndex);
	while (index < size) {
		const SizeType desiredLeftIndex(index * 2 + 1);
		const SizeType desiredRightIndex(index * 2 + 2);
		const SizeType leftIndex(desiredLeftIndex < size ? desiredLeftIndex : index);
		const SizeType rightIndex(desiredRightIndex < size ? desiredRightIndex : index);
		
		const KeyType& leftKey(myStorage[leftIndex].first);
		const KeyType& rightKey(myStorage[rightIndex].first);

		KeyType targetKey(myStorage[index].first);
		SizeType targetIndex(index);

		if (myComparator(leftKey, targetKey)) {
			targetIndex = leftIndex;
			targetKey = leftKey;
		}
		if (myComparator(rightKey, targetKey)) {
			targetIndex = rightIndex;
			targetKey = rightKey;
		}

		if (!(targetIndex ^ index)) {
			break;
		}

		std::swap(myStorage[index], myStorage[targetIndex]);

		index = targetIndex;
	}
}
template<class T, class Comparator>
inline void concurrent_heap<T, Comparator>::bubble(SizeType aIndex)
{
	SizeType index(aIndex);
	SizeType parent((index - 1) / 2);

	while (true) {
		if (!index)
			return;

		if (!myComparator(myStorage[index].first, myStorage[parent].first))
			break;

		std::swap(myStorage[index], myStorage[parent]);
		index = parent;
		parent = (index - 1) / 2;
	}
}
template<class T, class Comparator>
inline void concurrent_heap<T, Comparator>::pop_internal(T & aOutValue, KeyType & aOutKey)
{
	aOutKey = myStorage[0].first;
	aOutValue = std::move(myStorage[0].second);

	std::swap(myStorage[0], myStorage[myStorage.size() - 1]);
	myStorage.pop_back();

	trickle(0);

	mySize.fetch_sub(1, std::memory_order_relaxed);
}
template<class T, class Comparator>
inline void concurrent_heap<T, Comparator>::clear()
{
	myLock.lock();
	myStorage.clear();
	mySize.store(0, std::memory_order_relaxed);
	myLock.unlock();
}
template<class T, class Comparator>
inline void concurrent_heap<T, Comparator>::shrink_to_fit()
{
	myLock.lock();
	myStorage.shrink_to_fit();
	myLock.unlock();
}
template<class T, class Comparator>
inline void concurrent_heap<T, Comparator>::reserve(const size_t aCapacity)
{
	myLock.lock();
	myStorage.reserve(aCapacity);
	myLock.unlock();
}
template<class T, class Comparator>
inline bool concurrent_heap<T, Comparator>::compare_try_pop(T & aOutValue, KeyType & aExpectedKey)
{
	myLock.lock();
	if (!myStorage.size()) {
		myLock.unlock();
		return false;
	}

	if (myStorage[0].first != aExpectedKey) {
		aExpectedKey = myStorage[0].first;
		myLock.unlock();
		return false;
	}

	pop_internal(aOutValue, aExpectedKey);

	myLock.unlock();

	return true;
}

template<class T, class Comparator>
inline bool concurrent_heap<T, Comparator>::try_peek_top_key(KeyType & aOut)
{
	myLock.lock();

	if (!myStorage.size()) {
		myLock.unlock();
		return false;
	}

	aOut = myStorage[0].first;

	myLock.unlock();

	return true;
}

template <class T>
struct TinyLess
{
	constexpr const bool operator()(const T& aA, const T& aB) const
	{
		return aA < aB;
	};
};
