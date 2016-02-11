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

/////////////////
// functions used by the Http protocol, internally

#define _http_(protocol) ((struct HttpProtocol*)(protocol))

#define is_hex(c)                                              \
  (((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f') || \
   ((c) >= 'A' && c <= 'F'))
#define hex_val(c) (((c) >= '0' && (c) <= '9') ? ((c)-48) : (((c) | 32) - 87))

#define is_num(c) ((c) >= '0' && (c) <= '9')
#define num_val(c) ((c)-48)

static char* Day2Str[] = {"Sun",  // the week starts on Sunday.
                          "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static char* Mon2Str[] = {"Jan",  // the year starts on January.
                          "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                          "Aug", "Sep", "Oct", "Nov", "Dec"};

// reviewes the request and attempts to answer with a static file.
// returns 0 if no file was found, otherwise returns 1.
static int http_sendfile(struct HttpRequest* req) {
  static char* http_file_response_no_mime =
      "HTTP/1.1 200 OK\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout=1\r\n"
      "Accept-Ranges: none\r\n"
      "Content-Length: %lu\r\n"
      "Date: %s, %d %s %04d %02d:%02d:%02d GMT\r\n"
      "Last-Modified: %s, %d %s %04d %02d:%02d:%02d GMT\r\n"
      "\r\n";

  static char* http_file_response =
      "HTTP/1.1 200 OK\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout=1\r\n"
      "Accept-Ranges: none\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %lu\r\n"
      "Date: %s, %d %s %04d %02d:%02d:%02d GMT\r\n"
      "Last-Modified: %s, %d %s %04d %02d:%02d:%02d GMT\r\n"
      "\r\n";

  FILE* file;
  char* mime = NULL;
  char* ext = NULL;
  struct stat file_data = {};

  // collect the protocol path and the request's path length and data
  struct HttpProtocol* protocol =
      (struct HttpProtocol*)Server.get_protocol(req->server, req->sockfd);
  if (!protocol)
    return 1;
  size_t len = strlen(protocol->public_folder);
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

  // get file data (prevent folder access and get modification date)
  if (stat(fname, &file_data))
    return 0;
  // check that we have a file and not something else
  if (!S_ISREG(file_data.st_mode) && !S_ISLNK(file_data.st_mode))
    return 0;
  // fprintf(stderr, "looking for file: %s\n", fname);
  if ((file = fopen(fname, "rb"))) {
    // we will recycle len for the headers size and other things we want
    len = 0;

    // get the mime type (we have an ext pointer and the string isn't empty)
    if (ext && ext[1]) {
      mime = MimeType.find(ext + 1);
    }

    // Get a date data
    struct tm t_file;
    gmtime_r(&file_data.st_mtime, &t_file);
    struct tm t_now;
    gmtime_r(&Server.reactor(req->server)->last_tick, &t_now);

    // we now need to write some headers... we can recycle the ext pointer
    // for
    // the data
    if (HttpRequest.find(req, "RANGE") && ((ext = HttpRequest.value(req))) &&
        (ext[0] | 32) == 'b' && (ext[1] | 32) == 'y' && (ext[2] | 32) == 't' &&
        (ext[3] | 32) == 'e' && (ext[4] | 32) == 's' && (ext[5] | 32) == '=') {
      // ext holds the first range, starting on index 6 i.e. RANGE: bytes=0-1
      static char* http_range_response =
          "HTTP/1.1 206 Partial content\r\n"
          "Connection: keep-alive\r\n"
          "Keep-Alive: timeout=1\r\n"
          "%s%s%s"
          "Content-Length: %lu\r\n"
          "Date: %s, %d %s %04d %02d:%02d:%02d GMT\r\n"
          "Last-Modified: %s, %d %s %04d %02d:%02d:%02d GMT\r\n"
          "Accept-Ranges: bytes\r\n"
          "Content-Range: bytes %lu-%lu/%lu\r\n"
          "\r\n";
      size_t start = 0, finish = 0;
      ext = ext + 6;
      while (is_num(*ext)) {
        start = start * 10;
        start += num_val(*ext);
        ext++;
      }
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
      if (finish && finish >= start && (finish - start) < 65536 &&
          finish < file_data.st_size - 1) {
        // it's a small chunk, put it in the buffer and send it as data
        char* data = malloc(finish - start);
        if (!data) {
          fclose(file);
          return 0;
        }
        len = fread(data, 1, finish - start, file);
        if (len <= 0) {
          free(data);
          fclose(file);
          return 0;
        }
        len = asprintf(
            &ext, http_range_response, (mime ? "Content-Type: " : ""),
            (mime ? mime : ""), (mime ? "\r\n" : ""), len,
            Day2Str[t_now.tm_wday], t_now.tm_mday, Mon2Str[t_now.tm_mon],
            t_now.tm_year + 1900, t_now.tm_hour, t_now.tm_min, t_now.tm_sec,
            Day2Str[t_file.tm_wday], t_file.tm_mday, Mon2Str[t_file.tm_mon],
            t_file.tm_year + 1900, t_file.tm_hour, t_file.tm_min, t_file.tm_sec,
            start, finish - 1, file_data.st_size);
        // review the string
        if (!len || !ext) {
          fclose(file);
          return 0;
        }
        // send the headers and the data (moving the pointers to the buffer)
        Server.write_move(req->server, req->sockfd, ext, len);
        Server.write_move(req->server, req->sockfd, data, finish - start);
        return 1;
      } else {
        // going to the EOF (big chunk or EOL requested) - send as file
        finish = file_data.st_size - 1;
        fseek(file, start, SEEK_SET);
        len = asprintf(
            &ext, http_range_response, (mime ? "Content-Type: " : ""),
            (mime ? mime : ""), (mime ? "\r\n" : ""), file_data.st_size - start,
            Day2Str[t_now.tm_wday], t_now.tm_mday, Mon2Str[t_now.tm_mon],
            t_now.tm_year + 1900, t_now.tm_hour, t_now.tm_min, t_now.tm_sec,
            Day2Str[t_file.tm_wday], t_file.tm_mday, Mon2Str[t_file.tm_mon],
            t_file.tm_year + 1900, t_file.tm_hour, t_file.tm_min, t_file.tm_sec,
            start, finish, file_data.st_size);
        // send the headers and the file
        Server.write_move(req->server, req->sockfd, ext, len);
        Server.sendfile(req->server, req->sockfd, file);
        return 1;
      }
    }
  invalid_range:
    if (mime)
      len = asprintf(
          &ext, http_file_response, mime, file_data.st_size,
          Day2Str[t_now.tm_wday], t_now.tm_mday, Mon2Str[t_now.tm_mon],
          t_now.tm_year + 1900, t_now.tm_hour, t_now.tm_min, t_now.tm_sec,
          Day2Str[t_file.tm_wday], t_file.tm_mday, Mon2Str[t_file.tm_mon],
          t_file.tm_year + 1900, t_file.tm_hour, t_file.tm_min, t_file.tm_sec);
    else
      len = asprintf(
          &ext, http_file_response_no_mime, file_data.st_size,
          Day2Str[t_now.tm_wday], t_now.tm_mday, Mon2Str[t_now.tm_mon],
          t_now.tm_year + 1900, t_now.tm_hour, t_now.tm_min, t_now.tm_sec,
          Day2Str[t_file.tm_wday], t_file.tm_mday, Mon2Str[t_file.tm_mon],
          t_file.tm_year + 1900, t_file.tm_hour, t_file.tm_min, t_file.tm_sec);

    // review the string
    if (!len || !ext) {
      fclose(file);
      return 0;
    }

    // send headers
    Server.write_move(req->server, req->sockfd, ext, len);
    // send file, unless the request method is "HEAD"
    if (strcmp("HEAD", req->method))
      Server.sendfile(req->server, req->sockfd, file);
    // // The file will be closed by the buffer.
    // DONT fclose(file);

    // DEBUG - print headers
    // HttpRequest.first(req);
    // do {
    //   fprintf(stderr, "%s: %s\n", HttpRequest.name(req),
    //           HttpRequest.value(req));
    // } while (HttpRequest.next(req));

    return 1;
  }
  return 0;
}

// implement on_close to close the FILE * for the body (if exists).
static void http_on_close(struct Server* server, int sockfd) {
  HttpRequest.destroy(Server.set_udata(server, sockfd, NULL));
}
// implement on_data to parse incoming requests.
static void http_on_data(struct Server* server, int sockfd) {
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
  if (!request) {
    Server.set_udata(server, sockfd,
                     (request = HttpRequest.new(server, sockfd)));
  }
  char* buff = request->buffer;
  int pos = request->private.pos;

restart:

  // is this an ongoing request?
  if (request->body_file) {
    char buff[HTTP_HEAD_MAX_SIZE];
    int t = 0;
    while ((len = Server.read(sockfd, buff, HTTP_HEAD_MAX_SIZE)) > 0) {
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
  len = Server.read(sockfd, buff + pos, HTTP_HEAD_MAX_SIZE - pos);
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
    if (pos > len - 3)
      goto bad_request;
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
    if (pos + 5 > len)
      goto bad_request;
    request->version = buff + pos;
    if (buff[pos] != 'H' || buff[pos + 1] != 'T' || buff[pos + 2] != 'T' ||
        buff[pos + 3] != 'P')
      goto bad_request;
    // find first header name
    while (pos < len - 2 && buff[pos] != '\r')
      pos++;
    if (pos > len - 2)  // must have 2 EOL markers before a header
      goto bad_request;
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
      if (buff[pos] >= 'a' && buff[pos] <= 'z')
        buff[pos] = buff[pos] & 223;  // uppercase the header field.
      // buff[pos] = buff[pos] | 32;    // lowercase is nice, but less common.
      pos++;
    }
    if (pos >= len - 1)  // must have at least 2 eol markers + data
      goto bad_request;
    buff[pos++] = 0;
    if (buff[pos] == ' ')  // space after colon?
      buff[pos++] = 0;
    tmp2 = buff + pos;
    // skip value
    while (pos + 1 < len && buff[pos] != '\r')
      pos++;
    if (pos >= len - 1)  // must have at least 2 eol markers...
      goto bad_request;
    buff[pos++] = 0;
    buff[pos++] = 0;
    if (!strcmp(tmp1, "HOST")) {
      request->host = tmp2;
      // lowercase of hosts, to support case agnostic dns resolution
      while (*tmp2 && (*tmp2) != ':') {
        if (*tmp2 >= 'A' && *tmp2 <= 'Z')
          *tmp2 = *tmp2 | 32;
        tmp2++;
      }
    } else if (!strcmp(tmp1, "CONTENT-TYPE")) {
      request->content_type = tmp2;
    } else if (!strcmp(tmp1, "CONTENT-LENGTH")) {
      request->content_length = atoi(tmp2);
    } else if (!strcmp(tmp1, "UPGRADE")) {
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
  if (!request->host ||
      (request->content_type &&
       !request->content_length))  // convert dynamically to Mb?
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
  if (!strcmp(request->method, "OPTIONS"))
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
  if ((!protocol->public_folder || !http_sendfile(request)) &&
      protocol->on_request) {
    protocol->on_request(request);
  }

  if (Server.get_udata(server, sockfd)) {
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
    goto parse;
  }
  if (len == HTTP_HEAD_MAX_SIZE) {
    // we might not have read all the data in the network socket.
    // since we're edge triggered, we should continue reading.
    len = Server.read(sockfd, buff, HTTP_HEAD_MAX_SIZE);
    if (len > 0) {
      HttpRequest.clear(request);
      Server.set_udata(server, sockfd, request);
      goto parse;
    }
  }

cleanup_after_finish:

  // we need to destroy the request ourselves, because we disconnected the
  // request from the server's udata.
  HttpRequest.destroy(request);
  return;

options:
  // send a bed request response. hang up.
  send(sockfd, options_req, strlen(options_req), 0);
  Server.close(request->server, sockfd);
  return;

bad_request:
  // send a bed request response. hang up.
  send(sockfd, bad_req, strlen(bad_req), 0);
  Server.close(request->server, sockfd);
  return;

too_big:
  // send a bed request response. hang up.
  send(sockfd, too_big_err, strlen(too_big_err), 0);
  Server.close(request->server, sockfd);
  return;

internal_error:
  // send an internal error response. hang up.
  send(sockfd, intr_err, strlen(intr_err), 0);
  Server.close(request->server, sockfd);
  return;
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

/// returns a stack allocated, core-initialized, Http Protocol object.
struct HttpProtocol HttpProtocol(void) {
  return (struct HttpProtocol){
      .parent.service = "http",
      .parent.on_data = http_on_data,
      .parent.on_close = http_on_close,
      .maximum_body_size = 32,
      .on_request = http_default_on_request,
      .public_folder = NULL,
  };
}
