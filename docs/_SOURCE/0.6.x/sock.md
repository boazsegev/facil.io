---
title: facil.io - lib sock - a making sockets in C easy to use.
sidebar: 0.6.x/_sidebar.md
---
# A Simple Socket Library for non-blocking Sockets

This documentation is incomplete. I would love your help to finish it up. Until that time, please read the documentation in [the `sock.h` header file](https://github.com/boazsegev/facil.io/blob/master/lib/facil/core/sock.h).

## Overview

The `sock` library was born to solve [many concerns](sock_why) that pop up when using the system sockets API directly.

On systems that have unreasonably high open file limits, the sock library will artificially limit the open socket count to a reasonably high default that can be adjusted during compile time using the `LIB_SOCK_MAX_CAPACITY` macro (currently, 131,072 open files).

It supports TCP/IP sockets as well as unix doamin sockets. Pipes can also be attached to the library.

The `sock.h` API can be divided into a few different categories:

- Limits

- General helper functions

- Accepting connections and opening new sockets.

- Sending and receiving data.

- Read/Write Hooks.

### example

## Constants

## Types

## General Functions

## Socket Initialization and State

## Sending / Receiving Data

## Read/Write Hooks

## Important Notes
