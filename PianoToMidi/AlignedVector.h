#pragma once

template<typename T>
using AlignedVector = std::vector<T, boost::alignment::aligned_allocator<T, 64>>;