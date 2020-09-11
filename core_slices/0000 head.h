/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
********************************************************************************

********************************************************************************
NOTE: this file is auto-generated from: https://github.com/facil-io/io-core
***************************************************************************** */
#ifndef H_FACIL_IO_H
#define H_FACIL_IO_H

/* *****************************************************************************
 * Table of contents (find by subject):
 * =================
 * Helper and compile time settings (MACROs)
 *
 * Connection Callback (Protocol) Management
 * Listening to Incoming Connections
 * Connecting to remote servers as a client
 * Starting the IO reactor and reviewing it's state
 * Socket / Connection Functions
 * Connection Read / Write Hooks, for overriding the system calls
 * Concurrency overridable functions
 * Connection Task scheduling
 * Event / Task scheduling
 * Startup / State Callbacks (fork, start up, idle, etc')
 * TLS Support (weak functions, to bea overriden by library wrapper)
 * Lower Level API - for special circumstances, use with care under
 *
 * Pub/Sub / Cluster Messages API
 * Cluster Messages and Pub/Sub
 * Cluster / Pub/Sub Middleware and Extensions ("Engines")
 *
 * SipHash
 * SHA-1
 * SHA-2
 *
 *
 *
 * Quick Overview
 * ==============
 *
 * The core IO library follows an evented design and uses callbacks for IO
 * events. Using the API described in the Connection Callback (Protocol)
 * Management section:
 *
 * - Each connection / socket, is identified by a process unique number
 *   (`uuid`).
 *
 * - Connections are assigned protocol objects (`fio_protocol_s`) using the
 *   `fio_attach` function.
 *
 * - The callbacks in the protocol object are called whenever an IO event
 *   occurs.
 *
 * - Callbacks are protected using one of two connection bound locks -
 *   `FIO_PR_LOCK_TASK` for most tasks and `FIO_PR_LOCK_WRITE` for `on_ready`
 *   and `on_timeout` tasks.
 *
 * - User data is assumed to be stored in the protocol object using C style
 *   inheritance.
 *
 * Reading and writing operations use an internal user-land buffer and they
 * never fail... unless, the client is so slow that they appear to be attacking
 * the network layer (slowloris), the connection was lost due to other reasons
 * or the system is out of memory.
 *
 * Because the framework is evented, there's API that offers easy event and task
 * scheduling, including timers etc'. Also, connection events can be
 * rescheduled, allowing connections to behave like state-machines.
 *
 * The core library includes Pub/Sub (publish / subscribe) services which offer
 * easy IPC (inter process communication) in a network friendly API. Pub/Sub
 * services can be extended to synchronize with external databases such as
 * Redis.
 *
 **************************************************************************** */
