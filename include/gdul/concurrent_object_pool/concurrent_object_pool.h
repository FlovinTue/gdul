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

#include <assert.h>
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <atomic>

#undef get_object

namespace gdul {

template <class Object, class Allocator = std::allocator<std::uint8_t>>
class concurrent_object_pool
{
public:

	concurrent_object_pool(std::size_t blockSize);
	concurrent_object_pool(std::size_t blockSize, Allocator& allocator);
	~concurrent_object_pool();

	inline Object* get_object();
	inline void recycle_object(Object* object);

	inline std::size_t avaliable();

	inline void unsafe_destroy();

private:
	void try_alloc_block();

	struct block_node
	{
		Object* myBlock;
		std::atomic<block_node*> myPrevious;
	};

	using node_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<block_node>;

	Allocator myAllocator;

	concurrent_queue<Object*, Allocator> myUnusedObjects;

	std::atomic<block_node*> myLastBlock;

	const std::size_t myBlockSize;
};

template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(std::size_t blockSize)
	: myBlockSize(blockSize)
	, myUnusedObjects(blockSize, myAllocator)
	, myLastBlock(nullptr)
{
	try_alloc_block();
}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(std::size_t blockSize, Allocator & allocator)
	: myAllocator(allocator)
	, myBlockSize(blockSize)
	, myUnusedObjects(blockSize, allocator)
	, myLastBlock(nullptr)
{
	try_alloc_block();
}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::~concurrent_object_pool()
{
	unsafe_destroy();
}
template<class Object, class Allocator>
inline Object * concurrent_object_pool<Object, Allocator>::get_object()
{
	Object* out;

	while (!myUnusedObjects.try_pop(out)) {
		try_alloc_block();
	}
	return out;
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::recycle_object(Object * object)
{
	myUnusedObjects.push(object);
}
template<class Object, class Allocator>
inline std::size_t concurrent_object_pool<Object, Allocator>::avaliable()
{
	return static_cast<std::uint32_t>(myUnusedObjects.size());
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::unsafe_destroy()
{
	block_node* blockNode(myLastBlock.load(std::memory_order_acquire));
	while (blockNode) {
		block_node* const previous(blockNode->myPrevious);

		for (std::size_t i = 0; i < myBlockSize; ++i) {
			blockNode->myBlock->~Object();
		}
		myAllocator.deallocate(reinterpret_cast<std::uint8_t*>(blockNode->myBlock), myBlockSize * sizeof(Object));

		blockNode->~block_node();
		myAllocator.deallocate(reinterpret_cast<std::uint8_t*>(blockNode), sizeof(block_node));

		blockNode = previous;
	}
	myLastBlock = nullptr;

	myUnusedObjects.unsafe_reset();
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::try_alloc_block()
{
	block_node* expected(myLastBlock.load(std::memory_order_acquire));

	if (myUnusedObjects.size()) {
		return;
	}

	Object* const block(reinterpret_cast<Object*>(myAllocator.allocate(myBlockSize * sizeof(Object))));

	for (std::size_t i = 0; i < myBlockSize; ++i) {
		new (&block[i]) (Object);
	}

	block_node* const desired(reinterpret_cast<block_node*>(myAllocator.allocate(sizeof(block_node))));
	new (desired)(block_node);
	desired->myPrevious = expected;
	desired->myBlock = block;

	if (!myLastBlock.compare_exchange_strong(expected, desired)) {
		for (std::size_t i = 0; i < myBlockSize; ++i) {
			block->~Object();
		}
		myAllocator.deallocate(reinterpret_cast<std::uint8_t*>(block), myBlockSize * sizeof(Object));

		desired->~block_node();
		myAllocator.deallocate(reinterpret_cast<std::uint8_t*>(desired), sizeof(block_node));

		return;
	}

	for (std::size_t i = 0; i < myBlockSize; ++i) {
		myUnusedObjects.push(&block[i]);
	}
}
}