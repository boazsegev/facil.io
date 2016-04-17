#define _GNU_SOURCE
#include "http-request.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// we declare because we need them... implementation comes later.
static struct HttpRequest* request_new(void);
static void request_clear(struct HttpRequest* self);
static void request_destroy(struct HttpRequest* self);
static void request_first(struct HttpRequest* self);
static int request_next(struct HttpRequest* self);
static int request_find(struct HttpRequest* self, char* const name);
static char* request_name(struct HttpRequest* self);
static char* request_value(struct HttpRequest* self);
static int request_is_request(struct HttpRequest* self);

const struct HttpRequestClass HttpRequest = {
    // retures an new heap allocated request object
    .create = request_new,
    // releases the resources used by a request object and keeps it's memory.
    .clear = request_clear,
    // releases the resources used by a request object and frees it's memory.
    .destroy = request_destroy,
    // validated that this is a request object
    .is_request = request_is_request,

    // Header handling

    /// restarts the header itteration
    .first = request_first,
    /// moves to the next header. returns 0 if the end of the list was
    /// reached.
    .next = request_next,
    /// finds a specific header matching the requested string.
    /// all headers are lower-case, so the string should be lower case.
    /// returns 0 if the header couldn't be found.
    .find = request_find,
    /// returns the name of the current header in the itteration cycle.
    .name = request_name,
    /// returns the value of the current header in the itteration cycle.
    .value = request_value,
};

////////////////
// The Request object implementation

// The constructor
static struct HttpRequest* request_new(void) {
  struct HttpRequest* req = calloc(sizeof(struct HttpRequest), 1);
  req->private.is_request = request_is_request;
  return req;
}

// the destructor
static void request_destroy(struct HttpRequest* self) {
  if (!self || !self->server)
    return;
  if (self->body_file)
    fclose(self->body_file);
  self->server = 0;
  free(self);
}

// resetting the request
static void request_clear(struct HttpRequest* self) {
  if (!self || !self->server)
    return;
  if (self->body_file)
    fclose(self->body_file);
  self->method = 0;
  self->path = 0;
  self->query = 0;
  self->version = 0;
  self->host = 0;
  self->content_length = 0;
  self->content_type = 0;
  self->upgrade = 0;
  self->body_str = 0;
  self->body_file = NULL;
  self->private.header_hash = 0;
  self->private.pos = 0;
  self->private.max = 0;
  self->private.bd_rcved = 0;
}

// validating a request object
static int request_is_request(struct HttpRequest* self) {
  return (self && (self->private.is_request == request_is_request));
}

// implement the following request handlers:

static void request_first(struct HttpRequest* self) {
  self->private.pos = 0;
};
static int request_next(struct HttpRequest* self) {
  // repeat the following 2 times, as it's a name + value pair
  for (int i = 0; i < 2; i++) {
    // move over characters
    while (self->private.pos < self->private.max &&
           self->private.header_hash[self->private.pos])
      self->private.pos++;
    // move over NULL
    while (self->private.pos < self->private.max &&
           !self->private.header_hash[self->private.pos])
      self->private.pos++;
  }
  if (self->private.pos == self->private.max)
    return 0;
  return 1;
}
static int request_find(struct HttpRequest* self, char* const name) {
  self->private.pos = 0;
  do {
    if (!strcasecmp(self->private.header_hash + self->private.pos, name))
      return 1;
  } while (request_next(self));
  return 0;
}
static char* request_name(struct HttpRequest* self) {
  if (!self->private.header_hash[self->private.pos])
    return NULL;
  return self->private.header_hash + self->private.pos;
};
static char* request_value(struct HttpRequest* self) {
  if (!self->private.header_hash[self->private.pos])
    return NULL;
  int pos = self->private.pos;
  // move over characters
  while (pos < self->private.max && self->private.header_hash[pos])
    pos++;
  // move over NULL
  while (pos < self->private.max && !self->private.header_hash[pos])
    pos++;
  if (self->private.pos == self->private.max)
    return 0;
  return self->private.header_hash + pos;
};
