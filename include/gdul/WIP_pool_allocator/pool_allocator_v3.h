#pragma once

// Copyright(c) 2019 Flovin Michaelsen
// 
// Permission is hereby granted, free of chargeo any person obtaining a copy
// of this software and associated documentation files(the "Software")o deal
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
// LIABILITY, WHETHER IN AN ACTION OF CONTRACTORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <stdint.h>
#include <gdul/concurrent_object_pool/concurrent_object_pool_v2.h>

namespace gdul
{
namespace call_detail {
class memory_pool_base
{
public:
	virtual bool verify_compatibility(std::size_t chunkSize, std::size_t chunkAlign) const = 0;

	virtual void* get_chunk() = 0;
	virtual void recycle_chunk(void* chunk) = 0;
};

template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
class memory_pool_impl;
}

// Allocator associated with a pool instance. Is only able to request memory chunks of the 
// size that the pool is initialized with (but may do so quickly)
template <class T>
class pool_allocator
{
public:
	using value_type = T;

	// raw_ptr and shared_ptr may be used here, based on if an allocator
	// is allowed to outlive its pool or not
	using pool_ptr_type = raw_ptr<call_detail::memory_pool_base>;

	template <typename U>
	struct rebind
	{
		using other = pool_allocator<U>;
	};

	template <class U>
	pool_allocator(const pool_allocator<U>& other);
	template <class U>
	pool_allocator(pool_allocator<U>&& other);

	template <class U>
	pool_allocator& operator=(const pool_allocator<U>& other);
	template <class U>
	pool_allocator& operator=(pool_allocator<U>&& other);

	value_type* allocate(std::size_t = 1);
	void deallocate(value_type* chunk, std::size_t = 1);

	template <class U>
	pool_allocator<U> convert() const;

	pool_allocator(const pool_ptr_type& memoryPool);
private:

	friend class memory_pool;

	pool_ptr_type m_pool;
};

template<class T>
inline pool_allocator<T>::pool_allocator(const pool_ptr_type& memoryPool)
	: m_pool(memoryPool)
{}
template<class T>
template<class U>
inline pool_allocator<T>::pool_allocator(const pool_allocator<U>& other)
{
	operator=(other);
}
template<class T>
template<class U>
inline pool_allocator<T>::pool_allocator(pool_allocator<U>&& other)
{
	operator=(std::move(other));
}
template<class T>
template <class U>
inline pool_allocator<T>& pool_allocator<T>::operator=(const pool_allocator<U>& other)
{
	m_pool = std::move(other.convert<T>().m_pool);
	return *this;
}
template<class T>
template <class U>
inline pool_allocator<T>& pool_allocator<T>::operator=(pool_allocator<U>&& other)
{
	operator=(other);
	return *this;
}
template<class T>
template<class U>
inline pool_allocator<U> pool_allocator<T>::convert() const
{
	assert(m_pool->verify_compatibility(sizeof(U), alignof(U)) && "Memory pool stats incompatible with type");
	return pool_allocator<U>(m_pool);
}
template<class T>
inline typename pool_allocator<T>::value_type* pool_allocator<T>::allocate(std::size_t)
{
	return (value_type*)m_pool->get_chunk();
}
template<class T>
inline void pool_allocator<T>::deallocate(value_type* chunk, std::size_t)
{
	m_pool->recycle_chunk((void*)chunk);
}

class memory_pool
{
public:
	memory_pool() = default;

	template <std::size_t ChunkSize, std::size_t ChunkAlign>
	void init(std::size_t initChunkCount);

	template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
	void init(std::size_t initChunkCount, ParentAllocator allocator);

	void reset();

