# HTTP extension for lib-server

This folder contains HTTP Protocol (HttpProtocol) extension for `lib-server`... However, although this project's makefile contains instructions for subfolder compilation... you might want to copy all the files to a single folder when incorporating these libraries in your own project.

The folder hierarchy in this project is for maintenance convenience only and probably shouldn't be used in actual projects.

## Demo code

Here's a simple "Hello World" using the Http extensions:

```c
// include location may vary according to your makefile and project hierarchy.
#include "http.h"

#define THREAD_COUNT 1
#define WORKER_COUNT 1

void on_request(struct HttpRequest* request) {
  struct HttpResponse* response = HttpResponse.create(request);
  HttpResponse.write_body(response, "Hello World!", 12);
  HttpResponse.destroy(response);
}

/*****************************
The main function
*/
int main(int argc, char const* argv[]) {
  start_http_server(on_request, NULL,
                    .threads = THREAD_COUNT,
                    .processes = WORKER_COUNT );
  return 0;
}

```
