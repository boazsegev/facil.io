/* *****************************************************************************
Copyright: Boaz Segev, 2019-2020
License: ISC / MIT (choose your license)
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************







        Startup / State Callbacks (fork, start up, idle, etc')







***************************************************************************** */

typedef struct {
  void (*func)(void *);
  void *arg;
} fio___state_task_s;

#ifdef FIO_MEM_REALLOC_
#error FIO_MEM_REALLOC_ should have been undefined
#endif
#define FIO_MALLOC_TMP_USE_SYSTEM
#define FIO_OMAP_NAME          fio_state_tasks
#define FIO_MAP_TYPE           fio___state_task_s
#define FIO_MAP_TYPE_CMP(a, b) (a.func == b.func && a.arg == b.arg)
#include <fio-stl.h>

static fio_state_tasks_s fio_state_tasks_array[FIO_CALL_NEVER];
static fio_lock_i fio_state_tasks_array_lock[FIO_CALL_NEVER + 1];

/** a callback type */
static const char *fio_state_tasks_names[FIO_CALL_NEVER + 1] = {
    [FIO_CALL_ON_INITIALIZE] = "ON_INITIALIZE",
    [FIO_CALL_PRE_START] = "PRE_START",
    [FIO_CALL_BEFORE_FORK] = "BEFORE_FORK",
    [FIO_CALL_AFTER_FORK] = "AFTER_FORK",
    [FIO_CALL_IN_CHILD] = "IN_CHILD",
    [FIO_CALL_IN_MASTER] = "IN_MASTER",
    [FIO_CALL_ON_START] = "ON_START",
    [FIO_CALL_ON_PUBSUB_CONNECT] = "ON_PUBSUB_CONNECT",
    [FIO_CALL_ON_PUBSUB_ERROR] = "ON_PUBSUB_ERROR",
    [FIO_CALL_ON_USR] = "ON_USR",
    [FIO_CALL_ON_IDLE] = "ON_IDLE",
    [FIO_CALL_ON_USR_REVERSE] = "ON_USR_REVERSE",
    [FIO_CALL_ON_SHUTDOWN] = "ON_SHUTDOWN",
    [FIO_CALL_ON_FINISH] = "ON_FINISH",
    [FIO_CALL_ON_PARENT_CRUSH] = "ON_PARENT_CRUSH",
    [FIO_CALL_ON_CHILD_CRUSH] = "ON_CHILD_CRUSH",
    [FIO_CALL_AT_EXIT] = "AT_EXIT",
    [FIO_CALL_NEVER] = "NEVER",
};

FIO_IFUNC uint64_t fio___state_callback_hash(void (*func)(void *), void *arg) {
  uint64_t hash = (uint64_t)(uintptr_t)func + (uint64_t)(uintptr_t)arg;
  hash = fio_risky_ptr((void *)(uintptr_t)hash);
  return hash;
}

FIO_IFUNC void fio_state_callback_clear_all(void) {
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_callback_clear((callback_type_e)i);
  }
}

FIO_IFUNC void fio_state_callback_on_fork(void) {
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_tasks_array_lock[i] = FIO_LOCK_INIT;
  }
}

/** Adds a callback to the list of callbacks to be called for the event. */
void fio_state_callback_add(callback_type_e e,
                            void (*func)(void *),
                            void *arg) {
  if ((uintptr_t)e >= FIO_CALL_NEVER)
    return;
  uint64_t hash = fio___state_callback_hash(func, arg);
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  fio_state_tasks_set(fio_state_tasks_array + (uintptr_t)e,
                      hash,
                      (fio___state_task_s){func, arg},
                      NULL);
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);
  if (e == FIO_CALL_ON_INITIALIZE &&
      fio_state_tasks_array_lock[FIO_CALL_NEVER]) {
    /* initialization tasks already performed, perform this without delay */
    func(arg);
  }
}

/** Removes a callback from the list of callbacks to be called for the event. */
int fio_state_callback_remove(callback_type_e e,
                              void (*func)(void *),
                              void *arg) {
  if ((uintptr_t)e >= FIO_CALL_NEVER)
    return -1;
  int ret;
  uint64_t hash = fio___state_callback_hash(func, arg);
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  ret = fio_state_tasks_remove(fio_state_tasks_array + (uintptr_t)e,
                               hash,
                               (fio___state_task_s){func, arg},
                               NULL);
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);
  return ret;
}

/** Clears all the existing callbacks for the event. */
void fio_state_callback_clear(callback_type_e e) {
  if ((uintptr_t)e >= FIO_CALL_NEVER)
    return;
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  fio_state_tasks_destroy(fio_state_tasks_array + (uintptr_t)e);
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);
}

