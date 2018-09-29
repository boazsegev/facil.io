---
title: facil.io - FIOBJ Primitives
sidebar: 0.7.x/_sidebar.md
---
# {{{title}}}

The following Primitive types are defined:

#### `fiobj_null`

```c
FIOBJ fiobj_null(void);
```

A `null` primitive object.

Although primitive objects aren't dynamically allocated, it's good practice to call `fiobj_free` to "deallocate" them all the same.

#### `fiobj_true`

```c
FIOBJ fiobj_true(void);
```

A `null` primitive object.

Although primitive objects aren't dynamically allocated, it's good practice to call `fiobj_free` to "deallocate" them all the same.

#### `fiobj_false`

```c
FIOBJ fiobj_false(void);
```

A `null` primitive object.

Although primitive objects aren't dynamically allocated, it's good practice to call `fiobj_free` to "deallocate" them all the same.
