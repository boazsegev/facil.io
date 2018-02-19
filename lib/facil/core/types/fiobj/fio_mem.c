/*
Copyright: Boaz Segev, 2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#define MEMORY_BLOCK_SIZE ((uintptr_t)1 << 16)    /* 65,536 bytes == 64Kb*/
#define MEMORY_BLOCK_MASK (MEMORY_BLOCK_SIZE - 1) /* 0xFFFF bytes */
#define MEMORY_BLOCK_SLICES (MEMORY_BLOCK_SIZE >> 4)

#include "spnlock.inc"
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if !FIO_FORCE_MALLOC

#include "fio_llist.h"
#include "fio_mem.h"

/* *****************************************************************************
Memory Copying by 16 byte units
***************************************************************************** */

#if 0 && __SIZEOF_INT128__
static inline void fio_memcpy(__uint128_t *__restrict dest,
                              __uint128_t *__restrict src, size_t units) {
  // while (units >= 4) {
  //   dest[0] = src[0];
  //   dest[1] = src[1];
  //   dest[2] = src[2];
  //   dest[3] = src[3];
  //   dest += 4;
  //   src += 4;
  //   units -= 4;
  // }
  while (units) {
    dest[0] = src[0];
    dest += 1;
    src += 1;
    units -= 1;
  }
}
#else
static inline void fio_memcpy(uint64_t *__restrict dest,
                              uint64_t *__restrict src, size_t units) {
  // while (units >= 2) {
  //   dest[0] = src[0];
  //   dest[1] = src[1];
  //   dest[2] = src[2];
  //   dest[3] = src[3];
  //   dest += 4;
  //   src += 4;
  //   units -= 2;
  // }
  while (units) {
    dest[0] = src[0];
    dest[1] = src[1];
    dest += 2;
    src += 2;
    units -= 1;
  }
}

#endif

/* *****************************************************************************
System Memory wrappers
***************************************************************************** */

/*
 * allocates memory using `mmap`, but enforces block size alignment.
 * requires page aligned `len`.
 *
 * `align_shift` is used to move the memory page alignment to allow for a single
 * page allocation header. align_shift MUST be either 0 (normal) or 1 (single
 * page header). Other values might cause errors.
 */
static inline void *sys_alloc(size_t len, uint8_t align_shift) {
  void *result;
  if (align_shift == 0) {
/* hope for the best? */
#ifdef MAP_ALIGNED
    result = mmap(NULL, len, PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_ALIGNED(MEMORY_BLOCK_SIZE),
                  -1, 0);
#else
    result = mmap(NULL, len, PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, MEMORY_BLOCK_SIZE);
#endif
    if (result == MAP_FAILED)
      return NULL;
    if (!((uintptr_t)result & MEMORY_BLOCK_MASK))
      return result;
    munmap(result, len);
  }
  result =
      mmap(NULL, len + MEMORY_BLOCK_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED)
    return NULL;
  const uintptr_t offset =
      (MEMORY_BLOCK_SIZE - ((uintptr_t)result & MEMORY_BLOCK_MASK)) -
      (align_shift << 12);
  if (offset) {
    munmap(result, offset);
    result = (void *)((uintptr_t)result + offset);
  }
  munmap((void *)((uintptr_t)result + len), MEMORY_BLOCK_SIZE - offset);
  return result;
}

/* frees memory using `munmap`. requires exact, page aligned, `len` */
static void sys_free(void *mem, size_t len) { munmap(mem, len); }

static void *sys_realloc(void *mem, size_t prev_len, size_t new_len) {
  if (new_len > prev_len) {
#ifdef __linux__
    void *result = mremap(mem, prev_len, new_len, 0);
    if (result == MAP_FAILED)
      return NULL;
#else
    void *result = mmap((void *)((uintptr_t)mem + prev_len), new_len - prev_len,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == (void *)((uintptr_t)mem + prev_len)) {
      result = mem;
    } else {
      /* copy and free */
      munmap(result, new_len - prev_len);
      result = sys_alloc(new_len, 1);
      if (!result)
        return NULL;
      fio_memcpy(result, mem, prev_len >> 4);
      // memcpy(result, mem, prev_len);
    }
#endif
    return result;
  }
  if (prev_len + 4096 < new_len)
    munmap((void *)((uintptr_t)mem + new_len), prev_len - new_len);
  return mem;
}

static inline size_t sys_round_size(size_t size) {
  return (size & (~4095)) + (4096 * (!!(size & 4095)));
}

/* *****************************************************************************
Data Types
***************************************************************************** */

/* The basic block header. Starts a 64Kib memory block */
typedef struct block_s {
  uint16_t ref; /* reference count (per memory page) */
  uint16_t pos; /* position into the block */
  uint16_t max; /* available memory count */
  uint16_t pad; /* memory padding */
} block_s;

