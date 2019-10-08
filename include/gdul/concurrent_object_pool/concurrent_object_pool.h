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

template <class Object>
class concurrent_object_pool
{
public:
	concurrent_object_pool(std::size_t blockSize);
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

	concurrent_queue<Object*> myUnusedObjects;

	std::atomic<block_node*> myLastBlock;

	const std::size_t myBlockSize;
};

template<class Object>
inline concurrent_object_pool<Object>::concurrent_object_pool(std::size_t blockSize)
	: myBlockSize(blockSize)
	, myUnusedObjects(blockSize)
	, myLastBlock(nullptr)
{
	try_alloc_block();
}
template<class Object>
inline concurrent_object_pool<Object>::~concurrent_object_pool()
{
	unsafe_destroy();
}
template<class Object>
inline Object * concurrent_object_pool<Object>::get_object()
{
	Object* out;

	while (!myUnusedObjects.try_pop(out)) {
		try_alloc_block();
	}
	return out;
}
template<class Object>
inline void concurrent_object_pool<Object>::recycle_object(Object * object)
{
	myUnusedObjects.push(object);
}
template<class Object>
inline std::size_t concurrent_object_pool<Object>::avaliable()
{
	return static_cast<uint32_t>(myUnusedObjects.size());
}
template<class Object>
inline void concurrent_object_pool<Object>::unsafe_destroy()
{
	block_node* blockNode(myLastBlock.load(std::memory_order_relaxed));
	while (blockNode) {
		block_node* const previous(blockNode->myPrevious);

		delete[] blockNode->myBlock;
		delete blockNode;

		blockNode = previous;
	}
	myLastBlock = nullptr;

	myUnusedObjects.unsafe_reset();
}
template<class Object>
inline void concurrent_object_pool<Object>::try_alloc_block()
{
	block_node* expected(myLastBlock.load(std::memory_order_relaxed));

	if (myUnusedObjects.size()) {
		return;
	}

	Object* const block(new Object[myBlockSize]);

	block_node* const desired(new block_node);
	desired->myPrevious = expected;
	desired->myBlock = block;

	if (!myLastBlock.compare_exchange_strong(expected, desired)) {
		delete desired;
		delete[] block;

		return;
	}

	for (std::size_t i = 0; i < myBlockSize; ++i) {
		myUnusedObjects.push(&block[i]);
	}
}
}