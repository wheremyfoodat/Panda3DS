# lockfree
![CMake](https://github.com/DNedic/lockfree/actions/workflows/.github/workflows/cmake.yml/badge.svg)

`lockfree` is a collection of lock-free data structures written in standard C++11 and suitable for all platforms - from deeply embedded to HPC.

## What are lock-free data structures?

Lock-free data structures are data structures that are thread and interrupt safe for concurrent use without having to use mutual exclusion mechanisms. They are most useful for inter process communication, and often scale much better than lock-based structures with the number of operations and threads.

## Why use `lockfree`
* Written in standard C++11, compatible with all platforms supporting it
* All data structures are thread and interrupt safe in their respective usecases
* No dynamic allocation
* Optimized for high performance
* MIT Licensed
* Additional APIs for newer C++ versions

## What data structures are available?
### Single-producer single-consumer data structures
* [Queue](docs/spsc/queue.md) - Best for single element operations, extremely fast, simple API consisting of only 2 methods.
* [Ring Buffer](docs/spsc/ring_buf.md) - A more general data structure with the ability to handle multiple elements at a time, uses standard library copies making it very fast for bulk operations.
* [Bipartite Buffer](docs/spsc/bipartite_buf.md) - A variation of the ring buffer with the ability to always provide linear space in the buffer, enables in-buffer processing.
* [Priority Queue](docs/spsc/priority_queue.md) - A Variation of the queue with the ability to provide different priorities for elements, very useful for things like signals, events and communication packets.

These data structures are more performant and should generally be used whenever there is only one thread/interrupt pushing data and another one retrieving it.

### Multi-producer multi-consumer data structures
* [Queue](docs/mpmc/queue.md) - Best for single element operations, extremely fast, simple API consisting of only 2 methods.
* [Priority Queue](docs/mpmc/priority_queue.md) - A Variation of the queue with the ability to provide different priorities for elements, very useful for things like signals, events and communication packets.

These data structures are more general, supporting multiple producers and consumers at the same time, however they have storage and performance overhead compared to single producer single consumer data structures. They also require atomic instructions which can be missing from some low-end microcontrollers.

## How to get
There are three main ways to get the library:
* Using CMake [FetchContent()](https://cmake.org/cmake/help/latest/module/FetchContent.html)
* As a [git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
* By downloading a release from GitHub

## Configuration
`lockfree` uses cacheline alignment for indexes to avoid the [False Sharing](https://en.wikipedia.org/wiki/False_sharing) phenomenon by default, avoiding the performance loss of cacheline invalidation  on cache coherent systems.  This aligns the indexes to ```LOCKFREE_CACHELINE_LENGTH```, ```64``` by default.

On embedded systems, ```LOCKFREE_CACHE_COHERENT``` should almost always be set as ```false``` to avoid wasting memory.

Additionally, some systems have a non-typical cacheline length (for instance the apple M1/M2 CPUs have a cacheline length of 128 bytes), and ```LOCKFREE_CACHELINE_LENGTH``` should be set accordingly in those cases.

## Known limitations
All of the data structures in `lockfree` are only meant to be used for [trivial](https://en.cppreference.com/w/cpp/language/classes#Trivial_class) types.

## FAQ
### Why would I use this over locking data structures on a hosted machine?

The biggest reason you would want to use a lock-free data structure on hosted environments would be avoiding issues surrounding locking such as deadlocks, priority inversion and nondeterministic access latency. When used properly, lock-free data structures can also improve performance in some scenarios.

Additionally, `lockfree` provides a way to build applications and libraries that can be compiled to work on both POSIX and non-POSIX environments without `#ifdef`s or polymorphism.

### Why use this over RTOS-provided IPC mechanisms on an embedded system?

While locking usually isn't expensive on embedded systems such as microcontrollers, there is a wide variety of RTOS-es and no standardized API for locking. The fact that multiple architectures are present from 8051 to RISC-V means that interrupt management methods are not standardized either.

`lockfree` provides a way to build portable embedded code with a negligible performance cost as opposed to locking, code using `lockfree` can be compiled to run on any embedded platform supporting C++11. Additionally, the code can easily be tested on a host machine without the need for mocking.

### What advantages does using C++ over C provide for the library?
* Type safety, as data structures are type and size templated
* Much simpler and less error-prone instantiation
* Higher performance due to compile-time known size and header-only implementation
* Encapsulation, the data buffer is a class member instead of being passed by a pointer

### What is the formal classification of the data structures in `lockfree`?
All structures in `lockfree` are **bounded**, **array-based** and **lock-free**, spsc data structures are also **waitfree** and **termination safe**. 

## Theory and references
For more insight into lock-free programming, take a look at:
* This [brilliant talk series](https://youtu.be/c1gO9aB9nbs) from Herb Sutter
* [Live Lock-Free or Deadlock](https://youtu.be/lVBvHbJsg5Y) talk series from Fedor Pikus
* Dmitry Vyukov's excellent [blog](https://www.1024cores.net/home/lock-free-algorithms/introduction)
