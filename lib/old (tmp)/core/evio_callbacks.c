/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "evio.h"

#include <stdio.h>
#include <stdlib.h>

/* *****************************************************************************
Callbacks - weak versions to be overridden.
***************************************************************************** */
#pragma weak evio_on_data
void __attribute__((weak)) evio_on_data(void *arg) { (void)arg; }
#pragma weak evio_on_ready
void __attribute__((weak)) evio_on_ready(void *arg) { (void)arg; }
#pragma weak evio_on_error
void __attribute__((weak)) evio_on_error(void *arg) { (void)arg; }
#pragma weak evio_on_close
void __attribute__((weak)) evio_on_close(void *arg) { (void)arg; }
