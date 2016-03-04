/////////////////////////////
// paste your favorite example code here, and run:
//
//       $ make run
//
// The *.o files are the binary saved in the tmp folder.

// #include <stdio.h>
//
// int main(int argc, char const* argv[]) {
//   printf("Hi!\n");
//   return 0;
// }

#include "lib-server.h"
#include "http-protocol.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

void (*org_on_request)(struct HttpRequest* req);
void on_request(struct HttpRequest* req) {
  // // sleep(1);
  // // busy wait for up to 1 seconds
  // time_t start_time, now_time;
  // time(&start_time);
  // time(&now_time);
  // while (now_time == start_time) {
  //   time(&now_time);
  // }
  // org_on_request(req);
  static char hello[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n"
      "Connection: keep-alive\r\n"
      "Keep-Alive: timeout = 1\r\n"
      "\r\n"
      "Hello World\r\n";
  Server.write(req->server, req->sockfd, hello, sizeof(hello));
}

// print client count
void p_count(server_pt srv) {
  int c = Server.count(srv, NULL);
  printf("%d Clients connected\n", c);
}

// testing timers
void on_init(server_pt srv) {
  Server.run_every(srv, 1000, 0, (void (*)(void*))p_count, srv);
}

// running the server
int main(void) {
  // We'll create the echo protocol object.
  // We'll use the server's default `on_request` callback (echo).
  struct HttpProtocol* protocol = HttpProtocol.new();
  org_on_request = protocol->on_request;
  // protocol->on_request = on_request;
  protocol->public_folder = "/Users/2Be/Documents/Scratch";
  // struct Protocol protocol = {.on_data = on_data, .on_close = on_close};
  // We'll use the macro start_server, because our settings are simple.
  // (this will call Server.listen(&settings) with the settings we provide)
  start_server(.protocol = (struct Protocol*)(protocol), .timeout = 5,
               .threads = 16, .on_init = on_init);
  HttpProtocol.destroy(protocol);
}

// /////////////////////////////
// // paste your favorite example code here, and run:
// //
// //       $ make run
// //
// // The *.o files are the binary saved in the tmp folder.
//
// #include "libasync.h"
// #include <stdlib.h>
// #include <stdio.h>
// #include <time.h>
// #include <unistd.h>
//
// float run_start;
//
// // This one will fail with safe kernels...
// // On windows you might get a blue screen...
// void evil_code(void* arg) {
//   char* rewrite = arg;
//   while (1) {
//     rewrite[0] = '0';
//     rewrite++;
//   }
// }
//
// // an example task
// void say_hi(void* arg) {
//   int static i = 0;
//   // i = i + 5;
//   fprintf(stderr, "Hi! %d\n", i++);
// }
//
// // an example task
// void schedule_tasks2(void* arg) {
//   fprintf(stderr, "SCHEDULE TASKS START (%lf)\n",
//           ((float)clock() / CLOCKS_PER_SEC) - run_start);
//   async_p async = arg;
//   for (size_t i = 0; i < (8 * 1024); i++) {
//     Async.run(async, say_hi, NULL);
//     printf("wrote task %lu\n", i);
//   }
//   Async.run(async, say_hi, NULL);
//   // Async.run(async, evil_code, NULL);
//   // Async.run(async, say_hi, NULL);
//   Async.signal(async);
//   printf("signal finish at %lf\n",
//          ((float)clock() / CLOCKS_PER_SEC) - run_start);
// }
// void schedule_tasks(void* arg) {
//   fprintf(stderr, "SCHEDULE TASKS START (%lf)\n",
//           ((float)clock() / CLOCKS_PER_SEC) - run_start);
//   async_p async = arg;
//   for (size_t i = 0; i < (8 * 1024); i++) {
//     Async.run(async, say_hi, NULL);
//     // printf("wrote task %lu\n", i);
//   }
//   Async.run(async, schedule_tasks2, async);
// }
//
// // an example usage
// int main(void) {
//   fprintf(stderr, "%d Testing Async library\n", getpid());
//   // create the thread pool with a single threads.
//   // the callback is optional (we can pass NULL)
//   async_p async = Async.new(32, NULL, NULL);
//   if (!async) {
//     perror("ASYNC creation failed");
//     exit(1);
//   }
//   // send a task
//   float run_start = (float)clock() / CLOCKS_PER_SEC;
//   // for (size_t i = 0; i < 16000; i++) {
//   //   Async.run(async, say_hi, async);
//   // }
//   // Async.signal(async);
//   Async.run(async, schedule_tasks, async);
//   // wait for all tasks to finish, closing the threads, clearing the memory.
//   Async.wait(async);
//   fprintf(stderr, "Finish (%lf) ms\n",
//           (((float)clock() / CLOCKS_PER_SEC) - run_start) * 1000);
//   // getchar();
// }

// #include <stdlib.h>
// #include <stdio.h>
// #include <pthread.h>
// #include <signal.h>
// #include <unistd.h>
//
// pthread_t thr;
//
// void on_signal(int sig, siginfo_t* info, void* arg) {
//   fprintf(stderr, "Got signal!\n");
//   fprintf(stderr, "Handler done\n");
// }
//
// void* paused_task(void* arg) {
//   int* flag = arg;
//   struct sigaction on_sig;
//   sigemptyset(&on_sig.sa_mask);
//   sigaddset(&on_sig.sa_mask, SIGCONT);
//   on_sig.sa_sigaction = on_signal;
//   on_sig.sa_flags = SA_SIGINFO;
//   sigaction(SIGCONT, &on_sig, NULL);
//   while (*flag) {
//     fprintf(stderr, "Task thread pausing\n");
//     pause();
//     fprintf(stderr, "Task thread unpaused - sleeping 1...\n");
//   }
//   fprintf(stderr, "Task thread exiting\n");
//   return 0;
// }
//
// int main(int argc, char const* argv[]) {
//   int flag = 1;
//   pthread_create(&thr, NULL, paused_task, &flag);
//   while (getchar() != ' ')
//     pthread_kill(thr, SIGCONT);
//   flag = 0;
//   pthread_kill(thr, SIGCONT);
//   pthread_join(thr, NULL);
//   return 0;
// }