/* a per-CPU core "arena" for memory allocations  */
typedef struct {
  block_s *block;
  spn_lock_i lock;
} arena_s;

/* The memory allocators persistent state */
static struct {
  size_t active_size;      /* active array size */
  fio_ls_embd_s available; /* free list for memory blocks */
  size_t count;            /* free list counter */
  size_t cores;            /* the number of detected CPU cores*/
  spn_lock_i lock;         /* a global lock */
} memory = {
    .cores = 1,
    .available = FIO_LS_INIT(memory.available),
    .lock = SPN_LOCK_INIT,
};

/* The per-CPU arena array. */
static arena_s *arenas;

/* *****************************************************************************
Per-CPU Arena management
***************************************************************************** */

/* returned a locked arena. Attempts the preffered arena first. */
static inline arena_s *arena_lock(arena_s *preffered) {
  if (preffered && !spn_trylock(&preffered->lock))
    return preffered;
  do {
    arena_s *arena = arenas;
    for (size_t i = 0; i < memory.cores; ++i) {
      if (arena != preffered && !spn_trylock(&arena->lock))
        return arena;
      ++arena;
    }
    preffered = NULL;
    reschedule_thread();
  } while (1);
}

static _Thread_local arena_s *arena_last_used;

static void arena_enter(void) { arena_last_used = arena_lock(arena_last_used); }

static inline void arena_exit(void) { spn_unlock(&arena_last_used->lock); }

/* *****************************************************************************
Block management
***************************************************************************** */

// static inline block_s **block_find(void *mem_) {
//   const uintptr_t mem = (uintptr_t)mem_;
//   block_s *blk = memory.active;
// }

/* intializes the block header for an available block of memory. */
static inline block_s *block_init(void *blk_) {
  block_s *blk = blk_;
  *blk = (block_s){
      .ref = 1,
      .pos = (1 + (sizeof(block_s) >> 4)),
      .max = (MEMORY_BLOCK_SLICES - 1) -
             (sizeof(block_s) >> 4), /* count available units of 16 bytes */
  };
  return blk;
}

/* intializes the block header for an available block of memory. */
static inline void block_free(block_s *blk) {
  if (spn_sub(&blk->ref, 1))
    return;

  if (spn_add(&memory.count, 1) >
      (FIO_MEM_MAX_BLOCKS_PER_CORE * memory.cores)) {
    /* TODO: return memory to the system */
    spn_sub(&memory.count, 1);
    sys_free(blk, MEMORY_BLOCK_SIZE);
    return;
  }
  memset(blk, 0, MEMORY_BLOCK_SIZE);
  spn_lock(&memory.lock);
  fio_ls_embd_push(&memory.available, (fio_ls_embd_s *)blk);
  spn_unlock(&memory.lock);
}

/* intializes the block header for an available block of memory. */
static inline block_s *block_new(void) {
  block_s *blk;
  spn_lock(&memory.lock);
  blk = (block_s *)fio_ls_embd_pop(&memory.available);
  spn_unlock(&memory.lock);
  if (blk) {
    spn_sub(&memory.count, 1);
    ((uintptr_t *)blk)[0] = 0;
    ((uintptr_t *)blk)[1] = 0;
    return block_init(blk);
  }
  /* TODO: collect memory from the system */
  blk = sys_alloc(MEMORY_BLOCK_SIZE, 0);
  if (!blk)
    return NULL;
  block_init(blk);
  return blk;
}

static inline void *block_slice(uint16_t units) {
  block_s *blk = arena_last_used->block;
  if (!blk) {
    blk = block_new();
    arena_last_used->block = blk;
  } else if (blk->pos + units > blk->max) {
    /* not enough memory in the block */
    block_free(blk);
    blk = block_new();
    arena_last_used->block = blk;
  }
  if (!blk) {
    return NULL;
  }
  const void *mem = (void *)((uintptr_t)blk + ((uintptr_t)blk->pos << 4));
  spn_add(&blk->ref, 1);
  blk->pos += units;
  return (void *)mem;
}

static inline void block_slice_free(void *mem) {
  /* locate block boundary */
  block_s *blk = (block_s *)((uintptr_t)mem & (~MEMORY_BLOCK_MASK));
  block_free(blk);
}

/* *****************************************************************************
Library Initialization (initialize arenas and allocate a block for each CPU)
***************************************************************************** */

