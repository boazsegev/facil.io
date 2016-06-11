/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#define _GNU_SOURCE
#include "http-protocol.h"
#include "http-mime-types.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

/////////////////
// Header case (Uppercase vs. Lowercase)
#ifndef HEADERS_UPPERCASE
#define HEADERS_UPPERCASE 1
#endif

#if HEADERS_UPPERCASE == 1
#define HOST_HEADER "HOST"
#define CONTENT_TYPE_HEADER "CONTENT-TYPE"
#define CONTENT_LENGTH_HEADER "CONTENT-LENGTH"
#define UPGRADE_HEADER "UPGRADE"
#define CONNECTION_HEADER "CONNECTION"
#else
#define HOST_HEADER "host"
#define CONTENT_TYPE_HEADER "content-type"
#define CONTENT_LENGTH_HEADER "content-length"
#define UPGRADE_HEADER "upgrade"
#define CONNECTION_HEADER "connection"
#endif

#define HTTP_SEND_RANGE_AS_DATA_LIMIT (1024 * 32)

/////////////////
// functions used by the Http protocol, internally

#define _http_(protocol) ((struct HttpProtocol*)(protocol))

#define is_hex(c)                                              \
  (((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f') || \
   ((c) >= 'A' && c <= 'F'))
#define hex_val(c) (((c) >= '0' && (c) <= '9') ? ((c)-48) : (((c) | 32) - 87))

#define is_num(c) ((c) >= '0' && (c) <= '9')
#define num_val(c) ((c)-48)

// reviewes the request and attempts to answer with a static file.
// returns 0 if no file was found, otherwise returns 1.
static int http_sendfile(struct HttpRequest* req) {
  // collect the protocol path and the request's path length and data
  struct HttpProtocol* protocol =
      (struct HttpProtocol*)Server.get_protocol(req->server, req->sockfd);
  if (!protocol)
    return 1;
  if (protocol->public_folder == NULL)
    return 0;
  if (protocol->public_folder_length == 0)
    protocol->public_folder_length = strlen(protocol->public_folder);

  int file = -1;
  char* mime = NULL;
  char* ext = NULL;
  struct stat file_data = {};
  // fprintf(stderr, "\n\noriginal request path: %s\n", req->path);

  size_t len = protocol->public_folder_length;
  // create and initialize the filename, including decoding the path
  char fname[strlen(req->path) + len + 1];
  memcpy(fname, protocol->public_folder, len);
  // if the ast character is a '/', step back.
  if (fname[len - 1] == '/' || fname[len - 1] == '\\')
    len--;
  // decode and review the request->path data
  int i = 0;
  while (req->path[i]) {
    if (req->path[i] == '+')  // decode space
      fname[len] = ' ';
    else if (req->path[i] == '%') {
      // decode hex value
      if (is_hex(req->path[i + 1]) && is_hex(req->path[i + 2])) {
        // this is a percent encoded value.
        fname[len] =
            (hex_val(req->path[i + 1]) * 16) + hex_val(req->path[i + 2]);
        i += 2;
      } else {
        // there was an error in the URL encoding... what to do? ignore?
        return 0;
      }
    } else
      fname[len] = req->path[i];
    len++;
    i++;
  }
  fname[len] = 0;
  i = 0;

  // scan path string for double dots (security - prevent path manipulation)
  // set the extention point value, while were doing so.
  while (fname[i]) {
    if (fname[i] == '.')
      ext = fname + i;
    // return false if we found a "/.." in our string.
    if (fname[i++] == '/' && fname[i++] == '.' && fname[i++] == '.')
      return 0;
  }
  // fprintf(stderr, "file name: %s\noriginal request path: %s\n", fname,
  //         req->path);

  // get file data (prevent folder access and get modification date)
  if (stat(fname, &file_data))
    return 0;
  // check that we have a file and not something else
  if (!S_ISREG(file_data.st_mode) && !S_ISLNK(file_data.st_mode))
    return 0;

  // Okay - we have data - time to make a response.
  struct HttpResponse* response = HttpResponse.create(req);
  if (!response) {
    Server.close(req->server, req->sockfd);
    return 1;
  }
  if ((file = open(fname, O_RDONLY)) < 0)
    goto internal_error;

  // get the mime type (we have an ext pointer and the string isn't empty)
  if (ext && ext[1]) {
    mime = MimeType.find(ext + 1);
    if (mime) {
      HttpResponse.write_header(response, "Content-Type", 12, mime,
                                strlen(mime));
    }
  }

  // Set a date data
  response->last_modified = file_data.st_mtime;

  // Range handling
  if (HttpRequest.find(req, "range") && ((ext = HttpRequest.value(req))) &&
      (ext[0] | 32) == 'b' && (ext[1] | 32) == 'y' && (ext[2] | 32) == 't' &&
      (ext[3] | 32) == 'e' && (ext[4] | 32) == 's' && (ext[5] | 32) == '=') {
    // ext holds the first range, starting on index 6 i.e. RANGE: bytes=0-1
    // "HTTP/1.1 206 Partial content\r\n"
    // "Accept-Ranges: bytes\r\n"
    // "Content-Range: bytes %lu-%lu/%lu\r\n"
    // fprintf(stderr, "Got a range request %s\n", ext);
    size_t start = 0, finish = 0;
    ext = ext + 6;
    while (is_num(*ext)) {
      start = start * 10;
      start += num_val(*ext);
      ext++;
    }
    // fprintf(stderr, "Start: %lu / %lld\n", start, file_data.st_size);
    if (start >= file_data.st_size - 1)
      goto invalid_range;
    ext++;
    while (is_num(*ext)) {
      finish = finish * 10;
      finish += num_val(*ext);
      ext++;
    }
    if (finish)
      finish++;
    // fprintf(stderr, "finish: %lu / %lld\n", finish, file_data.st_size);
    if (finish && finish >= start &&
        (finish - start) < HTTP_SEND_RANGE_AS_DATA_LIMIT) {
      // it's a "small" chunk, put it in the buffer and send it as data
      // fprintf(stderr, "handling no-file chunk\n");
      char* data = malloc(finish - start);
      if (!data) {
        goto bad_request;
      }
      len = read(file, data, finish - start);
      if (len <= 0) {
        free(data);
        goto bad_request;
      }
      close(file);
      file = -1;
      response->status = 206;
      HttpResponse.write_header(response, "Accept-Ranges", 13, "bytes", 5);
      HttpResponse.write_header(response, "Cache-Control", 13,
                                "public, max-age=3600", 20);
      if (HttpResponse.printf(response, "Content-Range: bytes %lu-%lu/%lld",
                              start, start + len - 1, file_data.st_size)) {
        HttpResponse.reset(response, req);
        goto internal_error;
      }
      HttpResponse.write_body_move(response, data, len);
      HttpResponse.destroy(response);
      return 1;
    } else {
      // going to the EOF (big chunk or EOL requested) - send as file
      if (finish >= file_data.st_size)
        finish = file_data.st_size - 1;
      lseek(file, start, SEEK_SET);
      if (HttpResponse.printf(response, "Content-Range: bytes %lu-%lu/%lu",
                              start, finish, file_data.st_size)) {
        HttpResponse.reset(response, req);
        goto internal_error;
      }
      response->status = 206;
      HttpResponse.write_header(response, "Cache-Control", 13,
                                "public, max-age=3600", 20);
      HttpResponse.write_header(response, "Accept-Ranges", 13, "bytes", 5);
      HttpResponse.sendfile(response, file, file_data.st_size - start);
      HttpResponse.destroy(response);
      return 1;
    }
  }

invalid_range:
  lseek(file, 0, SEEK_SET);
  HttpResponse.write_header(response, "Accept-Ranges", 13, "none", 4);
  // set caching
  HttpResponse.write_header(response, "Cache-Control", 13,
                            "public, max-age=3600", 20);

  // send data
  if (strcasecmp("HEAD", req->method) == 0) {
    HttpResponse.send(response);
    close(file);
  } else
    HttpResponse.sendfile(response, file, file_data.st_size);
  HttpResponse.destroy(response);

  return 1;

internal_error:
  if (file >= 0)
    close(file);
  response->status = 500;
  response->metadata.should_close = 1;
  HttpResponse.write_body(response, "Internal error (F01)", 20);
  HttpResponse.destroy(response);
  return 1;
bad_request:
  if (file >= 0)
    close(file);
  response->status = 400;
  response->metadata.should_close = 1;
  HttpResponse.write_body(response, "Bad Request.", 12);
  HttpResponse.destroy(response);
  return 1;
}

// implement on_close to close the FILE * for the body (if exists).
static void http_on_close(server_pt server, uint64_t sockfd) {
  struct HttpRequest* request = Server.get_udata(server, sockfd);
  if (HttpRequest.is_request(request)) {
    // clear the request data.
    HttpRequest.destroy(request);
  }
}
// implement on_data to parse incoming requests.
static void http_on_data(const struct Server* server, uint64_t sockfd) {
  // setup static error codes
  static char* options_req =
      "HTTP/1.1 200 OK\r\n"
      "Allow: GET,HEAD,POST,PUT,DELETE,OPTIONS\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Accept-Ranges: none\r\n"
      "Connection: closed\r\n"
      "Content-Length: 0\r\n\r\n";
  static char* bad_req =
      "HTTP/1.1 400 Bad HttpRequest\r\n"
      "Connection: closed\r\n"
      "Content-Length: 16\r\n\r\n"
      "Bad HTTP Request\r\n";
  static char* too_big_err =
      "HTTP/1.1 413 Entity Too Large\r\n"
      "Connection: closed\r\n"
      "Content-Length: 18\r\n\r\n"
      "Entity Too Large\r\n";
  static char* intr_err =
      "HTTP/1.1 502 Internal Error\r\n"
      "Connection: closed\r\n"
      "Content-Length: 16\r\n\r\n"
      "Internal Error\r\n";
  // top : { ; }
  int len = 0;
  char* tmp1 = NULL;
  char* tmp2 = NULL;
  struct HttpProtocol* protocol =
      (struct HttpProtocol*)Server.get_protocol(server, sockfd);
  if (!protocol) {
    return;
  }
  struct HttpRequest* request = Server.get_udata(server, sockfd);
  if (request && !HttpRequest.is_request(request)) {
    // someone else is using the connection and/or storage, we can't continue.
    return;
  }
  if (!request) {
    Server.set_udata(server, sockfd, (request = HttpRequest.create()));
    request->server = server;
    request->sockfd = sockfd;
  }
  char* buff = request->buffer;
  int pos = request->private.pos;

restart:

  // is this an ongoing request?
  if (request->body_file) {
    char buff[HTTP_HEAD_MAX_SIZE];
    int t = 0;
    while ((len = Server.read(server, sockfd, buff, HTTP_HEAD_MAX_SIZE)) > 0) {
      pos = len;
      if (request->content_length - request->private.bd_rcved < pos) {
        pos = request->content_length - request->private.bd_rcved;
      }
      if ((t = fwrite(buff, 1, pos, request->body_file)) < pos) {
        perror("Tmpfile Err");
        goto internal_error;
      }
      request->private.bd_rcved += pos;
    }

    if (request->private.bd_rcved >= request->content_length) {
      rewind(request->body_file);
      goto finish;
    }
    return;
  }

  // review used size
  if (HTTP_HEAD_MAX_SIZE <= pos - 1)
    goto too_big;

  // read from the buffer
  len = Server.read(server, sockfd, buff + pos, HTTP_HEAD_MAX_SIZE - pos);
  if (len <= 0) {
    // buffer is empty, but more data is underway or error
    // anyway, don't cleanup - let `on_close` do it's job
    request->private.pos = pos;
    return;
  }
  // adjust length for buffer size positioing (so that len == max pos - 1).
  len += pos;

// review the data
parse:

  // check if the request is new
  if (!pos) {
    // start parsing the request
    request->method = request->buffer;
    // get query
    while (pos < (len - 1) && buff[pos] != ' ')
      pos++;
    buff[pos++] = 0;
    if (pos > len - 3) {
      if (len >= HTTP_HEAD_MAX_SIZE - 32)
        goto too_big;
      else
        goto bad_request;
    }
    request->path = &buff[pos];
    // get query and version
    while (pos < (len - 1) && buff[pos] != ' ' && buff[pos] != '?')
      pos++;
    if (buff[pos] == '?') {
      buff[pos++] = 0;
      request->query = buff + pos;
      while (pos < (len - 1) && buff[pos] != ' ')
        pos++;
    }
    buff[pos++] = 0;
    if (pos + 5 > len) {
      if (len >= HTTP_HEAD_MAX_SIZE - 32)
        goto too_big;
      else
        goto bad_request;
    }
    request->version = buff + pos;
    if (buff[pos] != 'H' || buff[pos + 1] != 'T' || buff[pos + 2] != 'T' ||
        buff[pos + 3] != 'P')
      goto bad_request;
    // find first header name
    while (pos < len - 2 && buff[pos] != '\r')
      pos++;
    if (pos > len - 2)  // must have 2 EOL markers before a header
    {
      if (len >= HTTP_HEAD_MAX_SIZE - 32)
        goto too_big;
      else
        goto bad_request;
    }
    buff[pos++] = 0;
    buff[pos++] = 0;

    request->private.header_hash = buff + pos;
    request->private.max = pos;
  }
  if (len == 2 && buff[pos] == '\r' && buff[pos + 1] == '\n')
    goto finish_headers;
  // get headers
  while (pos < len && buff[pos] != '\r') {
    tmp1 = buff + pos;
    while (pos < len && buff[pos] != ':') {
#if HEADERS_UPPERCASE == 1
      // uppercase / Ruby style.
      if (buff[pos] >= 'a' && buff[pos] <= 'z')
        buff[pos] = buff[pos] & 223;
#else
      // lowercase / Node.js style.
      if (buff[pos] >= 'A' && buff[pos] <= 'Z')
        buff[pos] = buff[pos] | 32;
#endif
      pos++;
    }
    if (pos >= len - 1)  // must have at least 2 eol markers + data
    {
      if (len >= HTTP_HEAD_MAX_SIZE - 32)
        goto too_big;
      else
        goto bad_request;
    }
    buff[pos++] = 0;
    if (buff[pos] == ' ')  // space after colon?
      buff[pos++] = 0;
    tmp2 = buff + pos;
    // skip value
    while (pos + 1 < len && buff[pos] != '\r')
      pos++;
    if (pos >= len - 1)  // must have at least 2 eol markers...
    {
      if (len >= HTTP_HEAD_MAX_SIZE - 32)
        goto too_big;
      else
        goto bad_request;
    }
    buff[pos++] = 0;
    buff[pos++] = 0;
#if HEADERS_UPPERCASE == 1
    if (tmp1[0] == 'C')
#else
    if (tmp1[0] == 'c')
#endif
    {
      if (!strcmp(tmp1, CONTENT_TYPE_HEADER)) {
        request->content_type = tmp2;
      } else if (!strcmp(tmp1, CONTENT_LENGTH_HEADER)) {
        request->content_length = atoi(tmp2);
      } else if (!strcmp(tmp1, CONNECTION_HEADER)) {
        request->connection = tmp2;
      }
    } else if (!strcmp(tmp1, HOST_HEADER)) {
      request->host = tmp2;
      // lowercase of hosts, to support case agnostic dns resolution
      while (*tmp2 && (*tmp2) != ':') {
        if (*tmp2 >= 'A' && *tmp2 <= 'Z')
          *tmp2 = *tmp2 | 32;
        tmp2++;
      }
    } else if (!strcmp(tmp1, UPGRADE_HEADER)) {
      request->upgrade = tmp2;
    }
  }

  // check if the the request was fully sent (the trailing \r\n is available)
  if (pos >= len - 1) {
    // break it up...
    goto restart;
  }

finish_headers:

  // set the safety endpoint
  request->private.max = pos - request->private.max;

  // check for required `host` header and body content length (not chuncked)
  if (!request->host || (request->content_type && !request->content_length))
    goto bad_request;
  // zero out the last two "\r\n" before any message body
  buff[pos++] = 0;
  buff[pos++] = 0;

  // no body, finish up
  if (!request->content_length)
    goto finish;

  // manage body
  if (request->content_length > protocol->maximum_body_size * 1024 * 1024)
    goto too_big;
  // did the body fit inside the received buffer?
  if (request->content_length + pos <= len) {
    // point the budy to the data
    request->body_str = buff + pos;
    // setup a NULL terminator?
    request->body_str[request->content_length] = 0;
    // advance the buffer pos
    pos += request->content_length;
    // finish up
    goto finish;
  } else {
    // we need a temporary file for the data.
    request->body_file = tmpfile();
    if (!request->body_file)
      goto internal_error;
    // write any trailing data to the tmpfile
    if (len - pos > 0) {
      if (fwrite(buff + pos, 1, len - pos, request->body_file) < len - pos)
        goto internal_error;
    }
    // add data count to marker
    request->private.bd_rcved = len - pos;
    // notifications are edge based. If there's still data in the stream, we
    // need to read it.
    goto restart;
  }

finish:

  // answer the OPTIONS method, if exists
  if (!strcasecmp(request->method, "OPTIONS"))
    goto options;

  // reset inner "pos"
  request->private.pos = 0;

  // disconnect the request object from the server storage
  // this prevents on_close from clearing the memory while on_request is still
  // accessing the request.
  // It also allows upgrade protocol objects to use the storage for their
  // data.
  Server.set_udata(server, sockfd, NULL);

  // perform callback if a file wasn't sent.
  if ((protocol->public_folder == NULL || http_sendfile(request) == 0) &&
      protocol->on_request) {
    protocol->on_request(request);
  }

  if (!Server.is_open(server, sockfd)) {
    // someone else already started using this connection...
    goto cleanup_after_finish;
  }
  if (pos < len) {
    // if we have more data in the pipe, clear the request, move the buffer data
    // and return to the beginning of the parsing.
    HttpRequest.clear(request);
    // move the data left in the buffer to the beginning of the buffer.
    for (size_t i = 0; i < len - pos; i++) {
      request->buffer[i] = request->buffer[pos + i];
    }
    len = len - pos;
    pos = 0;
    Server.set_udata(server, sockfd, request);
    if (Server.get_udata(server, sockfd) != request) {
      goto cleanup_after_finish;
    }
    goto parse;
  }
  if (len == HTTP_HEAD_MAX_SIZE) {
    // we might not have read all the data in the network socket.
    // since we're edge triggered, we should continue reading.
    len = Server.read(server, sockfd, buff, HTTP_HEAD_MAX_SIZE);
    if (len > 0) {
      HttpRequest.clear(request);
      Server.set_udata(server, sockfd, request);
      goto parse;
    }
  }

cleanup_after_finish:

  // we need to destroy the request ourselves, because we disconnected the
  // request from the server's udata.
  if (request && HttpRequest.is_request(request)) {
    HttpRequest.destroy(request);
    return;
  }
  return;

options:
  // send a bed request response. hang up.
  Server.write(server, sockfd, options_req, sizeof(options_req) - 1);
  Server.close(server, sockfd);
  goto cleanup_after_finish;

bad_request:
  // send a bed request response. hang up.
  Server.write(server, sockfd, bad_req, sizeof(bad_req) - 1);
  Server.close(server, sockfd);
  goto cleanup_after_finish;

too_big:
  // send a bed request response. hang up.
  Server.write(server, sockfd, too_big_err, sizeof(too_big_err) - 1);
  Server.close(server, sockfd);
  goto cleanup_after_finish;

internal_error:
  // send an internal error response. hang up.
  Server.write(server, sockfd, intr_err, sizeof(intr_err) - 1);
  Server.close(server, sockfd);
  goto cleanup_after_finish;
}

// implement on_data to parse incoming requests.
void http_default_on_request(struct HttpRequest* req) {
  // the response format
  static char* http_format =
      "HTTP/1.1 200 OK\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: 1\r\n"
      "Content-Length: %d\r\n\r\n"
      "%s";
  static char* http_file_echo =
      "HTTP/1.1 200 OK\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: 1\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %d\r\n\r\n";

  if (req->body_file) {
    char* head = NULL;
    if (asprintf(&head, http_file_echo, req->content_type,
                 req->content_length) <= 0 ||
        !head) {
      perror("WTF?! head");
      return;
    }
    if (Server.write_move(req->server, req->sockfd, head, strlen(head)) < 0)
      return;
    head = malloc(req->content_length + 1);  // the +1 is redundent.
    if (!head) {
      perror("WTF?! body");
      return;
    }
    if (!fread(head, 1, req->content_length, req->body_file)) {
      perror("WTF?! file reading");
      free(head);
      return;
    }
    Server.write_move(req->server, req->sockfd, head, req->content_length);
    return;
  }
  // write reques's head onto the buffer
  char buff[HTTP_HEAD_MAX_SIZE] = {0};
  int pos = 0;
  strcpy(buff, req->method);
  pos += strlen(req->method);
  buff[pos++] = ' ';
  strcpy(buff + pos, req->path);
  pos += strlen(req->path);
  if (req->query) {
    buff[pos++] = '?';
    strcpy(buff + pos, req->query);
    pos += strlen(req->query);
  }
  buff[pos++] = ' ';
  strcpy(buff + pos, req->version);
  pos += strlen(req->version);
  buff[pos++] = '\r';
  buff[pos++] = '\n';
  HttpRequest.first(req);
  do {
    strcpy(buff + pos, HttpRequest.name(req));
    pos += strlen(HttpRequest.name(req));
    buff[pos++] = ':';
    strcpy(buff + pos, HttpRequest.value(req));
    pos += strlen(HttpRequest.value(req));
    buff[pos++] = '\r';
    buff[pos++] = '\n';
  } while (HttpRequest.next(req));

  if (req->body_str) {
    buff[pos++] = '\r';
    buff[pos++] = '\n';
    memcpy(buff + pos, req->body_str, req->content_length);
    pos += req->content_length;
  }
  buff[pos++] = 0;
  // Prep reply
  char* reply;
  int buff_len = strlen(buff);
  buff_len = asprintf(&reply, http_format, buff_len, buff);
  // check
  if (!reply) {
    perror("WTF?!");
    Server.close(req->server, req->sockfd);
    return;
  }
  // send(req->sockfd, reply, strlen(reply), 0);
  Server.write_move(req->server, req->sockfd, reply, buff_len);
}

////////////////
// public API

static char http_service_name[] = "http";
/** returns a new, initialized, Http Protocol object. */
struct HttpProtocol* HttpProtocol_new(void) {
  struct HttpProtocol* http = malloc(sizeof(struct HttpProtocol));
  memset(http, 0, sizeof(struct HttpProtocol));
  http->parent.service = http_service_name;
  http->parent.on_data = http_on_data;
  http->parent.on_close = http_on_close;
  http->maximum_body_size = 32;
  http->on_request = http_default_on_request;
  http->public_folder = NULL;
  return http;
}
void HttpProtocol_destroy(struct HttpProtocol* http) {
  free(http);
}

struct HttpProtocolClass HttpProtocol = {
    .create = HttpProtocol_new,
    .destroy = HttpProtocol_destroy,
};
