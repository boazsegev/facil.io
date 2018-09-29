---
title: facil.io - Fast Concurrent Memory Allocator
sidebar: 0.6.x/_sidebar.md
---
# {{{title}}}

The system's memory allocator is usually highly optimized and very performant. In fact, for most cases, it should probably be preferred.

However, the system's memory allocator is optimized for generic unknown use-cases which means that sometimes a custom memory allocator will perform better for a subset of use-cases.

facil.io includes a memory allocator that was optimized for small (less than a page) concurrent group allocations and deallocations (calling `malloc` a bunch of times and than calling `free` a bunch of times from many different threads) with minimal or no reallocations (minimal use of `realloc`).

The `fio_mem.h` allocator will minimize overhead and lock contention, allowing for better concurrency.

## Overview

The `fio_mem.h` allocator is used the same way the system allocator is used, prefixing it's functions with `fio_` - so the `fio_malloc`, `fio_free`, `fio_realloc` and `fio_calloc` take the same parameters as `malloc`, `free`, `realloc` and `calloc` respectively.

Allocated memory is always zeroed out and aligned on a 16 byte boundary. When possible, consecutive calls to `fio_malloc` will return with a close physical memory address to the previous allocation, resulting in improved locality.

The memory allocator assumes multiple concurrent allocation/deallocation, short life spans (memory is freed shortly after it was allocated) and small allocations (`fio_realloc` almost always copies data).

These assumptions allow the allocator to ignore fragmentation within a memory "block", waiting for the whole "block" to be freed before it's memory is recycled.

This allocator should NOT be used for objects with a long life-span, because even a single persistent object will prevent the re-use of the whole memory block (128Kb by default) from which it was allocated.

This documentation is incomplete. I would love your help to finish it up. Until that time, please read the documentation in [the `fio_mem.h` header file](https://github.com/boazsegev/facil.io/blob/master/lib/facil/core/types/fiobj/fio_mem.h).

### example

## Constants



## Types

## Functions

## Important Notes
