/*
                An implementation of dynamic memory allocator
                     J. Iverson <jiverson@cs.umn.edu>
                       Thu Mar 12 14:17:25 CDT 2015

  ...
*/

/*****************************************************************************
From Wikipedia
While the x86 architecture originally did not require aligned memory
access and still works without it, SSE2 instructions on x86 CPUs do
require the data to be 128-bit (16-byte) aligned and there can be
substantial performance advantages from using aligned data on these
architectures. However, there are also instructions for unaligned
access such as MOVDQU.

From C Standard
4.10.3 Memory management functions

   The order and contiguity of storage allocated by successive calls
to the calloc , malloc , and realloc functions is unspecified.  The
pointer returned if the allocation succeeds is suitably aligned so
that it may be assigned to a pointer to any type of object and then
used to access such an object in the space allocated (until the space
is explicitly freed or reallocated).  Each such allocation shall yield
a pointer to an object disjoint from any other object.  The pointer
returned points to the start (lowest byte address) of the allocated
space.  If the space cannot be allocated, a null pointer is returned.
If the size of the space requested is zero, the behavior is
implementation-defined; the value returned shall be either a null
pointer or a unique pointer.  The value of a pointer that refers to
freed space is indeterminate.


4.10.3.1 The calloc function

Synopsis

         #include <stdlib.h>
         void *calloc(size_t nmemb, size_t size);

Description

   The calloc function allocates space for an array of nmemb objects,
each of whose size is size .  The space is initialized to all bits
zero.

Returns

   The calloc function returns either a null pointer or a pointer to
the allocated space.


4.10.3.2 The free function


Synopsis

         #include <stdlib.h>
         void free(void *ptr);

Description

   The free function causes the space pointed to by ptr to be
deallocated, that is, made available for further allocation.  If ptr
is a null pointer, no action occurs.  Otherwise, if the argument does
not match a pointer earlier returned by the calloc , malloc , or
realloc function, or if the space has been deallocated by a call to
free or realloc , the behavior is undefined.

Returns

   The free function returns no value.


4.10.3.3 The malloc function

Synopsis

         #include <stdlib.h>
         void *malloc(size_t size);

Description

   The malloc function allocates space for an object whose size is
specified by size and whose value is indeterminate.

Returns

   The malloc function returns either a null pointer or a pointer to
the allocated space.


4.10.3.4 The realloc function

Synopsis

         #include <stdlib.h>
         void *realloc(void *ptr, size_t size);

Description

   The realloc function changes the size of the object pointed to by
ptr to the size specified by size .  The contents of the object shall
be unchanged up to the lesser of the new and old sizes.  If the new
size is larger, the value of the newly allocated portion of the object
is indeterminate.  If ptr is a null pointer, the realloc function
behaves like the malloc function for the specified size.  Otherwise,
if ptr does not match a pointer earlier returned by the calloc ,
malloc , or realloc function, or if the space has been deallocated by
a call to the free or realloc function, the behavior is undefined.  If
the space cannot be allocated, the object pointed to by ptr is
unchanged.  If size is zero and ptr is not a null pointer, the object
it points to is freed.

Returns

   The realloc function returns either a null pointer or a pointer to
the possibly moved allocated space.
*****************************************************************************/


#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200112L
#elif _POSIX_C_SOURCE < 200112L
# undef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200112L
#endif
#include <stdlib.h> /* posix_memalign, size_t */
#undef _POSIX_C_SOURCE

#include <assert.h>   /* assert */
#include <limits.h>   /* CHAR_BIT */
#include <stdint.h>   /* uint*_t */
#include <string.h>   /* memset */

/* This alignment should be a power of 2. */
#ifdef MEMORY_ALLOCATION_ALIGNMENT
# define KL_MEMORY_ALLOCATION_ALIGNMENT MEMORY_ALLOCATION_ALIGNMENT
#else
# define KL_MEMORY_ALLOCATION_ALIGNMENT 16
#endif

