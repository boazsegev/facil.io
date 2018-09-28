#ifndef H_FIOBJ_MUSTACHE_H
#define H_FIOBJ_MUSTACHE_H

#include <fiobject.h>

#include <mustache_parser.h>

/**
 * Loads a mustache template, converting it into an opaque instruction array.
 *
 * Returns a pointer to the instruction array.
 *
 * The `filename` argument should contain the template's file name.
 */
mustache_s *fiobj_mustache_load(fio_str_info_s filename);

/** Free the mustache template */
void fiobj_mustache_free(mustache_s *mustache);

/**
 * Creates a FIOBJ String containing the rendered template using the information
 * in the `data` object.
 *
 * Returns FIOBJ_INVALID if an error occured and a FIOBJ String on success.
 */
FIOBJ fiobj_mustache_build(mustache_s *mustache, FIOBJ data);

/**
 * Renders a template into an existing FIOBJ String (`dest`'s end), using the
 * information in the `data` object.
 *
 * Returns FIOBJ_INVALID if an error occured and a FIOBJ String on success.
 */
FIOBJ fiobj_mustache_build2(FIOBJ dest, mustache_s *mustache, FIOBJ data);

#if DEBUG
void fiobj_mustache_test(void);
#endif

#endif /* H_FIOBJ_MUSTACHE_H */
