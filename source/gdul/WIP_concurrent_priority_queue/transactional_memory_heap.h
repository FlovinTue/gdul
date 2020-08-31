#pragma once


#include <functional>
#include <cassert>
#include <utility>
#include <vector>
#include <immintrin.h>

namespace gdul {

template <class Key, class Value, class Comparator = std::less<const Key&>>
class heap
{
public:
	heap();

	using size_type = std::size_t;
	using key_type = Key;
	using value_type = Value;
	using compare_type = Comparator;

	size_type size() const;

	void push(const std::pair<key_type, value_type>& item);
	bool try_pop(std::pair<key_type, value_type>& out);



private:
	void touch_cachelines();
	void trickle(size_type aIndex);
	void bubble(size_type aIndex);

	struct alignas(std::hardware_destructive_interference_size) cache_padded_item {

		// Value uninteresting at this time
		//value_type m_value;

		key_type m_key;
	};

	std::vector<cache_padded_item> m_items;

	Comparator m_comparator;
};
template<class Key, class Value, class Comparator>
heap<Key, Value, Comparator>::heap() :
	m_items(0),
	m_comparator(Comparator())
{
	m_items.reserve(128);
}
template<class Key, class Value, class Comparator>
inline typename heap<Key, Value, Comparator>::size_type heap<Key, Value, Comparator>::size() const
{
	return m_items.size();
}
template<class Key, class Value, class Comparator>
inline void heap<Key, Value, Comparator>::push(const std::pair<key_type, value_type>& item)
{
	for (bool test = false; !test;) {

		touch_cachelines();

		while (_xbegin() != _XBEGIN_STARTED);

		m_items.resize(m_items.size() + 1);
		m_items[m_items.size() - 1].m_key = item.first;

		bubble(m_items.size() - 1);

		test = true;
		_xend();
	}
}
template<class Key, class Value, class Comparator>
inline bool heap<Key, Value, Comparator>::try_pop(std::pair<key_type, value_type>& out)
{
	std::pair<key_type, value_type> item;

	for (bool test = false; !test;) {

		touch_cachelines();

		while (_xbegin() != _XBEGIN_STARTED);

		if (!m_items.size()) {
			_xabort(_XABORT_EXPLICIT);
		
			return false;
		}

		item.first = m_items[0].m_key;
		
		m_items[0].m_key = m_items[m_items.size() - 1].m_key;
		m_items.resize(m_items.size() - 1);

		trickle(0);

		test = true;

		_xend();
	}

	out = item;

	return true;
}
template<class Key, class Value, class Comparator>
inline void heap<Key, Value, Comparator>::touch_cachelines()
{
	_mm_prefetch((const char*)this, _MM_HINT_T1);
	for (std::size_t i = 0; i < m_items.size(); ++i) {
		_mm_prefetch((const char*)&m_items[i], _MM_HINT_T1);
	}
}
template<class Key, class Value, class Comparator>
inline void heap<Key, Value, Comparator>::trickle(size_type aIndex)
{
	size_type index(aIndex);
	size_type left(index * 2 + 1);
	size_type right(index * 2 + 2);
	size_type next(0);

	while (index < m_items.size())
	{
		next = index;

		if (right < m_items.size())
			if (m_comparator(m_items[right].m_key, m_items[next].m_key))
				next = right;
		if (left < m_items.size())
			if (m_comparator(m_items[left].m_key, m_items[next].m_key))
				next = left;

		if (next == index)
			break;

		std::swap(m_items[index].m_key, m_items[next].m_key);

		index = next;
		left = index * 2 + 1;
		right = index * 2 + 2;
	}
}
template<class Key, class Value, class Comparator>
inline void heap<Key, Value, Comparator>::bubble(size_type aIndex)
{
	size_type index(aIndex);
	size_type parent((index - 1) / 2);

	for (;;)
	{
		if (index == 0)
			break;

		if (m_comparator(m_items[parent].m_key, m_items[index].m_key))
			break;

		std::swap(m_items[parent].m_key, m_items[index].m_key);
		index = parent;
		parent = (index - 1) / 2;
	}
}
}