#ifndef KL_EXPORT
# define KL_EXPORT extern
#endif


#define KL_DEBUG 0
#if defined(KL_DEBUG) && KL_DEBUG > 0
# include <stdio.h>
# define KL_PRINT(...) printf(__VA_ARGS__); fflush(stdout);
#else
# define KL_PRINT(...)
#endif
#include <stdio.h>


/****************************************************************************/
/*
  Memory block:

    | size_t | size_t | `memory chunks' |
    +--------+--------+-----------------+

    Memory blocks must be allocated so that their starting address is aligned
    to their size, which is a power of 2.  This allows for a memory chunk
    carved out of a block to be traced back to its containing block by masking
    away all bits according to the block size.


  Memory chunk:

      ACTIVE  | size_t | `used memory' |
              +--------+---------------+

    INACTIVE  | size_t | void * | void * | `unused memory' |
              +--------+--------+--------+-----------------+

    When a memory chunk is inactive, then the two `void *' shall be used for
    the pointers in the doubly-linked list in the free chunk data structure.


  Abbriviations:

    A = active chunk
    B = block
    C = chunk
    D = doubly linked-list node
    F = footer size
    I = inactive chunk
    N = number
    P = pointer
    S = size
    T = split chunk
 */
/****************************************************************************/
#define KL_B2S(B)    (*(size_t*)(B))
#define KL_B2N(B)    (*(size_t*)((uintptr_t)(B)+sizeof(size_t)))
#define KL_B2C(B)    (void*)((uintptr_t)(B)+2*sizeof(size_t))

#define KL_C2S(C)    (((kl_chunk_header_t*)(C))->size)
#define KL_C2D(C)    (&(((kl_chunk_header_t*)(C))->node))

#define KL_C2P(C)    (void*)((uintptr_t)(C)+KL_CHUNK_SIZE_HEADER)
#define KL_C2B(C)    (void*)((uintptr_t)(C)&(~(KL_BLOCK_SIZE_ALIGNED-1)))

#define KL_C2T(C)    (void*)((uintptr_t)(C)+KL_C2S(C))
#define KL_C2F(C)    (*(size_t*)((uintptr_t)(C)+KL_C2S(C)-sizeof(size_t)))

#define KL_D2C(D)    (void*)((uintptr_t)(D)-sizeof(size_t))
#define KL_D2S(D)    (*(size_t*)((uintptr_t)(D)-sizeof(size_t)))
#define KL_P2C(P)    (void*)((uintptr_t)(P)-sizeof(size_t))
#define KL_P2S(P)    (*(size_t*)((uintptr_t)(P)-KL_CHUNK_SIZE_HEADER))
#define KL_T2C(T)    (void*)((uintptr_t)(T)-(*(size_t*)((uintptr_t)(T)-sizeof(size_t))))
#define KL_CS2T(C,S) (void*)((uintptr_t)(C)+KL_CHUNK_SIZE_ALIGNED(S))

#define KL_ISFIRST(C) ((C) == KL_B2C(KL_C2B(C)))
#define KL_ISLAST(C)  \
  ((uintptr_t)(C)+KL_C2S(C) == (uintptr_t)KL_C2B(C)+KL_B2S(KL_C2B(C)))
#define KL_INUSE(C)   (0 == KL_C2F(C))


/****************************************************************************/
/* Align an unsigned integer value to a power supplied power of 2 */
/****************************************************************************/
#define KL_CHUNK_SIZE_MINIMUM (2*sizeof(size_t)+2*sizeof(void*))
#define KL_BLOCK_SIZE_ALIGNED 65536

#define KL_BLOCK_META_ALIGNED \
  KL_SIZE_ALIGNED(2*sizeof(size_t), KL_MEMORY_ALLOCATION_ALIGNMENT)