static void __attribute__((constructor)) fio_mem_init(void) {
  if (arenas)
    return;

#ifdef _SC_NPROCESSORS_ONLN
  ssize_t cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
#else
#warning Dynamic CPU core count is unavailable - assuming 8 cores for memory allocation pools.
  ssize_t cpu_count = 8; /* fallback */
#endif
  memory.cores = cpu_count;
  const uintptr_t arenas_block_size =
      sizeof(uintptr_t) + (sys_round_size(sizeof(*arenas) * cpu_count));
  uintptr_t *arenas_block = sys_alloc(arenas_block_size, 0);
  *arenas_block = arenas_block_size;
  arenas = (void *)(arenas_block + 1);
  arena_s *arena = arenas;
  for (size_t i = 0; i < memory.cores; ++i) {
    arena->block = block_new();
    ++arena;
  }
}

static void __attribute__((destructor)) fio_mem_destroy(void) {
  if (!arenas)
    return;

  arena_s *arena = arenas;
  for (size_t i = 0; i < memory.cores; ++i) {
    if (arena->block)
      block_free(arena->block);
    arena->block = NULL;
    ++arena;
  }
  block_s *b;
  while ((b = (void *)fio_ls_embd_pop(&memory.available))) {
    sys_free(b, MEMORY_BLOCK_SIZE);
  }
  uintptr_t *arenas_block = (void *)arenas;
  --arenas_block;
  sys_free(arenas_block, *arenas_block);
  arenas = NULL;
}

/* *****************************************************************************
Memory allocation / deacclocation API
***************************************************************************** */

void *fio_malloc(size_t size) {
  if (!size)
    return NULL;
  if (size >= (MEMORY_BLOCK_SIZE - 4095)) {
    /* system allocation - must be block aligned */
    size = sys_round_size(size) + 4096;
    void *mem = sys_alloc(size, 1);
    *(uintptr_t *)(mem) = size;
    return (void *)((uintptr_t)mem + 4096);
  }
  /* ceiling for 16 byte alignement, translated to 16 byte units */
  size = (size >> 4) + (!!(size & 15));
  arena_enter();
  void *mem = block_slice(size);
  arena_exit();
  return mem;
}

void *fio_calloc(size_t size, size_t count) {
  size = size * count;
  return fio_malloc(size); // memory is pre-initialized by mmap or pool.
}

void fio_free(void *ptr) {
  if (!ptr)
    return;
  if ((uintptr_t)ptr & MEMORY_BLOCK_MASK) {
    /* allocated within block */
    block_slice_free(ptr);
    return;
  }
  /* big allocation - direct from the system */
  uintptr_t *mem = (uintptr_t *)((uintptr_t)ptr - 4096);
  sys_free(mem, *mem);
}

void *fio_realloc(void *ptr, size_t new_size) {
  if (!ptr)
    return fio_malloc(new_size);
  if ((uintptr_t)ptr & MEMORY_BLOCK_MASK) {
    /* allocated within block - don't even try to expand the allocation */
    const size_t max_old =
        MEMORY_BLOCK_SIZE - ((uintptr_t)ptr & MEMORY_BLOCK_MASK);
    /* ceiling for 16 byte alignement, translated to 16 byte units */
    void *new_mem = fio_malloc(new_size);
    if (!new_mem)
      return NULL;
    new_size = ((new_size >> 4) + (!!(new_size & 15))) << 4;
    // memcpy(new_mem, ptr, (max_old > new_size ? new_size : max_old));
    fio_memcpy(new_mem, ptr, (max_old > new_size ? new_size : max_old) >> 4);
    block_slice_free(ptr);
    return new_mem;
  }
  /* big reallocation - direct from the system */
  uintptr_t *mem = (uintptr_t *)((uintptr_t)ptr - 4096);
  new_size = sys_round_size(new_size) + 4096;
  mem = sys_realloc(mem, (size_t)*mem, new_size);
  if (!mem)
    return NULL;
  *mem = new_size;
  return (void *)((uintptr_t)mem + 4096);
}

/* *****************************************************************************
FIO_OVERRIDE_MALLOC - override glibc / library malloc
***************************************************************************** */
#if FIO_OVERRIDE_MALLOC
void *malloc(size_t size) { return fio_malloc(size); }
void *calloc(size_t size, size_t count) { return fio_calloc(size, count); }
void free(void *ptr) { fio_free(ptr); }
void *realloc(void *ptr, size_t new_size) { return fio_realloc(ptr, new_size); }
#endif

/* *****************************************************************************
FIO_FORCE_MALLOC - use glibc / library malloc
***************************************************************************** */
#else

void *fio_malloc(size_t size) { return malloc(size); }

void *fio_calloc(size_t size, size_t count) { return calloc(size, count); }

void fio_free(void *ptr) { free(ptr); }

void *fio_realloc(void *ptr, size_t new_size) { return realloc(ptr, new_size); }

#endif

/* *****************************************************************************
Some Tests
***************************************************************************** */

