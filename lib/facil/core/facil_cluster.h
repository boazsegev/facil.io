#ifndef H_FACIL_CLUSTER_H
/* *****************************************************************************
 * Cluster Messages API
 *
 * Facil supports a message oriented API for use for Inter Process Communication
 * (IPC), publish/subscribe patterns, horizontal scaling and similar use-cases.
 **************************************************************************** */
#define H_FACIL_CLUSTER_H

#include "facil.h"

/* *****************************************************************************
 * Types
 **************************************************************************** */

/** This contains message metadata, set by message extensions. */
typedef struct facil_msg_metadata_s {
  size_t type_id;
  struct facil_msg_metadata_s *next;
} facil_msg_metadata_s;

/** Message structure, with an integer filter as well as a channel filter. */
typedef struct facil_msg_s {
  /** A unique message type. Negative values are reserved, 0 == pub/sub. */
  int32_t filter;
  /** A channel name, allowing for pub/sub patterns. */
  FIOBJ channel;
  /** The actual message. */
  FIOBJ msg;
  /** Metadata can be set by message extensions. */
  facil_msg_metadata_s *meta;
} facil_msg_s;

/**
 * Pattern matching callback type - should return 0 unless channel matches
 * pattern.
 */
typedef int (*facil_match_fn)(FIOBJ pattern, FIOBJ channel);

/**
 * Signals all workers to shutdown, which might invoke a respawning of the
 * workers unless the shutdown signal was received.
 *
 * NOT signal safe.
 */
void facil_cluster_signal_children(void);

/* *****************************************************************************
 * Initialization
 **************************************************************************** */

// typedef struct {
// } facil_msg_s;

// typedef struct {
// } facil_msg_s;

/**
Sets a callback / handler for a message of type `msg_type`.

Callbacks are invoked using an O(n) matching, where `n` is the number of
registered callbacks.

The `msg_type` value can be any positive number up to 2^31-1 (2,147,483,647).
All values less than 0 are reserved for internal use.
*/

#endif
