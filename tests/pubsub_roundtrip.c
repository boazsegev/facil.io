#include "facil.h"
#include "pubsub.h"
#include "redis_engine.h"

static void reporter_subscribe(const pubsub_engine_s *eng, FIOBJ channel,
                               pubsub_match_fn match) {
  fprintf(stderr, "(%u) + subscribing to %s\n", getpid(),
          fiobj_obj2cstr(channel).data);
  (void)eng;
  (void)match;
}
static void reporter_unsubscribe(const pubsub_engine_s *eng, FIOBJ channel,
                                 pubsub_match_fn match) {
  fprintf(stderr, "(%u) - unsubscribing to %s\n", getpid(),
          fiobj_obj2cstr(channel).data);
  (void)eng;
  (void)match;
}
static int reporter_publish(const pubsub_engine_s *eng, FIOBJ channel,
                            FIOBJ msg) {
  fprintf(stderr, "(%u) publishing to %s\n", getpid(),
          fiobj_obj2cstr(channel).data);
  (void)eng;
  (void)msg;
  return 0;
}

pubsub_engine_s REPORTER = {
    .subscribe = reporter_subscribe,
    .unsubscribe = reporter_unsubscribe,
    .publish = reporter_publish,
};

void my_on_message(pubsub_message_s *msg) {
  fio_cstr_s s = fiobj_obj2cstr(msg->channel);
  if (FIOBJ_TYPE(msg->message) == FIOBJ_T_STRING) {
    fprintf(stderr, "Got message from %s: %s\n", s.data,
            fiobj_obj2cstr(msg->message).data);
  } else {
    fprintf(stderr, "Got message from %s, with subscription %p\n", s.data,
            (void *)fiobj_obj2num(msg->message));
    pubsub_sub_pt sub =
        pubsub_find_sub(.channel = msg->channel, .on_message = my_on_message,
                        .udata1 = msg->udata1, .udata2 = msg->udata2);
    pubsub_unsubscribe(sub);
  }
}

void perfrom_sub(void *a) {
  if (facil_parent_pid() != getpid()) {
    (void)a;
  } else {
    FIOBJ ch = fiobj_str_new("my channel", 10);
    FIOBJ msg = fiobj_num_new(
        (intptr_t)pubsub_subscribe(.channel = ch, .on_message = my_on_message,
                                   .udata1 = a, .udata2 = NULL,
                                   .match = PUBSUB_MATCH_GLOB));
    pubsub_publish(.channel = ch, .message = ch);
    pubsub_publish(.channel = ch, .message = msg);
    fiobj_free(msg);
    fiobj_free(ch);
  }
}

void watch_all_messages(pubsub_message_s *msg) {
  fio_cstr_s s = fiobj_obj2cstr(msg->channel);
  if (FIOBJ_TYPE(msg->message) == FIOBJ_T_STRING) {
    fprintf(stderr, "Got message from %s: %s\n", s.data,
            fiobj_obj2cstr(msg->message).data);
  } else {
    fprintf(stderr, "Got message from %s, with subscription %p\n", s.data,
            (void *)fiobj_obj2num(msg->message));
  }
}
int main(void) {
  // PUBSUB_DEFAULT_ENGINE = redis_engine_create(.address = "localhost");
  FIOBJ ch = fiobj_str_new("*", 1);
  pubsub_subscribe(.channel = ch, .on_message = watch_all_messages,
                   .udata1 = NULL, .udata2 = NULL, .match = PUBSUB_MATCH_GLOB);
  fiobj_free(ch);
  pubsub_engine_register(&REPORTER);
  facil_run_every(1000, 4, perfrom_sub, NULL, NULL);
  facil_run(.threads = 4, .processes = 4);
  pubsub_engine_deregister(&REPORTER);
  return 0;
}
