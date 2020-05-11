// Copyright(c) 2020 Flovin Michaelsen
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
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/concurrent_object_pool/concurrent_object_pool.h>
#include <vector>
#include <random>

#ifndef MAKE_UNIQUE_NAME 
#define CONCAT(a,b)  a##b
#define EXPAND_AND_CONCAT(a, b) CONCAT(a,b)
#define MAKE_UNIQUE_NAME(prefix) EXPAND_AND_CONCAT(prefix, __COUNTER__)
#endif

#define GDUL_CPQ_PADDING(bytes) const std::uint8_t MAKE_UNIQUE_NAME(padding)[bytes] {}

#pragma warning(disable:4505)

namespace gdul
{
namespace cpq_detail
{
using size_type = std::size_t;

}

template <class KeyType, class ValueType, class Allocator = std::allocator<std::uint8_t>>
class concurrent_priority_queue
{
public:
	using allocator_type = Allocator;
	using key_type = KeyType;
	using value_type = ValueType;
	using size_type = cpq_detail::size_type;
	using comparator_type = std::less<KeyType>;

	concurrent_priority_queue();

	/// <summary>
	/// Insert an item in to the queue
	/// </summary>
	/// <param name="item">Key value pair to insert</param>
	void insert(const std::pair<key_type, value_type>& item);

	/// <summary>
	/// Attempt to retrieve the first item
	/// </summary>
	/// <param name="out">The out value item</param>
	/// <returns>Indicates if the operation was successful</returns>
	bool try_pop(value_type& out);
	static constexpr size_type Max_Node_Height = cpq_detail::log2ceil(ExpectedEntriesHint);
private:

	struct node
	{
		node()
			: m_kvPair()
		{}
		std::pair<key_type, value_type> m_kvPair;
	};

	concurrent_object_pool<node> m_itemPool;

	std::vector<node*> m_items;

	comparator_type m_comparator;
};
template <class KeyType, class ValueType, class Allocator>
concurrent_priority_queue<KeyType, ValueType, Allocator>::concurrent_priority_queue()
	: m_comparator()
{
	m_items.resize()
}
template <class KeyType, class ValueType, class Allocator>
void concurrent_priority_queue<KeyType, ValueType, Allocator>::insert(const std::pair<key_type, value_type>& item)
{
	// 

	// Begin with a subtree local lock. Somehow. 
	// This would mean independent operations could be in progress in different subtrees
	// ...

}
template <class KeyType, class ValueType, class Allocator>
bool concurrent_priority_queue<KeyType, ValueType, Allocator>::try_pop(value_type& out)
{
	// Perhaps keep thread local item arrays. Reconfigure locally and compare exchange the new configuration
	// Could be interesting, really.

}
namespace cpq_detail
{


}