#define KL_CHUNK_SIZE_HEADER \
  KL_SIZE_ALIGNED(sizeof(kl_chunk_header_t), KL_MEMORY_ALLOCATION_ALIGNMENT)

#define KL_SIZE_ALIGNED(S,A) \
  (assert(0 == ((A)&((A)-1))), (((S)+((A)-1))&(~(((A)-1)))))

#define KL_CHUNK_SIZE_ALIGNED(S) \
  KL_SIZE_ALIGNED(2*sizeof(size_t)+(S), KL_MEMORY_ALLOCATION_ALIGNMENT)


/****************************************************************************/
/* Sanity checks */
/****************************************************************************/
/* Sanity check to make sure that the returned pointer is valid in terms of
 * alignment and points to a valid piece of memory. */
#define KL_SANITY_VALID_POINTER(P)                                          \
do {                                                                        \
  assert(0 == ((uintptr_t)(P)&(KL_MEMORY_ALLOCATION_ALIGNMENT-1)));         \
  assert((uintptr_t)(P)+KL_P2S(P) <=                                        \
    (uintptr_t)KL_C2B(KL_P2C(P))+KL_B2S(KL_C2B(KL_P2C(P))));                \
} while (0)



/****************************************************************************/
/* Base 2 integer logarithm */
/****************************************************************************/
#if !defined(__INTEL_COMPILER) && !defined(__GNUC__)
  static int kl_builtin_clzl(size_t v) {
    /* result will be nonsense if v is 0 */
    int i;
    for (i=sizeof(size_t)*CHAR_BIT-1; i>=0; --i) {
      if (v & ((size_t)1 << i))
        break;
    }
    return sizeof(size_t)*CHAR_BIT-i-1;
  }
  #define kl_clz(V) kl_builtin_clzl(V)
#else
  #define kl_clz(V) __builtin_clzl(V)
#endif

#define KLLOG2(V) (sizeof(size_t)*CHAR_BIT-1-kl_clz(V))


/****************************************************************************/
/* Lookup tables to convert between size and bin number */
/****************************************************************************/
#define KLNUMBIN   1576
#define KLSMALLBIN 1532
#define KLISSMALLBIN(B) (B < KLSMALLBIN)

#define KLSIZE2BIN(S)                                                       \
  (                                                                         \
    (1==(S))                                                                \
      ? (size_t)0                                                           \
      : (KLLOG2((S)-1)<20)                                                  \
        ? log2off[KLLOG2((S)-1)]+(S)/log2size[KLLOG2((S)-1)]                \
        : log2off[KLLOG2((S)-1)]                                            \
  )

