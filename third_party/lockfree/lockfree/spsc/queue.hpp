/**************************************************************
 * @file queue.hpp
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
 * Version:         v2.0.9
 **************************************************************/

/************************** INCLUDE ***************************/
#ifndef LOCKFREE_QUEUE_HPP
#define LOCKFREE_QUEUE_HPP

#include <atomic>
#include <cstddef>
#include <type_traits>

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#include <optional>
#endif

namespace lockfree {
namespace spsc {
/*************************** TYPES ****************************/

template <typename T, size_t size> class Queue {
    static_assert(std::is_trivial<T>::value, "The type T must be trivial");
    static_assert(size > 2, "Buffer size must be bigger than 2");

    /********************** PUBLIC METHODS ************************/
  public:
    Queue();

    /**
     * @brief Adds an element into the queue.
     * Should only be called from the producer thread.
     * @param[in] element
     * @retval Operation success
     */
    bool Push(const T &element);

    /**
     * @brief Removes an element from the queue.
     * Should only be called from the consumer thread.
     * @param[out] element
     * @retval Operation success
     */
    bool Pop(T &element);

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
    /**
     * @brief Removes an element from the queue.
     * Should only be called from the consumer thread.
     * @retval Either the element or nothing
     */
    std::optional<T> PopOptional();
#endif

    /********************** PRIVATE MEMBERS ***********************/
  private:
    T _data[size]; /**< Data array */
#if LOCKFREE_CACHE_COHERENT
    alignas(LOCKFREE_CACHELINE_LENGTH) std::atomic_size_t _r; /**< Read index */
    alignas(
        LOCKFREE_CACHELINE_LENGTH) std::atomic_size_t _w; /**< Write index */
#else
    std::atomic_size_t _r; /**< Read index */
    std::atomic_size_t _w; /**< Write index */
#endif
};

} /* namespace spsc */
} /* namespace lockfree */

/************************** INCLUDE ***************************/

/* Include the implementation */
#include "queue_impl.hpp"

#endif /* LOCKFREE_QUEUE_HPP */
