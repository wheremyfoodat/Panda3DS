/**************************************************************
 * @file queue_impl.hpp
 * @brief A queue implementation written in standard c++11
 * suitable for both low-end microcontrollers all the way
 * to HPC machines. Lock-free for single consumer single
 * producer scenarios.
 **************************************************************/

/**************************************************************
 * Copyright (c) 2023 Djordje Nedic
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall
 * be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of lockfree
 *
 * Author:          Djordje Nedic <nedic.djordje2@gmail.com>
 * Version:         v2.0.8
 **************************************************************/

namespace lockfree {
namespace spsc {
/********************** PUBLIC METHODS ************************/

template <typename T, size_t size> Queue<T, size>::Queue() : _r(0U), _w(0U) {}

template <typename T, size_t size> bool Queue<T, size>::Push(const T &element) {
    /*
       The full check needs to be performed using the next write index not to
       miss the case when the read index wrapped and write index is at the end
     */
    const size_t w = _w.load(std::memory_order_relaxed);
    size_t w_next = w + 1;
    if (w_next == size) {
        w_next = 0U;
    }

    /* Full check  */
    const size_t r = _r.load(std::memory_order_acquire);
    if (w_next == r) {
        return false;
    }

    /* Place the element */
    _data[w] = element;

    /* Store the next write index */
    _w.store(w_next, std::memory_order_release);
    return true;
}

template <typename T, size_t size> bool Queue<T, size>::Pop(T &element) {
    /* Preload indexes with adequate memory ordering */
    size_t r = _r.load(std::memory_order_relaxed);
    const size_t w = _w.load(std::memory_order_acquire);

    /* Empty check */
    if (r == w) {
        return false;
    }

    /* Remove the element */
    element = _data[r];

    /* Increment the read index */
    r++;
    if (r == size) {
        r = 0U;
    }

    /* Store the read index */
    _r.store(r, std::memory_order_release);
    return true;
}

/********************* std::optional API **********************/
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
template <typename T, size_t size>
std::optional<T> Queue<T, size>::PopOptional() {
    T element;
    bool result = Pop(element);

    if (result) {
        return element;
    } else {
        return {};
    }
}
#endif

} /* namespace spsc */
} /* namespace lockfree */