static size_t log2size[64]=
{
  8, 8, 8, 8, 8, 8, 16, 16, 32, 32, 64, 64, 128, 128, 256, 256, 512, 512,
  1024, 1024, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static size_t log2off[64]=
{
  0, 0, 0, 0, 0, 0, 8, 8, 20, 20, 44, 44, 92, 92, 188, 188, 380, 380, 764,
  764, 1532, 1533, 1534, 1535, 1536, 1537, 1538, 1539, 1540, 1541, 1542, 1543,
  1544, 1545, 1546, 1547, 1548, 1549, 1550, 1551, 1552, 1553, 1554, 1555,
  1556, 1557, 1558, 1559, 1560, 1561, 1562, 1563, 1564, 1565, 1566, 1567,
  1568, 1569, 1570, 1571, 1572, 1573, 1574, 1575
};


/****************************************************************************/
/* System memory allocation related macros */
/****************************************************************************/
#define KL_SYS_ALLOC_FAIL                -1
#define KL_CALL_SYS_ALLOC_ALIGNED(P,A,S) posix_memalign(&(P),A,S)
#define KL_CALL_SYS_FREE(P,S)            free(P)


/****************************************************************************/
/****************************************************************************/
/* Free chunk data structure API */
/****************************************************************************/
/****************************************************************************/

/****************************************************************************/
/* Free chunk data structure node */
/****************************************************************************/
typedef struct kl_bin_node
{
  struct kl_bin_node * p; /* previous node */
  struct kl_bin_node * n; /* next node */
} kl_bin_node_t;


/****************************************************************************/
/* Free chunk data structure */
/****************************************************************************/
typedef struct kl_bin
{
  int init;
  struct kl_bin_node * bin[KLNUMBIN];
} kl_bin_t;

typedef struct kl_chunk_header
{
  size_t size;
  struct kl_bin_node node;
} kl_chunk_header_t;


/****************************************************************************/
/* Initialize free chunk data structure */
/****************************************************************************/
static int
kl_bin_init(kl_bin_t * const bin)
{
  int i;

  for (i=0; i<KLNUMBIN; ++i)
    bin->bin[i] = NULL;

  bin->init = 1;

  return 0;
}


/****************************************************************************/
/* Add a node to a free chunk data structure */
/****************************************************************************/
static int
kl_bin_ad(kl_bin_t * const bin, void * const chunk)
{
  size_t bidx = KLSIZE2BIN(KL_C2S(chunk));
  kl_bin_node_t * p, * n, * node = KL_C2D(chunk);

  /* Treat fixed size bins and large bins differently */
  if (KLISSMALLBIN(bidx)) {
    KL_PRINT("klinfo: adding available fixed size memory chunk\n");
    KL_PRINT("klinfo:   block address: %p\n", KL_C2B(chunk));
    KL_PRINT("klinfo:   block size:    %zu\n", KL_B2S(KL_C2B(chunk)));
    KL_PRINT("klinfo:   chunk address: %p\n", chunk);
    KL_PRINT("klinfo:   chunk size:    %zu\n", KL_D2S(node));
    KL_PRINT("klinfo:   node address:  %p\n", (void*)node);
    KL_PRINT("klinfo:   bin index:     %zu\n", bidx);

    /* Sanity check to make sure that bin->bin[bidx] and node are well formed.
     * */
    assert(NULL == bin->bin[bidx] || NULL == bin->bin[bidx]->p);
    assert(node != bin->bin[bidx]);
    assert(NULL == node->p);
    assert(NULL == node->n);
    assert(KL_C2S(chunk) == KL_C2F(chunk));

    /* Prepend n to front of bin[bidx] linked-list. */
    node->p = NULL;
    node->n = bin->bin[bidx];
    if (NULL != bin->bin[bidx]) {
      assert(NULL == bin->bin[bidx]->p);
      bin->bin[bidx]->p = node;
    }
    bin->bin[bidx] = node;

    assert(NULL == bin->bin[bidx]->p);
  }
  else {
    assert(0);
    KL_PRINT("klinfo: adding available variable size memory chunk\n");
    KL_PRINT("klinfo:   chunk address: %p\n", (void*)node);
    KL_PRINT("klinfo:   chunk size:    %zu\n", KL_D2S(node));
    KL_PRINT("klinfo:   bin index:    %zu\n", bidx);

    /* This will keep large buckets sorted. */
    n = bin->bin[bidx];
    p = NULL;

    while (NULL != n && KL_D2S(n) < KL_C2S(chunk)) {
      p = n;
      n = n->n;
    }

    if (NULL != n) {
      /* insert internally */
      node->p = n->p;
      node->n = n;
      if (NULL == n->p)
        bin->bin[bidx] = node;
      else
        n->p->n = node;
      n->p = node;
    }
    else if (NULL != p) {
      /* insert at the end */
      p->n = node;
      node->p = p;
      node->n = NULL;
    }
    else {
      /* insert at the beginning */
      bin->bin[bidx] = node;
      node->p = NULL;
      node->n = NULL;
    }
  }

  KL_PRINT("klinfo:\n");

  return 0;
}


/****************************************************************************/
/* Remove a node from a free chunk data structure */
/****************************************************************************/
static int
kl_bin_rm(kl_bin_t * const bin, void * const chunk)
{
  size_t bidx = KLSIZE2BIN(KL_C2S(chunk));
  kl_bin_node_t * node = KL_C2D(chunk);

  KL_PRINT("klinfo: removing memory chunk\n");
  KL_PRINT("klinfo:   block address: %p\n", KL_C2B(chunk));
  KL_PRINT("klinfo:   block size:    %zu\n", KL_B2S(KL_C2B(chunk)));
  KL_PRINT("klinfo:   chunk address: %p\n", chunk);
  KL_PRINT("klinfo:   chunk size:    %zu\n", KL_D2S(node));
  KL_PRINT("klinfo:   bin index:     %zu\n", bidx);
  KL_PRINT("klinfo:   node address:  %p\n", (void*)node);
  KL_PRINT("klinfo:   node previous: %p\n", (void*)node->p);
  KL_PRINT("klinfo:   node next:     %p\n", (void*)node->n);

  assert(NULL != node->p || NULL != node->n || bin->bin[bidx] == node);

  /* Fixed and variable sized bins are treated the same, since removing a node
   * from a variable sized bin will not cause it to become unsorted. */
  if (NULL == node->p)
    bin->bin[bidx] = node->n;
  else
    node->p->n = node->n;
  if (NULL != node->n)
    node->n->p = node->p;
  node->n = NULL;
  node->p = NULL;

  assert(NULL == bin->bin[bidx] || NULL == bin->bin[bidx]->p);

  return 0;
}


/****************************************************************************/
/* Find the bin with the smallest size >= size parameter */
/****************************************************************************/
static void *
kl_bin_find(kl_bin_t * const bin, size_t const size)
{
  size_t bidx = KLSIZE2BIN(size);
  kl_bin_node_t * n;

  if (KLISSMALLBIN(bidx)) {
    KL_PRINT("klinfo: searching for available fixed size memory chunk\n");
    KL_PRINT("klinfo:   request size:    %zu\n", size);
    KL_PRINT("klinfo:   bin index:       %zu\n", bidx);
    KL_PRINT("klinfo:   head of bin:     %p\n", (void*)bin->bin[bidx]);

    /* Find first bin with a node. */
    n = bin->bin[bidx];
    while (NULL == n && bidx < KLSMALLBIN)
      n = bin->bin[++bidx];

    KL_PRINT("klinfo:   final bin index: %zu\n", bidx);
    KL_PRINT("klinfo:\n");

    /* Remove head of bin[bidx]. */
    if (NULL != n) {
      assert(NULL != bin->bin[bidx]);
      assert(NULL == n->p);
      bin->bin[bidx] = n->n;
      if (NULL != n->n) {
        n->n->p = NULL;
        n->n = NULL;
      }
    }

    assert(NULL == bin->bin[bidx] || NULL == bin->bin[bidx]->p);
  }
  else {
    KL_PRINT("klinfo: searching for available variable size memory chunk\n");
    KL_PRINT("klinfo:   request size: %zu\n", size);

    /* Find first bin with a node. */
    n = bin->bin[bidx];
    while (NULL == n && bidx < KLNUMBIN)
      n = bin->bin[++bidx];

    assert(NULL == n);

    /* Find first node in bin[bidx] with size >= size parameter. */
    while (NULL != n && KL_D2S(n) < size)
      n = n->n;

    /* Remove n from bin[bidx]. */
    if (NULL != n) {
      if (NULL == n->p) {
        bin->bin[bidx] = n->n;
        if (NULL != n->n)
          n->n->p = NULL;
      }
      else {
        n->p->n = n->n;
        if (NULL != n->n)
          n->n->p = n->p;
      }
      n->p = NULL;
      n->n = NULL;
    }
  }

  if (NULL != n) {
    KL_PRINT("klinfo:   available chunk found\n");
    KL_PRINT("klinfo:     chunk address: %p\n", (void*)n);
  }
  else {
    KL_PRINT("klinfo:   no available chunk found\n");
  }

  KL_PRINT("klinfo:\n");

  return (NULL == n) ? NULL : KL_D2C(n);
}


/****************************************************************************/
/****************************************************************************/
/* KL API */
/****************************************************************************/
/****************************************************************************/

/****************************************************************************/
/* Free chunk data structure */
/****************************************************************************/
static kl_bin_t bin={.init=0};


/****************************************************************************/
/* Initialize static variables and data structures */
/****************************************************************************/
#define KL_INIT_CHECK                                                       \
do {                                                                        \
  if (0 == bin.init)                                                        \
    kl_bin_init(&bin);                                                      \
} while (0)


/****************************************************************************/
/* Allocate size bytes of memory */
/****************************************************************************/
KL_EXPORT void *
klmalloc(size_t const size)
{
  int ret;
  size_t bsize;
  void * block, * chunk;

  KL_INIT_CHECK;

  KL_PRINT("klinfo: klmalloc beg\n");

  /* Try to get a previously allocated block of memory.  If a chunk is found,
   * it will be removed from the free chunk data structure by the kl_bin_find
   * function. */
  if (NULL == (chunk=kl_bin_find(&bin, KL_CHUNK_SIZE_ALIGNED(size)))) {
    /* If no previously allocated block of memory can support this allocation,
     * then allocate a new block.  If requested size is less than
     * KL_BLOCK_SIZE_ALIGNED-KL_BLOCK_META_ALIGNED, then allocate a new block.
     * Otherwise, directly allocate the required amount of memory. */
    if (KL_CHUNK_SIZE_ALIGNED(size) <= KL_BLOCK_SIZE_ALIGNED-KL_BLOCK_META_ALIGNED) {
      KL_PRINT("klinfo: allocating a new fixed size block of memory\n");
      bsize = KL_BLOCK_SIZE_ALIGNED;
    }
    else {
      KL_PRINT("klinfo: allocating a new variable size block of memory\n");
      bsize = KL_BLOCK_META_ALIGNED+KL_CHUNK_SIZE_ALIGNED(size);
    }

    /* Get system memory */
    ret = KL_CALL_SYS_ALLOC_ALIGNED(block, KL_BLOCK_SIZE_ALIGNED, bsize);
    if (KL_SYS_ALLOC_FAIL == ret)
      return NULL;

    /* Zero memory */
    memset(block, 0, bsize);

    KL_PRINT("klinfo:   block start: %p\n", (void*)block);
    KL_PRINT("klinfo:   size start:  %p\n", (void*)&(KL_B2S(block)));
    KL_PRINT("klinfo:   count start: %p\n", (void*)&(KL_B2N(block)));
    KL_PRINT("klinfo:   block size:  %zu\n", bsize);
    KL_PRINT("klinfo:\n");

    /* Set block size */
    KL_B2S(block) = bsize;

    /* Set the chunk to be returned */
    chunk = KL_B2C(block);

    /* Temporarily set chunk size */
    KL_C2S(chunk) = bsize-KL_BLOCK_META_ALIGNED;
  }

  /* Conceptually break mem into two chunks:
   *   chunk[0..KL_CHUNK_SIZE_ALIGNED(size)-1],
   *   chunk[KL_CHUNK_SIZE_ALIGNED(size)..KL_C2S(chunk)-1]
   * Add the second chunk to free chunk data structure, when applicable. */
  if (KL_C2S(chunk) > KL_CHUNK_SIZE_MINIMUM &&
      KL_CHUNK_SIZE_ALIGNED(size) < KL_C2S(chunk)-KL_CHUNK_SIZE_MINIMUM)
  {
    KL_PRINT("klinfo:   splitting block into 2 chunk(s)\n");
    KL_PRINT("klinfo:     chunk[0]:\n");
    KL_PRINT("klinfo:       system address:  %p\n", chunk);
    KL_PRINT("klinfo:       system size:     %zu\n", KL_CHUNK_SIZE_ALIGNED(size));
    KL_PRINT("klinfo:       program address: %p\n", KL_C2P(chunk));
    KL_PRINT("klinfo:       program size:    %zu\n", size);
    KL_PRINT("klinfo:     chunk[1]:\n");
    KL_PRINT("klinfo:       system address:  %p\n", KL_CS2T(chunk, size));
    KL_PRINT("klinfo:       system size:     %zu\n",
      KL_C2S(chunk)-KL_CHUNK_SIZE_ALIGNED(size));
    KL_PRINT("klinfo:\n");

    /* Set chunk[1] size. */
    KL_C2S(KL_CS2T(chunk, size)) = KL_C2S(chunk)-KL_CHUNK_SIZE_ALIGNED(size);
    KL_C2F(KL_CS2T(chunk, size)) = KL_C2S(chunk)-KL_CHUNK_SIZE_ALIGNED(size);

    /* Add chunk[1] to free chunk data structure. */
    if (0 != kl_bin_ad(&bin, KL_CS2T(chunk, size)))
      return NULL;

    /* Update chunk[0] size. */
    KL_C2S(chunk) = KL_CHUNK_SIZE_ALIGNED(size);

    /* Sanity check to make sure that chunk[0] can be reached from chunk[1] */
    assert((KL_C2F(chunk) = KL_CHUNK_SIZE_ALIGNED(size),
      chunk == KL_T2C(KL_CS2T(chunk, size))));

    //KL_PRINT("klinfo: footer0 address: %p\n", (void*)&(KL_C2F(chunk)));
    //KL_PRINT("klinfo: footer1 address: %p\n",
    //  (void*)&(KL_C2F(KL_CS2T(chunk, size))));
  }
  else {
    KL_PRINT("klinfo:   splitting block into 1 chunk(s)\n");
    KL_PRINT("klinfo:     chunk[0]:\n");
    KL_PRINT("klinfo:       system address:  %p\n", chunk);
    KL_PRINT("klinfo:       system size:     %zu\n", KL_C2S(chunk));
    KL_PRINT("klinfo:       program address: %p\n", KL_C2P(chunk));
    KL_PRINT("klinfo:       program size:    %zu\n", size);
    KL_PRINT("klinfo:\n");
  }

  /* Set chunk as in use */
  KL_C2F(chunk) = 0;

  /* Increment count for containing block. */
  KL_B2N(KL_C2B(chunk))++;

  KL_PRINT("klinfo: incrementing block count: %zu\n", KL_B2N(KL_C2B(chunk)));
  KL_PRINT("klinfo: klmalloc end\n");
  KL_PRINT("klinfo:\n");

  KL_SANITY_VALID_POINTER(KL_C2P(chunk));

  return KL_C2P(chunk);
}


/****************************************************************************/
/* Allocate num*size bytes of zeroed memory */
/****************************************************************************/
KL_EXPORT void *
klcalloc(size_t const num, size_t const size)
{
  KL_INIT_CHECK;

  void * ptr=klmalloc(num*size);
  if (NULL != ptr)
    memset(ptr, 0, num*size);
  return ptr;
}


/****************************************************************************/
/* Release size bytes of memory */
/****************************************************************************/
KL_EXPORT void
klfree(void * const ptr)
{
  void * chunk = KL_P2C(ptr);
  void * block = KL_C2B(chunk);

  /* TODO: one issue with current implementation is the following.  When a
   * large block gets split into smaller blocks, the ability to use the KL_C2B
   * macro goes away, since the large block could get split up such that a
   * chunk beyond the KL_BLOCK_SIZE_ALIGNED limit is allocated, thus when
   * KL_C2B is called it will return a memory location of something which is
   * not a block.  To address this, large blocks are not allowed to be split.
   * */
  /* TODO: another issue with current implementation is the following.  When a
   * block count gets decremented to zero, its associated memory is freed.
   * However, nowhere does the system account for any chunks that remain in
   * the free chunk data structure.  This is especially problematic because
   * those chunks still in the free chunk data structure cannot be removed in
   * place, due to the singly-linked nature of the data structure. */

  KL_INIT_CHECK;

  KL_PRINT("klinfo: freeing from a block with %zu chunk(s)\n", KL_B2N(block));
  KL_PRINT("klinfo:   block address:  %p\n", block);
  KL_PRINT("klinfo:   chunk address:  %p\n", chunk);

  /* Sanity check to make sure the containing block is somewhat valid. */
  assert(0 != KL_B2N(block));

  /* TODO: Implicitly, the following rule prevents large allocations from
   * going into the free chunk data structure.  Thus, it also prevents the
   * limitation described above.  However, in many cases, it would be nice to
   * keep large allocations around for quicker allocation time. */

  /* Decrement count for containing block and release entire block if it is
   * empty. */
  if (0 == --KL_B2N(block)) {
    KL_PRINT("klinfo:   releasing block back to system\n");

    assert(KL_ISLAST(chunk) || !KL_INUSE(KL_C2T(chunk)));

    /* Remove following chunk from free chunk data structure. */
    if (!KL_ISLAST(chunk) && 0 != kl_bin_rm(&bin, KL_C2T(chunk)))
      return;

    KL_CALL_SYS_FREE(block, KL_B2S(block));

    KL_PRINT("klinfo:\n");
  }
  /* Otherwise, add the chunk back into free chunk data structure. */
  else {
    /* Coalesce with previous chunk. */
    if (!KL_ISFIRST(chunk) && !KL_INUSE(KL_T2C(chunk))) {
      /* Remove previous chunk from free chunk data structure. */
      if (0 != kl_bin_rm(&bin, KL_T2C(chunk)))
        return;

      KL_PRINT("klinfo:   coalescing with previous chunk\n");
      KL_PRINT("klinfo:     previous chunk size:  %zu\n", KL_C2S(KL_T2C(chunk)));
      KL_PRINT("klinfo:     chunk size:           %zu\n", KL_C2S(chunk));
      KL_PRINT("klinfo:     new chunk size:       %zu\n",
        KL_C2S(KL_T2C(chunk))+KL_C2S(chunk));

      /* Set chunk to point to previous chunk. */
      chunk = KL_T2C(chunk);

      /* Update chunk size. */
      KL_C2S(chunk) = KL_C2S(chunk) + KL_C2S(KL_C2T(chunk));
    }

    /* Coalesce with following chunk. */
    if (!KL_ISLAST(chunk) && !KL_INUSE(KL_C2T(chunk))) {
      /* Remove following chunk from free chunk data structure. */
      if (0 != kl_bin_rm(&bin, KL_C2T(chunk)))
        return;

      KL_PRINT("klinfo:   coalescing with following chunk\n");
      KL_PRINT("klinfo:     chunk size:           %zu\n", KL_C2S(chunk));
      KL_PRINT("klinfo:     following chunk size: %zu\n", KL_C2S(KL_C2T(chunk)));
      KL_PRINT("klinfo:     new chunk size:       %zu\n",
        KL_C2S(chunk)+KL_C2S(KL_C2T(chunk)));

      /* Update chunk size. */
      KL_C2S(chunk) = KL_C2S(chunk) + KL_C2S(KL_C2T(chunk));
    }

    /* Set chunk as not in use. */
    KL_C2F(chunk) = KL_C2S(chunk);

    KL_PRINT("klinfo:   adding chunk back into free chunk data structure\n");
    /* Add chunk to free chunk data structure. */
    if (0 != kl_bin_ad(&bin, chunk))
      return;

    KL_PRINT("klinfo:\n");
  }
}
