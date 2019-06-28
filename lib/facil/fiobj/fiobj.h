#ifndef H_FIOBJ_H
#define H_FIOBJ_H

/*
 * For using FIOBJ extensions in a stand-alone omplementation, one translation
 * unit (C file) in MUST define `FIOBJ_EXTERN_COMPLETE` **before** including
 * this file. Also, the `fio-stl.h` file is also required (for the core FIOBJ
 * types).
 */
#ifdef FIOBJ_STANDALONE
#define FIO_FIOBJ
#define FIOBJ_EXTERN
#include "fio-stl.h"
#else
#include <fio.h>
#endif

#include <fiobj_io.h>
#include <fiobj_mustache.h>

#define fiobj_test()                                                           \
  do {                                                                         \
    fiobj_mustache_test();                                                     \
    fiobj_io_test();                                                           \
  } while (0);

#endif