	template <class T>
	pool_allocator<T> create_allocator() const;

private:
	shared_ptr<call_detail::memory_pool_base> m_impl;
};
template<std::size_t ChunkSize, std::size_t ChunkAlign>
inline void memory_pool::init(std::size_t initChunkCount)
{
	init<ChunkSize, ChunkAlign>(initChunkCount, std::allocator<std::uint8_t>());
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void memory_pool::init(std::size_t initChunkCount, ParentAllocator allocator)
{
	if (m_impl)
		return;

	m_impl = gdul::allocate_shared<call_detail::memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>>(allocator, initChunkCount, allocator);
}
inline void memory_pool::reset()
{
	m_impl = shared_ptr<call_detail::memory_pool_base>();
}
template<class T>
inline pool_allocator<T> memory_pool::create_allocator() const
{
	assert(m_impl->verify_compatibility(sizeof(T), alignof(T)) && "Memory pool stats incompatible with type");
	return pool_allocator<T>((typename pool_allocator<T>::pool_ptr_type)m_impl);
}
namespace call_detail {

template <std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
class memory_pool_impl : public memory_pool_base
{
	struct alignas(ChunkAlign) chunk_rep
	{
		std::uint8_t m_block[ChunkSize]{};
		std::atomic_bool m_claim = false;
	};

	struct block_rep
	{
		shared_ptr<chunk_rep[]> m_chunks;
		chunk_rep* m_begin = nullptr;
		chunk_rep* m_end = nullptr;
		std::atomic_uint m_accessIndex = 0;
	};

public:
	memory_pool_impl(std::size_t initChunkCount, ParentAllocator allocator);

	void* get_chunk() override final;
	void recycle_chunk(void* chunk) override final;

	bool verify_compatibility(std::size_t chunkSize, std::size_t chunkAlign) const override final {
		constexpr std::size_t maxChunkSize(ChunkSize);
		constexpr std::size_t maxChunkAlign(ChunkAlign);
		const bool size(!(maxChunkSize < chunkSize));
		const bool align(!(maxChunkAlign < chunkAlign));
		return size & align;
	}


private:
	void try_allocate_block(std::size_t chunkCount);
	void refresh_tlocal();
	void grow_block();
	std::size_t to_chunk_index(void* chunk, const chunk_rep* begin);

	static struct dummy_container
	{
		shared_ptr<block_rep> m_block;

		template <class T>
		class dummy_allocator
		{
		public:
			using value_type = T;
			template <class U>
			dummy_allocator(const dummy_allocator<U>& other) : m_mem(other.m_mem) {}
			dummy_allocator(void* mem) : m_mem(mem) {};
			T* allocate(std::size_t) { return (T*)m_mem; }
			void deallocate(T*, std::size_t) {};

			void* m_mem;
		};

		static constexpr std::size_t Dummy_Size = allocate_shared_size<block_rep, dummy_allocator<block_rep>>();

		std::uint8_t m_dummyMem[Dummy_Size];

		dummy_container()
			: m_block(gdul::allocate_shared<block_rep>(dummy_allocator<block_rep>((void*)&m_dummyMem)))
		{}
	} s_dummy;

	atomic_shared_ptr<block_rep> m_block;

	struct retiree
	{
		shared_ptr<chunk_rep[]> m_block;
		chunk_rep* m_begin = nullptr;
		chunk_rep* m_end = nullptr;
		std::size_t m_allocatedCount = 0;
	};
	struct t_container
	{
		t_container() = default;
		t_container(const shared_ptr<block_rep>& init) 
			: m_block(init){}
		shared_ptr<block_rep> m_block;
		std::size_t m_allocatedCount = 0;
		std::vector<retiree> m_retirees;
	};
	tlm<t_container> t_block;

	ParentAllocator m_allocator;
};
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline  memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::memory_pool_impl(std::size_t initChunkCount, ParentAllocator allocator) 
	: t_block(s_dummy.m_block, allocator)
	, m_allocator(allocator)
{
	try_allocate_block(initChunkCount);
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::try_allocate_block(std::size_t chunkCount)
{
	shared_ptr<block_rep> expected(m_block.load());

	if (expected && !(expected->m_chunks.item_count() < chunkCount)) {
		refresh_tlocal();
		return;
	}

	shared_ptr<block_rep> desired(gdul::allocate_shared<block_rep>(m_allocator));
	desired->m_chunks = gdul::allocate_shared<chunk_rep[]>(chunkCount, m_allocator);
	desired->m_begin = desired->m_chunks.get();
	desired->m_end = desired->m_chunks.get() + chunkCount;

	if (m_block.compare_exchange_strong(expected, desired)){
		if (expected) {
			expected->m_begin = nullptr;
			expected->m_end = nullptr;
		}
	}
	refresh_tlocal();
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void* memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::get_chunk() 
{
	for (;;) {
		block_rep* const ref(t_block.get().m_block.get());

		if (!ref->m_begin) {
			refresh_tlocal();
		}
		else {
			const std::size_t itemCount(ref->m_chunks.item_count());
			const std::uint32_t first(ref->m_accessIndex++);

			for (std::uint32_t i = first; i < (first + itemCount); i = ref->m_accessIndex++) {
				const std::size_t localIndex((first + i) % itemCount);
				chunk_rep& chunk(ref->m_chunks[localIndex]);

				if (!chunk.m_claim.load(std::memory_order_relaxed)){
					if (!chunk.m_claim.exchange(true, std::memory_order_relaxed)) {
						++t_block.get().m_allocatedCount;
						return (void*)&chunk.m_block;
					}
				}
			}
			grow_block();
		}
	}
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::recycle_chunk(void* chunk)
{
	t_container& tlocal(t_block.get());
	
	if (!(chunk < tlocal.m_block->m_begin) &&
		 chunk < tlocal.m_block->m_end) {
		chunk_rep* const begin(tlocal.m_block->m_begin);
		const std::size_t chunkIndex(to_chunk_index(chunk, begin));
		begin[chunkIndex].m_claim.store(false, std::memory_order_relaxed);
		--tlocal.m_allocatedCount;
	}
	else {
		for(auto i = 0; i < tlocal.m_retirees.size(); ++i){
			retiree& reti(tlocal.m_retirees[i]);
			if (!(chunk < reti.m_begin) &&
				chunk < reti.m_end){
				chunk_rep* const begin(reti.m_begin);
				const std::size_t chunkIndex(to_chunk_index(chunk, begin));
				begin[chunkIndex].m_claim.store(false, std::memory_order_relaxed);
				--reti.m_allocatedCount;
				if (!reti.m_allocatedCount) {
					tlocal.m_retirees.erase(tlocal.m_retirees.begin() + i);
				}
				break;
			}
		}
	}

}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::refresh_tlocal()
{
	t_container& tlocal(t_block.get());

	if (tlocal.m_block == m_block)
		return;

	shared_ptr<block_rep> block(m_block.load());

	if (tlocal.m_allocatedCount) {
		retiree reti;
		reti.m_block = tlocal.m_block->m_chunks;
		reti.m_begin = reti.m_block.get();
		reti.m_end = reti.m_block.get() + reti.m_block.item_count();
		reti.m_allocatedCount = tlocal.m_allocatedCount;

		tlocal.m_retirees.emplace_back(std::move(reti));
	}

	tlocal.m_allocatedCount = 0;
	tlocal.m_block = std::move(block);
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline void memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::grow_block()
{
	const std::size_t from(t_block.get().m_block->m_chunks.item_count());

	try_allocate_block(from * 2);
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
inline std::size_t memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::to_chunk_index(void* chunk, const chunk_rep* begin)
{
	const std::uintptr_t from((std::uintptr_t)begin);
	const std::uintptr_t chunkAt((std::uintptr_t)chunk);
	const std::uintptr_t chunkLocal(chunkAt - from);

	return std::size_t(chunkLocal / sizeof(chunk_rep));
}
template<std::size_t ChunkSize, std::size_t ChunkAlign, class ParentAllocator>
typename memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::dummy_container memory_pool_impl<ChunkSize, ChunkAlign, ParentAllocator>::s_dummy;
}
}