#if DEBUG && !FIO_FORCE_MALLOC

void fio_malloc_test(void) {
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "\nTesting failed.\n");                                    \
    exit(-1);                                                                  \
  }

  fprintf(stderr, "=== Testing facil.io memory allocator's system calls\n");
  char *mem = sys_alloc(MEMORY_BLOCK_SIZE, 0);
  TEST_ASSERT(!((uintptr_t)mem & MEMORY_BLOCK_MASK),
              "Memory allocation not aligned to MEMORY_BLOCK_SIZE!");
  mem[0] = 'a';
  mem[MEMORY_BLOCK_SIZE - 1] = 'z';
  fprintf(stderr, "* Testing reallocation from %p\n", (void *)mem);
  char *mem2 = sys_realloc(mem, MEMORY_BLOCK_SIZE, MEMORY_BLOCK_SIZE * 2);
  if (mem == mem2)
    fprintf(stderr, "* Performed system realloc without copy :-)\n");
  TEST_ASSERT(mem2[0] = 'a' && mem2[MEMORY_BLOCK_SIZE - 1] == 'z',
              "Reaclloc data was lost!");
  sys_free(mem2, MEMORY_BLOCK_SIZE * 2);
  mem = sys_alloc(MEMORY_BLOCK_SIZE, 1);
  TEST_ASSERT(
      ((uintptr_t)mem & MEMORY_BLOCK_MASK) == (MEMORY_BLOCK_SIZE - 4096),
      "Memory allocation not aligned to a page behind MEMORY_BLOCK_SIZE! (%p)",
      (void *)mem);
  sys_free(mem, MEMORY_BLOCK_SIZE);
  fprintf(stderr, "=== Testing facil.io memory allocator's internal data.\n");
  TEST_ASSERT(arenas, "Missing arena data - library not initialized!");
  mem = fio_malloc(1);
  TEST_ASSERT(mem, "fio_malloc failed to allocate memory!\n");
  TEST_ASSERT(!((uintptr_t)mem & 15), "fio_malloc memory not aligned!\n");
  TEST_ASSERT(((uintptr_t)mem & MEMORY_BLOCK_MASK),
              "fio_malloc memory divisable by block size!\n");
  mem[0] = 'a';
  TEST_ASSERT(mem[0] == 'a', "allocate memory wasn't written to!\n");
  mem = fio_realloc(mem, 1);
  TEST_ASSERT(mem[0] == 'a', "fio_realloc memory wasn't copied!\n");
  block_s *b = arena_last_used->block;
  size_t count = 2;
  do {
    mem2 = mem;
    mem = fio_malloc(1);
    fio_free(mem2); /* make sure we hold on to the block, so it rotates */
    TEST_ASSERT(mem, "fio_malloc failed to allocate memory!\n");
    TEST_ASSERT(!((uintptr_t)mem & 15),
                "fio_malloc memory not aligned at allocation #%zu!\n", count);
    TEST_ASSERT(((uintptr_t)mem & MEMORY_BLOCK_MASK),
                "fio_malloc memory divisable by block size!\n");
    mem[0] = 'a';
    ++count;
  } while (arena_last_used->block == b);
  fio_free(mem);
  fprintf(
      stderr,
      "* Performed %zu allocation out of expected %zu allocations per block.\n",
      count, (size_t)((MEMORY_BLOCK_SLICES - 1) - (sizeof(block_s) >> 4)));
  TEST_ASSERT(fio_ls_embd_any(&memory.available),
              "memory pool empty (memory block wasn't freed)!\n");
  TEST_ASSERT(memory.count, "memory.count == 0 (memory block not counted)!\n");
  mem = fio_calloc(4096, 7);
  TEST_ASSERT(((uintptr_t)mem & MEMORY_BLOCK_MASK),
              "fio_calloc (7 pages) memory divisable by block size!\n");
  mem2 = fio_malloc(1);
  mem2[0] = 'a';
  fio_free(mem2);
  for (int i = 0; i < (4096 * 7); ++i) {
    TEST_ASSERT(mem[i] == 0,
                "calloc returned memory that wasn't initialized?!\n");
  }
  fio_free(mem);

  mem = fio_malloc(MEMORY_BLOCK_SIZE);
  TEST_ASSERT(!((uintptr_t)mem & MEMORY_BLOCK_MASK),
              "fio_malloc (big) memory isn't aligned!\n");
  mem = fio_realloc(mem, MEMORY_BLOCK_SIZE * 2);
  fio_free(mem);
  TEST_ASSERT(!((uintptr_t)mem & MEMORY_BLOCK_MASK),
              "fio_realloc (big) memory isn't aligned!\n");

  fprintf(stderr, "* passed.\n");
}

#else

void fio_malloc_test(void) {}

#endif
