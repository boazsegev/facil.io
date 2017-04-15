#ifndef H_DEFER_H
/**
A library for deferring execution of code.

Deferred execution can be multi-threaded, although threads aren't managed by the
library.

All deferred execution is shared among the same process and inherited by any
forked process.
*/
#define H_DEFER_H

/** Defer an execution of a function for later. Returns -1 on error.*/
int defer(void (*func)(void *), void *arg);

/** Performs all deferred functions until the queue had been depleated. */
void defer_perform(void);

/** returns true if there are deferred functions waiting for execution. */
int defer_has_queue(void);

#endif
