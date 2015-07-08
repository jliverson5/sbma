/*
Copyright (c) 2015, Jeremy Iverson
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __IPC_H__
#define __IPC_H__ 1


#include <semaphore.h> /* semaphore library */
#include <stdint.h>    /* uint8_t */
#include <stddef.h>    /* size_t */


#define SIGIPC (SIGRTMIN+0)


/****************************************************************************/
/*!
 * Inter-process communication process status bits:
 *
 *   bit 0 ==    0: running       1: blocked
 *   bit 1 ==    0: unpopulated   1: populated
 */
/****************************************************************************/
enum ipc_code
{
  IPC_BLOCKED   = 1 << 0,
  IPC_POPULATED = 1 << 1
};


/****************************************************************************/
/*! Interprocess environment. */
/****************************************************************************/
struct ipc
{
  int init;      /*!< initialized indicator */

  int id;        /*!< ipc id of process amoung the n_procs */
  int n_procs;   /*!< number of processes in coordination */

  int uniq;      /*!< unique identifier for shared mem and semaphores */

  sem_t * mtx;   /*!< critical section semaphores */
  sem_t * cnt;   /*!< ... */
  sem_t * trn1;  /*!< ... */
  sem_t * trn2;  /*!< ... */

  void * shm;    /*!< shared memory region */
  size_t * smem; /*!< pointer into shm for smem scalar */
  size_t * pmem; /*!< pointer into shm for pmem array */
  int * pid;     /*!< pointer into shm for pid array */

  uint8_t * flags; /*!< pointer into shm for flags array */
};


#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*! Initialize the interprocess environment. */
/****************************************************************************/
int
__ipc_init(struct ipc * const __ipc, int const __uniq, int const __n_procs,
           size_t const __max_mem);


/****************************************************************************/
/*! Destroy the interprocess environment. */
/****************************************************************************/
int
__ipc_destroy(struct ipc * const __ipc);


/****************************************************************************/
/*! Transition process to blocked state. */
/****************************************************************************/
int
__ipc_block(struct ipc * const __ipc);


/****************************************************************************/
/*! Transition process to running state. */
/****************************************************************************/
int
__ipc_unblock(struct ipc * const __ipc);


/****************************************************************************/
/*! Transition process to populated status. */
/****************************************************************************/
int
__ipc_populate(struct ipc * const __ipc);


/****************************************************************************/
/*! Transition process to unpopulated status. */
/****************************************************************************/
int
__ipc_unpopulate(struct ipc * const __ipc);


/****************************************************************************/
/*! Check if a SIGIPC was received during last __ipc_block/__ipc_unblock
 * cycle. */
/****************************************************************************/
int
__ipc_sigrecvd(struct ipc * const __ipc);


/****************************************************************************/
/*! Check if process is eligible for eviction. */
/****************************************************************************/
int
__ipc_is_eligible(struct ipc * const __ipc);


/****************************************************************************/
/*! Account for resident memory before admission. Check to see if the system
 *  can support the addition of __value bytes of memory. */
/****************************************************************************/
int
__ipc_madmit(struct ipc * const __ipc, size_t const __value);


/****************************************************************************/
/*! Account for loaded memory after eviction. */
/****************************************************************************/
int
__ipc_mevict(struct ipc * const __ipc, size_t const __value);

#ifdef __cplusplus
}
#endif


#endif