static void fio_state_callback_force___task(void *fn_p, void *arg) {
  union {
    void *p;
    void (*fn)(void *);
  } u = {.p = fn_p};
  u.fn(arg);
}
/**
 * Forces all the existing callbacks to run, as if the event occurred.
 *
 * Callbacks are called from last to first (last callback executes first).
 *
 * During an event, changes to the callback list are ignored (callbacks can't
 * remove other callbacks for the same event).
 */
void fio_state_callback_force(callback_type_e e) {
  if ((uintptr_t)e >= FIO_CALL_NEVER)
    return;
  fio___state_task_s *ary = NULL;
  size_t len = 0;
  union {
    void *p;
    void (*fn)(void *);
  } u;

  FIO_LOG_DDEBUG2("(%d) Scheduling %s callbacks.",
                  (int)fio_data.pid,
                  fio_state_tasks_names[e]);

  /* copy task queue */
  fio_lock(fio_state_tasks_array_lock + (uintptr_t)e);
  if (fio_state_tasks_array[e].w) {
    ary = fio_malloc(sizeof(*ary) * fio_state_tasks_array[e].w);
    FIO_ASSERT_ALLOC(ary);
    FIO_MAP_EACH(fio_state_tasks, (fio_state_tasks_array + e), pos) {
      if (!pos->hash || !pos->obj.func)
        continue;
      ary[len++] = pos->obj;
    }
  }
  fio_unlock(fio_state_tasks_array_lock + (uintptr_t)e);

  /* perform copied tasks */
  if (e <= FIO_CALL_ON_IDLE) {
    /* perform tasks in order */
    for (size_t i = 0; i < len; ++i) {
      u.fn = ary[i].func;
      fio_queue_push(FIO_QUEUE_USER,
                     fio_state_callback_force___task,
                     u.p,
                     ary[i].arg);
    }
  } else {
    /* perform tasks in reverse */
    while (len--) {
      u.fn = ary[len].func;
      fio_queue_push(FIO_QUEUE_USER,
                     fio_state_callback_force___task,
                     u.p,
                     ary[len].arg);
    }
  }

  /* cleanup */
  fio_free(ary);
}

/* *****************************************************************************
Test state callbacks
***************************************************************************** */
#ifdef TEST

/* State callback test task */
FIO_SFUNC void fio___test__state_callbacks__task(void *udata) {
  struct {
    uint64_t num;
    uint8_t bit;
    uint8_t rev;
  } *p = udata;
  uint64_t mask = (((uint64_t)1ULL << p->bit) - 1);
  if (!p->rev)
    mask = ~mask;
  FIO_ASSERT(!(p->num & mask),
             "fio_state_tasks order error on bit %d (%p | %p) %s",
             (int)p->bit,
             (void *)(uintptr_t)p->num,
             (void *)(uintptr_t)mask,
             (p->rev ? "reversed" : ""));
  p->num |= (uint64_t)1ULL << p->bit;
  p->bit += 1;
}

/* State callback tests */
FIO_SFUNC void FIO_NAME_TEST(io, state)(void) {
  fprintf(stderr, "* testing state callback performance and ordering.\n");
  fio_state_tasks_s fio_state_tasks_array_old[FIO_CALL_NEVER];
  /* store old state */
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_tasks_array_old[i] = fio_state_tasks_array[i];
    fio_state_tasks_array[i] = (fio_state_tasks_s)FIO_MAP_INIT;
  }
  struct {
    uint64_t num;
    uint8_t bit;
    uint8_t rev;
  } data = {0, 0, 0};
  /* state tests for build up tasks */
  for (size_t i = 0; i < 24; ++i) {
    fio_state_callback_add(FIO_CALL_ON_IDLE,
                           fio___test__state_callbacks__task,
                           &data);
  }
  fio_state_callback_force(FIO_CALL_ON_IDLE);
  FIO_ASSERT(data.num = (((uint64_t)1ULL << 24) - 1),
             "fio_state_tasks incomplete");

  /* state tests for clean up tasks */
  data.num = 0;
  data.bit = 0;
  data.rev = 1;
  for (size_t i = 0; i < 24; ++i) {
    fio_state_callback_add(FIO_CALL_ON_SHUTDOWN,
                           fio___test__state_callbacks__task,
                           &data);
  }
  fio_state_callback_force(FIO_CALL_ON_SHUTDOWN);
  FIO_ASSERT(data.num = (((uint64_t)1ULL << 24) - 1),
             "fio_state_tasks incomplete");

  /* restore old state and cleanup */
  for (size_t i = 0; i < FIO_CALL_NEVER; ++i) {
    fio_state_tasks_destroy(fio_state_tasks_array + i);
    fio_state_tasks_array[i] = fio_state_tasks_array_old[i];
  }
}
#endif /* TEST */
