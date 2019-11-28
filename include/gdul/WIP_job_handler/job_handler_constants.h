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

namespace gdul{
namespace jh_detail{

constexpr std::uint32_t Job_Max_Dependencies = std::numeric_limits<std::uint32_t>::max() / 2;

// The number of internal job queues. 
constexpr std::uint8_t Priority_Granularity = 4;
constexpr std::uint8_t Default_Job_Priority = 0;
constexpr std::uint8_t Callable_Max_Size_No_Heap_Alloc = 24;
constexpr std::uint16_t Job_Handler_Max_Workers = 32;

constexpr std::uint8_t Worker_Auto_Affinity = std::numeric_limits<std::uint8_t>::max();

// The number of job chunks that the Job_Impl block allocator will allocate
// when empty
constexpr std::size_t Job_Impl_Allocator_Block_Size = 128;

using allocator_type = std::allocator<uint8_t>;

}
}