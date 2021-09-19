/* *****************************************************************************
Testing
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */
#ifdef TEST

FIO_SFUNC void fio_test___on_message(fio_msg_s *msg) {
  size_t *i = msg->udata;
  FIO_LOG_DEBUG("fio_test___on_message called");
  FIO_ASSERT(msg->channel.len == 10, "channel name length error");
  FIO_ASSERT(!memcmp("my channel", msg->channel.buf, msg->channel.len),
             "channel name error");
  FIO_ASSERT(msg->message.len == 7, "message length error");
  FIO_ASSERT(!memcmp("payload", msg->message.buf, msg->message.len),
             "message payload error");
  fio_unsubscribe(.channel = {"my channel", 10});
  i[0] += 1;
}

FIO_SFUNC void fio_test___on_unsubscribe(void *udata) {
  size_t *i = udata;
  i[1] += 1;
  FIO_LOG_DEBUG("fio_test___on_unsubscribe called");
}

FIO_SFUNC int fio_test___task_timed(void *u1, void *u2) {
  fio_stop();
  (void)u1;
  (void)u2;
  return -1;
}
FIO_SFUNC void fio_test___task(void *u1, void *u2) {
  fio_run_every(.fn = fio_test___task_timed, .every = 2000, .repetitions = 1);
  (void)u1;
  (void)u2;
}
void fio_test(void) {
  FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  size_t subscription_flags[2] = {0};
  fprintf(stderr, "Testing facil.io IO-Core framework modules.\n");
  FIO_NAME_TEST(io, letter)();
  FIO_NAME_TEST(io, state)();
  fio_subscribe(.channel = {"my channel", 10},
                .on_message = fio_test___on_message,
                .on_unsubscribe = fio_test___on_unsubscribe,
                .udata = subscription_flags);
  fio_publish(.channel = {"my channel", 10}, .message = {"payload", 7});
  // fio_run_every
  fio_defer(fio_test___task, NULL, NULL);
  fio_start(.threads = -2, .workers = 0);
  FIO_ASSERT(subscription_flags[0] == 1,
             "subscription on_message never called");
  FIO_ASSERT(subscription_flags[1] == 1,
             "subscription on_unsubscribe never called");
}
#endif
