---
title: facil.io - 0.8.x C STL - a Simple Template Library for C
sidebar: 0.8.x/_sidebar.md
---
# {{{title}}}

## Lower Level API Notice

>> **The core library is probably not the API most developers need to focus on** (although it's always good to know).
>>
>> This API is used to power the higher level API offered by the [HTTP / WebSockts extension](./http) and the [dynamic FIOBJ types](./fiobj).

## Overview

The core library is a single file library (`fio-stl.h`).

The core library includes a Simple Template Library for common types, such as:

* Linked Lists - defined by `FIO_LIST_NAME`

* Dynamic Arrays - defined by `FIO_ARY_NAME`

* Hash Maps / Sets - defined by `FIO_MAP_NAME`

* Binary Safe Dynamic Strings - defined by `FIO_STR_NAME`

* Reference counting / Type wrapper - defined by `FIO_REF_NAME`

In addition, the core library includes helpers for common tasks, such as:

* Logging and Assertion (without heap allocation) - defined by `FIO_LOG_LENGTH_LIMIT`

* Atomic operations - defined by `FIO_ATOMIC`

* Bit-Byte Operations - defined by `FIO_BITWISE` and `FIO_BITMAP`

* Network byte ordering macros - defined by `FIO_NTOL`

* Data Hashing (using Risky Hash) - defined by `FIO_RISKY_HASH`

* Psedo Random Generation - defined by `FIO_RAND`

* String / Number conversion - defined by `FIO_ATOL`

* Command Line Interface helpers - defined by `FIO_CLI`

* Custom Memory Allocation - defined by `FIO_MALLOC`

## Linked Lists
