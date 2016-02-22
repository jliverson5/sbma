/*
Copyright (c) 2015,2016 Jeremy Iverson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif


#include <stdarg.h> /* stdarg library */
#include <stddef.h> /* size_t */
#include "common.h"
#include "lock.h"
#include "sbma.h"
#include "vmm.h"


/****************************************************************************/
/*! Initialization variables. */
/****************************************************************************/
#ifdef USE_THREAD
static pthread_mutex_t init_lock=PTHREAD_MUTEX_INITIALIZER;
#endif


/****************************************************************************/
/*! The single instance of vmm per process. */
/****************************************************************************/
struct vmm _vmm_={.init=0, .ipc.init=0};


/****************************************************************************/
/*! Initialize the sbma environment. */
/****************************************************************************/
SBMA_EXTERN int
sbma_init(char const * const __fstem, int const __uniq,
          size_t const __page_size, int const __n_procs,
          size_t const __max_mem, int const __opts)
{
  /* acquire init lock */
  if (-1 == lock_get(&init_lock))
    return -1;

  if (-1 == vmm_init(&_vmm_, __fstem, __uniq, __page_size, __n_procs,\
      __max_mem, __opts))
  {
    (void)lock_let(&init_lock);
    return -1;
  }

  /* release init lock */
  if (-1 == lock_let(&init_lock))
    return -1;

  return 0;
}


/****************************************************************************/
/*! Initialize the sbma environment from a va_list. */
/****************************************************************************/
SBMA_EXTERN int
sbma_vinit(va_list args)
{
  char const * fstem     = va_arg(args, char const *);
  int const uniq         = va_arg(args, int);
  size_t const page_size = va_arg(args, size_t);
  int const n_procs      = va_arg(args, int);
  size_t const max_mem   = va_arg(args, size_t);
  int const opts         = va_arg(args, int);
  return sbma_init(fstem, uniq, page_size, n_procs, max_mem, opts);
}


/****************************************************************************/
/*! Destroy the sbma environment. */
/****************************************************************************/
SBMA_EXTERN int
sbma_destroy(void)
{
  /* acquire init lock */
  if (-1 == lock_get(&init_lock))
    return -1;

  if (-1 == vmm_destroy(&_vmm_)) {
    (void)lock_let(&init_lock);
    return -1;
  }

  /* release init lock */
  if (-1 == lock_let(&init_lock))
    return -1;

  return 0;
}