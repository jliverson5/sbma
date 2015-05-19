#ifndef __MMU_H__
#define __MMU_H__ 1


#include <stdint.h> /* uint8_t, uintptr_t */
#include <stddef.h> /* NULL */
#include "config.h"


/****************************************************************************/
/*
 * Memory management unit page status code bits:
 *
 *   bit 0 ==    0: zero fill allowed    1: page must be filled from disk
 *   bit 1 ==    0: page is resident     1: page is not resident
 *   bit 2 ==    0: page is unmodified   1: page is dirty
 */
/****************************************************************************/
enum mmu_status_code
{
  MMU_ZFILL = 1 << 0,
  MMU_RSDNT = 1 << 1,
  MMU_DIRTY = 1 << 2
};


/****************************************************************************/
/*! Allocation table entry. */
/****************************************************************************/
struct ate
{
  size_t n_pages;       /*!< number of pages allocated */
  size_t l_pages;       /*!< number of pages loaded */
  uintptr_t base;       /*!< starting address fro the allocation */
  uint8_t * flags;      /*!< status flags for pages */
  struct ate * prev;    /*!< doubly linked list pointer */
  struct ate * next;    /*!< doubly linked list pointer */
#ifdef USE_PTHREAD
  pthread_mutex_t lock; /*!< mutex guarding struct */
#endif
};


/****************************************************************************/
/*! Memory management unit. */
/****************************************************************************/
struct mmu
{
  size_t page_size;     /*!< page size */
  struct ate * a_tbl;   /*!< mmu allocation table */
#ifdef USE_PTHREAD
  pthread_mutex_t lock; /*!< mutex guarding struct */
#endif
};


/****************************************************************************/
/*! Initialize the memory management unit. */
/****************************************************************************/
static inline int
__mmu_init__(struct mmu * const __mmu, size_t const __page_size)
{
  /* clear pointer */
  __mmu->a_tbl = NULL;

  /* set mmu page size */
  __mmu->page_size = __page_size;

  /* initialize mmu lock */
  if (-1 == LOCK_INIT(&(__mmu->lock)))
    return -1;

  return 0;
}


/****************************************************************************/
/*! Destroy the memory management unit. */
/****************************************************************************/
static inline int
__mmu_destroy__(struct mmu * const __mmu)
{
  /* destroy mmu lock */
  if (-1 == LOCK_FREE(&(__mmu->lock)))
    return -1;

  return 0;

  if (NULL == __mmu) {}
}


/****************************************************************************/
/*! Insert __ate into __mmu. */
/****************************************************************************/
static inline int
__mmu_insert_ate__(struct mmu * const __mmu, struct ate * const __ate)
{
  /* acquire lock */
  if (-1 == LOCK_GET(&(__mmu_>lock)))
    return -1;

  /* insert at beginning of doubly linked list */
  if (NULL == __mmu->a_tbl) {
    __mmu->a_tbl = __ate;
    __ate->prev  = NULL;
    __ate->next  = NULL;
  }
  else {
    __ate->prev        = __mmu->a_tbl->prev;
    __ate->next        = __mmu->a_tbl;
    __mmu->a_tbl->prev = __ate;
    __mmu->a_tbl       = __ate;
  }

  /* release lock */
  if (-1 == LOCK_LET(&(__mmu_>lock)))
    return -1;

  return 0;
}


/****************************************************************************/
/*! Invalidate __ate. */
/****************************************************************************/
static inline int
__mmu_invalidate_ate__(struct mmu * const __mmu, struct ate * const __ate)
{
  if (-1 == LOCK_GET(&(__mmu->lock)))
    return -1;

  /* remove from doubly linked list */
  if (NULL == __ate->prev)
    __mmu->a_tbl = __ate->next;
  else
    __ate->prev->next = __ate->next;
  if (NULL != __ate->next)
    __ate->next->prev = __ate->prev;

  if (-1 == LOCK_LET(&(__mmu->lock)))
    return -1;

  return 0;
}


/****************************************************************************/
/*! Find the ate, if one exists, that contains __addr. */
/****************************************************************************/
static inline struct ate *
__mmu_lookup_ate__(struct mmu * const __mmu, void const * const __addr)
{
  size_t len;
  void * addr;
  struct ate * ate;

  /* acquire lock */
  if (-1 == LOCK_GET(&(__mmu->lock)))
    return (struct ate*)-1;

  /* search doubly linked list for a ate which contains __addr */
  for (ate=__mmu->a_tbl; NULL!=ate; ate=ate->next) {
    len  = ate->n_pages*__mmu->page_size;
    addr = (void*)ate->base;
    if (addr <= __addr && __addr < (void*)((uintptr_t)addr+len))
      break;
  }

  /* TODO: should ate get locked here? */
  if (-1 == LOCK_GET(&(ate->lock))) {
    (void)LOCK_LET(&(__mmu->lock));
    return (struct ate*)-1;
  }

  /* release lock */
  if (-1 == LOCK_LET(&(__mmu->lock)))
    return (struct ate*)-1;

  return ate;
}


#endif
