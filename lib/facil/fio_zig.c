#include <fiobject.h>

void fiobj_free_wrapped(FIOBJ o) {
  if (!FIOBJ_IS_ALLOCATED(o))
    return;
  if (fiobj_ref_dec(o))
    return;
  if (FIOBJECT2VTBL(o)->each && FIOBJECT2VTBL(o)->count(o))
    fiobj_free_complex_object(o);
  else
    FIOBJECT2VTBL(o)->dealloc(o, NULL, NULL);
}
