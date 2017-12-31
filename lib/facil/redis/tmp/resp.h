/*
Copyright: Boaz segev, 2017
License: MIT except for any non-public-domain algorithms (none that I'm aware
of), which might be subject to their own licenses.

Feel free to copy, use and enjoy in accordance with to the license(s).
*/
#ifndef H_RESP_PARSER_H
/**
This is a neive implementation of the RESP protocol for Redis.
*/
#define H_RESP_PARSER_H

#include <stdint.h>
#include <stdlib.h>

/* support C++ */
#ifdef __cplusplus
extern "C" {
#endif

/** The RESP Parser Type */
typedef struct resp_parser_s *resp_parser_pt;

/* *****************************************************************************
RESP types and objects (Arrays, Strings & Integers)
***************************************************************************** */

enum resp_type_enum {
  /** A simple flag object object (`resp_object_s`) for NULL. */
  RESP_NULL = 0,
  /** A simple flag object object (`resp_object_s`) for OK. */
  RESP_OK = 1,
  /** A String object (`resp_string_s`) that indicates an error. */
  RESP_ERR = (2 + 8),
  /** A Number object object (`resp_number_s`). */
  RESP_NUMBER = 4,
  /** A String object (`resp_string_s`). */
  RESP_STRING = 8,
  /** An Array object object (`resp_array_s`). */
  RESP_ARRAY = 16,
  /**
   * A specific Array object object (`resp_array_s`) for Pub/Sub semantics.
   *
   * This is more of a hint than a decree, sometimes pubsub semantics are
   * misleading.
   */
  RESP_PUBSUB = (32 + 16),
};

/** a simple emtpy object type, used for RESP_NULL and RESP_OK */
typedef struct { enum resp_type_enum type; } resp_object_s;

/** The RESP_ARRAY and RESP_PUBSUB types */
typedef struct {
  enum resp_type_enum type;
  size_t len;
  size_t pos; /** allows simple iteration. */
  resp_object_s *array[];
} resp_array_s;

/** The RESP_STRING and RESP_ERR types */
typedef struct {
  enum resp_type_enum type;
  size_t len;
  uint8_t string[];
} resp_string_s;

/** The RESP_NUMBER type */
typedef struct {
  enum resp_type_enum type;
  int64_t number;
} resp_number_s;

#define resp_obj2arr(obj)                                                      \
  ((resp_array_s *)((obj)->type == RESP_ARRAY || (obj)->type == RESP_PUBSUB    \
                        ? (obj)                                                \
                        : NULL))
#define resp_obj2str(obj)                                                      \
  ((resp_string_s *)((obj)->type == RESP_STRING || (obj)->type == RESP_ERR     \
                         ? (obj)                                               \
                         : NULL))
#define resp_obj2num(obj)                                                      \
  ((resp_number_s *)((obj)->type == RESP_NUMBER ? (obj) : NULL))

/** Allocates an RESP NULL objcet. Remeber to free when done. */
resp_object_s *resp_nil2obj(void);

/** Allocates an RESP OK objcet. Remeber to free when done. */
resp_object_s *resp_OK2obj(void);

/** Allocates an RESP Error objcet. Remeber to free when done. */
resp_object_s *resp_err2obj(const void *msg, size_t len);

/** Allocates an RESP Number objcet. Remeber to free when done. */
resp_object_s *resp_num2obj(uint64_t num);

/** Allocates an RESP String objcet. Remeber to free when done. */
resp_object_s *resp_str2obj(const void *str, size_t len);

/**
 *Allocates an RESP Array objcet. Remeber to free when done (freeing an array
 *frees it's children automatically).
 *
 * It's possible to pass NULL as the `argv`, in which case the array created
 * will have the capacity `argc` and could me manually populated.
 *
 * The objects are MOVED into the array's possesion. If you wish to retain a
 * copy of the objects, use the `resp_dup_object` to increase their reference
 * count.
 */
resp_object_s *resp_arr2obj(int argc, resp_object_s *argv[]);

/** Duplicates an object by increasing it's reference count. */
resp_object_s *resp_dup_object(resp_object_s *obj);

/** frees an object by decreasing it's reference count and testing. */
void resp_free_object(resp_object_s *obj);

/**
 * Formats a RESP object back into a string.
 *
 * Returns 0 on success and -1 on failur.
 *
 * Accepts a memory buffer `dest` to which the data will be written and a poiner
 * to the size of the buffer.
 *
 * Once the function returns, `size` will be updated to include the number of
 * bytes required for the string. If the function returned a failure, this value
 * can be used to allocate enough memory to contain the string.
 *
 * The string is Binary safe and it ISN'T always NUL terminated.
 *
 * The optional `parser` argument allows experimental extensions to be used when
 * formatting the object. It can be ignored when formatting without extensions.
 *
 * **If implementing a server**:
 *
 * When implementing a server, Pub/Sub should avoid multiple copies by using a
 * dedicated buffer with a reference count. By decreasing the reference count
 * every time the message was sent (see the `sock_write2` support for the
 * dealloc callback), it's possible to avoid multiple copies of the message.
 */
int resp_format(resp_parser_pt p, uint8_t *dest, size_t *size,
                resp_object_s *obj);

/**
 * Performs a task on each object. Protects from loop-backs.
 *
 * To break loop in the middle, `task` can return -1.
 *
 * Returns count.
 */
size_t resp_obj_each(resp_parser_pt p, resp_object_s *obj,
                     int (*task)(resp_parser_pt p, resp_object_s *obj,
                                 void *arg),
                     void *arg);

/* *****************************************************************************
The RESP Parser
***************************************************************************** */

/** create the parser */
resp_parser_pt resp_parser_new(void);

/** free the parser and it's resources. */
void resp_parser_destroy(resp_parser_pt);

/** Clears the parser state, as if starting a new session / connection. */
void resp_parser_clear(resp_parser_pt);

/**
 * Feed the parser with data.
 *
 * Returns any fully parsed object / reply (often an array, but not always) or
 * NULL (needs more data / error).
 *
 * If a RESP object was parsed, it is returned and `len` is updated to reflect
 * the number of bytes actually read.
 *
 * If more data is needed, NULL is returned and `len` is left unchanged.
 *
 * An error is reported by by returning NULL and setting `len` to 0 at the same
 * time.
 *
 * Partial consumption is possible when multiple replys were available in the
 * buffer. Otherwise the parser will consume the whole of the buffer.
 *
 */
resp_object_s *resp_parser_feed(resp_parser_pt, uint8_t *buffer, size_t *len);

/* *****************************************************************************
State - The Pub / Sub Multiplexer (Experimental)
***************************************************************************** */

/**
It seems to me that the main reason that pub/sub messages and normal RESP
connetcions cannot share the same socket is the risk of identity collisions.

For example, the command LRANGE might return the following array response
`["message", "users", "hello"]`... this looks exactly like a Pub/Sub
notification.

However, this situation is very uncomfortable. For example:

* The sender already knows the content of the message. There is no reason to
  waste bandwidth to send the same message back to the sender using a different
  socket.

* The cost isn't just Bandwidth, but also memory, since the sender will have two
  copies of the same message (if not more), the one being sent and the other
  being received (sometimes more then once, for different channel patterns).


* Instead of handling the message localy, the sender is forced to wait until the
  message is received by the pub/sub Redis conection - otherwise tere will be
  duplicate messages being published.

* This design doubles the client load (number of connections) for each Redis
  server (and client).

But we can solve this.

For example, what if we use a "magic byte" to distinguish between the array
`["message", "users", "hello"]` and the pub/sub notification `["message",
"users", "hello"]`?

What if every time the first word in an array response satrts with an `"m"` or a
`"+"`, we will add the `"+"` byte infront of it?

Now the notification will look like this: `["message", "users", "hello"]`, and
the array response (i.e. to `LRANGE`) will be: `["+message", "users", "hello"]`
- a distinct difference allowing for pub/sub and regular conections to use the
same pipelining socket.

The big issue (and I may be wrong), is backwards compatibility - we can't change
the semantics of the protocol without breaking existing clients... or can we?

I believe this hurdle can be easily circumvented by adding a single command to
the existing pallet. Somthing along the lines of: `ENABLE <feature>`.

Such a flexible command allows clients to negotiate changes to the semantics
for the connection. It's meant to be a single non-reversible change for the
specific connection (similar to the `Upgrade` HTTP/Websocket concept).

Now, the little `"+"` "magic byte" can be easily handled.

The following function activates the "magic byte" assuming the `ENABLE` command
was negotiated for the connection.
*/
void resp_enable_duplex_pubsub(resp_parser_pt parser);

#ifdef DEBUG
void resp_test(void);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
