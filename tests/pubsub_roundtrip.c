#include "facil.h"
#include "redis_engine.h"

static void reporter_subscribe(const pubsub_engine_s *eng, FIOBJ channel,
                               facil_match_fn match) {
  fprintf(stderr, "(%u) + subscribing to %s\n", getpid(),
          fiobj_obj2cstr(channel).data);
  (void)eng;
  (void)match;
}
static void reporter_unsubscribe(const pubsub_engine_s *eng, FIOBJ channel,
                                 facil_match_fn match) {
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

void my_on_message(facil_msg_s *msg) {
  fio_cstr_s s = fiobj_obj2cstr(msg->channel);
  if (FIOBJ_TYPE(msg->msg) == FIOBJ_T_STRING) {
    fprintf(stderr, "Got message from %s: %s\n", s.data,
            fiobj_obj2cstr(msg->msg).data);
  } else {
    fprintf(stderr, "Got message from %s, with subscription %p\n", s.data,
            (void *)fiobj_obj2num(msg->msg));
    facil_unsubscribe((void *)fiobj_obj2num(msg->msg));
  }
}

void perfrom_sub(void *a) {
  if (facil_parent_pid() != getpid()) {
    (void)a;
  } else {
    FIOBJ ch = fiobj_str_new("my channel", 10);
    FIOBJ msg = fiobj_num_new(
        (intptr_t)facil_subscribe(.channel = ch, .on_message = my_on_message,
                                  .udata1 = a, .udata2 = NULL,
                                  .match = FACIL_MATCH_GLOB));
    facil_publish(.channel = ch, .message = ch);
    facil_publish(.channel = ch, .message = msg);
    fiobj_free(msg);
    fiobj_free(ch);
  }
}

void watch_all_messages(facil_msg_s *msg) {
  fio_cstr_s s = fiobj_obj2cstr(msg->channel);
  if (FIOBJ_TYPE(msg->msg) == FIOBJ_T_STRING) {
    fprintf(stderr, "Got message from %s: %s\n", s.data,
            fiobj_obj2cstr(msg->msg).data);
  } else {
    fprintf(stderr, "Got message from %s, with subscription %p\n", s.data,
            (void *)fiobj_obj2num(msg->msg));
  }
}
int main(void) {
  // PUBSUB_DEFAULT_ENGINE = redis_engine_create(.address = "localhost");
  FIOBJ ch = fiobj_str_new("*", 1);
  facil_subscribe(.channel = ch, .on_message = watch_all_messages,
                  .udata1 = NULL, .udata2 = NULL, .match = FACIL_MATCH_GLOB);
  fiobj_free(ch);
  facil_pubsub_attach(&REPORTER);
  facil_run_every(1000, 4, perfrom_sub, NULL, NULL);
  facil_run(.threads = 4, .processes = 4);
  facil_pubsub_detach(&REPORTER);
  return 0;
}
