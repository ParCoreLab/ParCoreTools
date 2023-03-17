// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2019, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *


#include <sys/time.h>
#include <sys/resource.h>
//***************************************************************************
// local include files
//***************************************************************************
#include "sample_event.h"
#include "disabled.h"

#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <unwind/common/validate_return_addr.h>
#if ADAMANT_USED
#include <adm_init_fini.h>
#endif
#include "matrix.h"
#include "env.h"
#include "myposix.h"

//***************************************************************************
// local variables
//***************************************************************************

static atomic_long num_samples_total = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_attempted = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_blocked_async = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_blocked_dlopen = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_dropped = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_segv = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_partial = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_yielded = ATOMIC_VAR_INIT(0);


static atomic_long num_samples_imprecise = ATOMIC_VAR_INIT(0);
static atomic_long num_watchpoints_triggered = ATOMIC_VAR_INIT(0);
static atomic_long num_watchpoints_set = ATOMIC_VAR_INIT(0);
static atomic_long num_watchpoints_dropped = ATOMIC_VAR_INIT(0);
static atomic_long num_watchpoints_imprecise = ATOMIC_VAR_INIT(0);
static atomic_long num_watchpoints_imprecise_address = ATOMIC_VAR_INIT(0);
static atomic_long num_watchpoints_imprecise_address_8_byte = ATOMIC_VAR_INIT(0);
static atomic_long num_sample_triggering_watchpoints= ATOMIC_VAR_INIT(0);
static atomic_long num_insane_ip= ATOMIC_VAR_INIT(0);

static atomic_long num_writtenBytes = ATOMIC_VAR_INIT(0);
static atomic_long num_usedBytes = ATOMIC_VAR_INIT(0);
static atomic_long num_deadBytes = ATOMIC_VAR_INIT(0);

static atomic_long num_newBytes = ATOMIC_VAR_INIT(0);
static atomic_long num_oldBytes = ATOMIC_VAR_INIT(0);
static atomic_long num_oldAppxBytes = ATOMIC_VAR_INIT(0);
static atomic_long num_loadedBytes = ATOMIC_VAR_INIT(0);

static atomic_long num_accessedIns = ATOMIC_VAR_INIT(0);
static atomic_long num_trueWWIns = ATOMIC_VAR_INIT(0);
static atomic_long num_trueRWIns = ATOMIC_VAR_INIT(0);
static atomic_long num_trueWRIns = ATOMIC_VAR_INIT(0);
static atomic_long num_falseWWIns = ATOMIC_VAR_INIT(0);
static atomic_long num_falseRWIns = ATOMIC_VAR_INIT(0);
static atomic_long num_falseWRIns = ATOMIC_VAR_INIT(0);

static atomic_long num_reuseSpatial = ATOMIC_VAR_INIT(0);
static atomic_long num_reuseTemporal =  ATOMIC_VAR_INIT(0);
static atomic_long num_reuse = ATOMIC_VAR_INIT(0);
static atomic_long num_latency = ATOMIC_VAR_INIT(0);
static atomic_long num_corrected_reuse_distance = ATOMIC_VAR_INIT(0);

static atomic_long num_unwind_intervals_total = ATOMIC_VAR_INIT(0);
static atomic_long num_unwind_intervals_suspicious = ATOMIC_VAR_INIT(0);

static atomic_long trolled = ATOMIC_VAR_INIT(0);
static atomic_long frames_total = ATOMIC_VAR_INIT(0);
static atomic_long trolled_frames = ATOMIC_VAR_INIT(0);

extern void dump_profiling_metrics();

extern char output_directory[PATH_MAX];

//***************************************************************************
// interface operations
//***************************************************************************
long load_and_store_all_load;

long load_and_store_all_store;

long store_all_store;

void
hpcrun_stats_reinit(void)
{
  fs_matrix_size =  0;
  ts_matrix_size =  0;
  as_matrix_size =  0;
  as_core_matrix_size = 0;
  HASHTABLESIZE = atoi(getenv(BULLETIN_BOARD_SIZE));
#if ADAMANT_USED
  if(getenv(HPCRUN_OBJECT_LEVEL)) {
        //adm_initialize();
        fprintf(stderr, "object level is activated\n");
        //OBJECT_THRESHOLD = atoi(getenv(OBJECT_SIZE_THRESHOLD));
  }
#endif
  //fprintf(stderr, "bulletin board size is %d\n", HASHTABLESIZE);
  //fprintf(stderr, "object threshold is %d\n", OBJECT_THRESHOLD);
  //fprintf(stderr, "watchpoint size is %d\n", atoi(getenv(WATCHPOINT_SIZE)));
  for(int i = 0; i < HASHTABLESIZE; i++) {
    bulletinBoard.hashTable[i].cacheLineBaseAddress = -1;
  }
  atomic_store_explicit(&num_samples_total, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_attempted, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_blocked_async, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_blocked_dlopen, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_dropped, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_segv, 0, memory_order_relaxed);
  atomic_store_explicit(&num_unwind_intervals_total, 0, memory_order_relaxed);
  atomic_store_explicit(&num_unwind_intervals_suspicious, 0, memory_order_relaxed);
  atomic_store_explicit(&trolled, 0, memory_order_relaxed);
  atomic_store_explicit(&frames_total, 0, memory_order_relaxed);
  atomic_store_explicit(&trolled_frames, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_imprecise, 0, memory_order_relaxed);
  atomic_store_explicit(&num_watchpoints_triggered, 0, memory_order_relaxed);
  atomic_store_explicit(&num_watchpoints_set, 0, memory_order_relaxed);
  atomic_store_explicit(&num_watchpoints_dropped, 0, memory_order_relaxed);
  atomic_store_explicit(&num_watchpoints_imprecise, 0, memory_order_relaxed);
  atomic_store_explicit(&num_watchpoints_imprecise_address, 0, memory_order_relaxed);
  atomic_store_explicit(&num_watchpoints_imprecise_address_8_byte, 0, memory_order_relaxed);
  atomic_store_explicit(&num_sample_triggering_watchpoints, 0, memory_order_relaxed);
  atomic_store_explicit(&num_insane_ip, 0, memory_order_relaxed);
  atomic_store_explicit(&num_writtenBytes, 0, memory_order_relaxed);
  atomic_store_explicit(&num_usedBytes, 0, memory_order_relaxed);
  atomic_store_explicit(&num_deadBytes,0,  memory_order_relaxed);

  atomic_store_explicit(&num_newBytes, 0, memory_order_relaxed);
  atomic_store_explicit(&num_oldBytes, 0, memory_order_relaxed);
  atomic_store_explicit(&num_oldAppxBytes, 0, memory_order_relaxed);
  atomic_store_explicit(&num_loadedBytes, 0, memory_order_relaxed);

  atomic_store_explicit(&num_accessedIns, 0, memory_order_relaxed);
  atomic_store_explicit(&num_falseWWIns, 0, memory_order_relaxed);
  atomic_store_explicit(&num_falseRWIns, 0, memory_order_relaxed);
  atomic_store_explicit(&num_falseWRIns, 0, memory_order_relaxed);
  atomic_store_explicit(&num_trueWWIns, 0, memory_order_relaxed);
  atomic_store_explicit(&num_trueRWIns, 0, memory_order_relaxed);
  atomic_store_explicit(&num_trueWRIns, 0, memory_order_relaxed);

  atomic_store_explicit(&num_reuseSpatial, 0, memory_order_relaxed);
  atomic_store_explicit(&num_reuseTemporal, 0, memory_order_relaxed);
  atomic_store_explicit(&num_latency, 0, memory_order_relaxed);
  atomic_store_explicit(&num_corrected_reuse_distance, 0, memory_order_relaxed);
}



//-----------------------------
// watchpoints
//-----------------------------

void
hpcrun_stats_num_samples_imprecise_inc(long val)
{
  atomic_fetch_add_explicit(&num_samples_imprecise, val, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_imprecise(void)
{
  return atomic_load_explicit(&num_samples_imprecise, memory_order_relaxed);
}

void
hpcrun_stats_num_watchpoints_set_inc(long val)
{
  atomic_fetch_add_explicit(&num_watchpoints_set, val, memory_order_relaxed);
}


long
hpcrun_stats_num_watchpoints_set(void)
{
  return atomic_load_explicit(&num_watchpoints_set, memory_order_relaxed);
}

void
hpcrun_stats_num_watchpoints_triggered_inc(long val)
{
  atomic_fetch_add_explicit(&num_watchpoints_triggered, val, memory_order_relaxed);
}


long
hpcrun_stats_num_watchpoints_triggered(void)
{
  return atomic_load_explicit(&num_watchpoints_triggered, memory_order_relaxed);
}

void
hpcrun_stats_num_watchpoints_dropped_inc(long val)
{
  atomic_fetch_add_explicit(&num_watchpoints_dropped, val, memory_order_relaxed);
}


long
hpcrun_stats_num_watchpoints_dropped(void)
{
  return atomic_load_explicit(&num_watchpoints_dropped, memory_order_relaxed);
}

void
hpcrun_stats_num_watchpoints_imprecise_inc(long val)
{
  atomic_fetch_add_explicit(&num_watchpoints_imprecise, val, memory_order_relaxed);
}


long
hpcrun_stats_num_watchpoints_imprecise(void)
{
  return atomic_load_explicit(&num_watchpoints_imprecise, memory_order_relaxed);
}


void
hpcrun_stats_num_watchpoints_imprecise_address_inc(long val)
{
  atomic_fetch_add_explicit(&num_watchpoints_imprecise_address, val, memory_order_relaxed);
}


long
hpcrun_stats_num_watchpoints_imprecise_address(void)
{
  return atomic_load_explicit(&num_watchpoints_imprecise_address, memory_order_relaxed);
}


void
hpcrun_stats_num_watchpoints_imprecise_address_8_byte_inc(long val)
{
  atomic_fetch_add_explicit(&num_watchpoints_imprecise_address_8_byte, val, memory_order_relaxed);
}


long
hpcrun_stats_num_watchpoints_imprecise_address_8_byte(void)
{
  return atomic_load_explicit(&num_watchpoints_imprecise_address_8_byte, memory_order_relaxed);
}


void
hpcrun_stats_num_sample_triggering_watchpoints_inc(long val)
{
  atomic_fetch_add_explicit(&num_sample_triggering_watchpoints, val, memory_order_relaxed);
}


long
hpcrun_stats_num_sample_triggering_watchpoints(void)
{
  return atomic_load_explicit(&num_sample_triggering_watchpoints, memory_order_relaxed);
}

void
hpcrun_stats_num_insane_ip_inc(long val)
{
  atomic_fetch_add_explicit(&num_insane_ip, val, memory_order_relaxed);
}                
                 
                 
long             
hpcrun_stats_num_insane_ip(void)
{                
  return atomic_load_explicit(&num_insane_ip, memory_order_relaxed);
}                


void
hpcrun_stats_num_writtenBytes_inc(long val)
{
  atomic_fetch_add_explicit(&num_writtenBytes, val, memory_order_relaxed);
}                
                 

void
hpcrun_stats_num_usedBytes_inc(long val)
{
  atomic_fetch_add_explicit(&num_usedBytes, val, memory_order_relaxed);
}                

void
hpcrun_stats_num_deadBytes_inc(long val)
{
  atomic_fetch_add_explicit(&num_deadBytes, val, memory_order_relaxed);
}                

void
hpcrun_stats_num_newBytes_inc(long val)
{
  atomic_fetch_add_explicit(&num_newBytes, val, memory_order_relaxed);
}                

void
hpcrun_stats_num_oldAppxBytes_inc(long val)
{
  atomic_fetch_add_explicit(&num_oldAppxBytes, val, memory_order_relaxed);
}   

void
hpcrun_stats_num_oldBytes_inc(long val)
{
  atomic_fetch_add_explicit(&num_oldBytes, val, memory_order_relaxed);
}                
             
void
hpcrun_stats_num_loadedBytes_inc(long val)
{
  atomic_fetch_add_explicit(&num_loadedBytes, val, memory_order_relaxed);
}                

void
hpcrun_stats_num_accessedIns_inc(long val)
{
  atomic_fetch_add_explicit(&num_accessedIns, val, memory_order_relaxed);
}            

void
hpcrun_stats_num_reuse_inc(long val)
{
  atomic_fetch_add_explicit(&num_reuse, val, memory_order_relaxed);
}            

void
hpcrun_stats_num_reuseTemporal_inc(long val)
{
  atomic_fetch_add_explicit(&num_reuseTemporal, val, memory_order_relaxed);
}

void
hpcrun_stats_num_reuseSpatial_inc(long val)
{
  atomic_fetch_add_explicit(&num_reuseSpatial, val, memory_order_relaxed);
}

void
hpcrun_stats_num_latency_inc(long val)
{
  atomic_fetch_add_explicit(&num_latency, val, memory_order_relaxed);
}            

void
hpcrun_stats_num_corrected_reuse_distance_inc(long val)
{
  atomic_fetch_add_explicit(&num_corrected_reuse_distance, val, memory_order_relaxed);
}

void
hpcrun_stats_num_falseWWIns_inc(long val)
{
  atomic_fetch_add_explicit(&num_falseWWIns, val, memory_order_relaxed);
}            


void
hpcrun_stats_num_falseRWIns_inc(long val)
{
  atomic_fetch_add_explicit(&num_falseRWIns, val, memory_order_relaxed);
}            

void
hpcrun_stats_num_falseWRIns_inc(long val)
{
  atomic_fetch_add_explicit(&num_falseWRIns, val, memory_order_relaxed);
}            

void
hpcrun_stats_num_trueWWIns_inc(long val)
{
    atomic_fetch_add_explicit(&num_trueWWIns, val, memory_order_relaxed);
}

void
hpcrun_stats_num_trueRWIns_inc(long val)
{
    atomic_fetch_add_explicit(&num_trueRWIns, val, memory_order_relaxed);
}

void
hpcrun_stats_num_trueWRIns_inc(long val)
{
    atomic_fetch_add_explicit(&num_trueWRIns, val, memory_order_relaxed);
}


//-----------------------------
// samples total 
//-----------------------------

void
hpcrun_stats_num_samples_total_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_total, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_total(void)
{
  return atomic_load_explicit(&num_samples_total, memory_order_relaxed);
}



//-----------------------------
// samples attempted 
//-----------------------------

void
hpcrun_stats_num_samples_attempted_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_attempted, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_attempted(void)
{
  return atomic_load_explicit(&num_samples_attempted, memory_order_relaxed);
}



//-----------------------------
// samples blocked async 
//-----------------------------

// The async blocks happen in the signal handlers, without getting to
// hpcrun_sample_callpath, so also increment the total count here.
void
hpcrun_stats_num_samples_blocked_async_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_blocked_async, 1L, memory_order_relaxed);
  atomic_fetch_add_explicit(&num_samples_total, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_blocked_async(void)
{
  return atomic_load_explicit(&num_samples_blocked_async, memory_order_relaxed);
}



//-----------------------------
// samples blocked dlopen 
//-----------------------------

void
hpcrun_stats_num_samples_blocked_dlopen_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_blocked_dlopen, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_blocked_dlopen(void)
{
  return atomic_load_explicit(&num_samples_blocked_dlopen, memory_order_relaxed);
}



//-----------------------------
// samples dropped
//-----------------------------

void
hpcrun_stats_num_samples_dropped_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_dropped, 1L, memory_order_relaxed);
}

long
hpcrun_stats_num_samples_dropped(void)
{
  return atomic_load_explicit(&num_samples_dropped, memory_order_relaxed);
}

//----------------------------
// partial unwinds
//----------------------------

void
hpcrun_stats_num_samples_partial_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_partial, 1L, memory_order_relaxed);
}

long
hpcrun_stats_num_samples_partial(void)
{
  return atomic_load_explicit(&num_samples_partial, memory_order_relaxed);
}

//-----------------------------
// samples segv
//-----------------------------

void
hpcrun_stats_num_samples_segv_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_segv, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_segv(void)
{
  return atomic_load_explicit(&num_samples_segv, memory_order_relaxed);
}




//-----------------------------
// unwind intervals total
//-----------------------------

void
hpcrun_stats_num_unwind_intervals_total_inc(void)
{
  atomic_fetch_add_explicit(&num_unwind_intervals_total, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_unwind_intervals_total(void)
{
  return atomic_load_explicit(&num_unwind_intervals_total, memory_order_relaxed);
}



//-----------------------------
// unwind intervals suspicious
//-----------------------------

void
hpcrun_stats_num_unwind_intervals_suspicious_inc(void)
{
  atomic_fetch_add_explicit(&num_unwind_intervals_suspicious, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_unwind_intervals_suspicious(void)
{
  return atomic_load_explicit(&num_unwind_intervals_suspicious, memory_order_relaxed);
}

//------------------------------------------------------
// samples that include 1 or more successful troll steps
//------------------------------------------------------

void
hpcrun_stats_trolled_inc(void)
{
  atomic_fetch_add_explicit(&trolled, 1L, memory_order_relaxed);
}

long
hpcrun_stats_trolled(void)
{
  return atomic_load_explicit(&trolled, memory_order_relaxed);
}

//------------------------------------------------------
// total number of (unwind) frames in sample set
//------------------------------------------------------

void
hpcrun_stats_frames_total_inc(long amt)
{
  atomic_fetch_add_explicit(&frames_total, amt, memory_order_relaxed);
}

long
hpcrun_stats_frames_total(void)
{
  return atomic_load_explicit(&frames_total, memory_order_relaxed);
}

//---------------------------------------------------------------------
// total number of (unwind) frames in sample set that employed trolling
//---------------------------------------------------------------------

void
hpcrun_stats_trolled_frames_inc(long amt)
{
  atomic_fetch_add_explicit(&trolled_frames, amt, memory_order_relaxed);
}

long
hpcrun_stats_trolled_frames(void)
{
  return atomic_load_explicit(&trolled_frames, memory_order_relaxed);
}

//----------------------------
// samples yielded due to deadlock prevention
//----------------------------

void
hpcrun_stats_num_samples_yielded_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_yielded, 1L, memory_order_relaxed);
}

long
hpcrun_stats_num_samples_yielded(void)
{
  return atomic_load_explicit(&num_samples_yielded, memory_order_relaxed);
}

//-----------------------------
// print summary
//-----------------------------

void
hpcrun_stats_print_summary(void)
{
  int object_flag = 0;
#if ADAMANT_USED
  if(getenv(HPCRUN_OBJECT_LEVEL)) {
    object_flag = 1;
    adm_finalize(object_flag, output_directory, hpcrun_files_executable_name(), getpid() );
  }
#endif
  dump_profiling_metrics();
  long blocked = atomic_load_explicit(&num_samples_blocked_async, memory_order_relaxed) +
    atomic_load_explicit(&num_samples_blocked_dlopen, memory_order_relaxed);
  long errant = atomic_load_explicit(&num_samples_dropped, memory_order_relaxed);
  long soft = atomic_load_explicit(&num_samples_dropped, memory_order_relaxed) -
    atomic_load_explicit(&num_samples_segv, memory_order_relaxed);
  long valid = atomic_load_explicit(&num_samples_attempted, memory_order_relaxed);
  if (ENABLED(NO_PARTIAL_UNW)) {
    valid = atomic_load_explicit(&num_samples_attempted, memory_order_relaxed) - errant;
  }

  hpcrun_memory_summary();

  struct rusage rusage;
  getrusage(RUSAGE_SELF, &rusage);

  //fprintf(stdierr, "load_and_store_all_load: %ld\n", load_and_store_all_load);
  //fprintf(stderr, "load_and_store_all_store: %ld\n", load_and_store_all_store);
  //fprintf(stderr, "store_all_store: %ld\n", store_all_store);
  //AMSG("WATCHPOINT ANOMALIES: samples:%ld, SM_imprecise:%ld, WP_Set:%ld, WP_triggered:%ld, WP_SampleTriggering:%ld, WP_ImpreciseIP:%ld, WP_InsaneIP:%ld, WP_Off8Addr:%ld, WP_ImpreciseAddr:%ld, WP_Dropped:%ld", num_samples_total, num_samples_imprecise, num_watchpoints_set, num_watchpoints_triggered, num_sample_triggering_watchpoints,  num_watchpoints_imprecise, num_insane_ip, num_watchpoints_imprecise_address_8_byte, num_watchpoints_imprecise_address, num_watchpoints_dropped);
  AMSG("WATCHPOINT ANOMALIES: samples:%.2e, SM_imprecise:%.2e, WP_Set:%.2e, WP_triggered:%.2e, WP_SampleTriggering:%.2e, WP_ImpreciseIP:%.2e, WP_InsaneIP:%.2e, WP_Off8Addr:%.2e, WP_ImpreciseAddr:%.2e, WP_Dropped:%.2e", (double)atomic_load(&num_samples_total), (double)atomic_load(&num_samples_imprecise), (double)atomic_load(&num_watchpoints_set), (double)atomic_load(&num_watchpoints_triggered), (double)atomic_load(&num_sample_triggering_watchpoints),  (double)atomic_load(&num_watchpoints_imprecise), (double)atomic_load(&num_insane_ip), (double)atomic_load(&num_watchpoints_imprecise_address_8_byte), (double)atomic_load(&num_watchpoints_imprecise_address), (double)atomic_load(&num_watchpoints_dropped));

  AMSG("WATCHPOINT STATS: writtenBytes:%ld, usedBytes:%ld, deadBytes:%ld, newBytes:%ld, oldBytes:%ld, oldAppxBytes:%ld, loadedBytes:%ld, accessedIns:%ld, falseWWIns:%ld, falseRWIns:%ld, falseWRIns:%ld, trueWWIns:%ld, trueRWIns:%ld, trueWRIns:%ld, RSS:%ld, reuse:%ld, reuseTemporal:%ld, reuseSpatial:%ld, latency:%ld", num_writtenBytes, num_usedBytes, num_deadBytes, num_newBytes, num_oldBytes, num_oldAppxBytes, num_loadedBytes, num_accessedIns, num_falseWWIns, num_falseRWIns, num_falseWRIns, num_trueWWIns, num_trueRWIns, num_trueWRIns,  (size_t)(rusage.ru_maxrss), num_reuse, num_reuseTemporal, num_reuseSpatial, num_latency);

  AMSG("COMDETECTIVE STATS: fs_volume:%0.2lf, fs_core_volume:%0.2lf, ts_volume:%0.2lf, ts_core_volume:%0.2lf, as_volume:%0.2lf, as_core_volume:%0.2lf, cache_line_transfer:%0.2lf, cache_line_transfer_millions:%0.2lf, cache_line_transfer_gbytes:%0.2lf", fs_volume, fs_core_volume, ts_volume, ts_core_volume, as_volume, as_core_volume, cache_line_transfer, cache_line_transfer_millions, cache_line_transfer_gbytes);

  AMSG("SAMPLE ANOMALIES: blocks: %ld (async: %ld, dlopen: %ld), "
       "errors: %ld (segv: %ld, soft: %ld)",
       blocked, num_samples_blocked_async, num_samples_blocked_dlopen,
       errant, num_samples_segv, soft);

  AMSG("SUMMARY: samples: %ld (recorded: %ld, blocked: %ld, errant: %ld, trolled: %ld, yielded: %ld),\n"
       "         frames: %ld (trolled: %ld)\n"
       "         intervals: %ld (suspicious: %ld)",
       num_samples_total, valid, blocked, errant, trolled, num_samples_yielded,
       frames_total, trolled_frames,
       num_unwind_intervals_total,  num_unwind_intervals_suspicious);

  if (hpcrun_get_disabled()) {
    AMSG("SAMPLING HAS BEEN DISABLED");
  }

  // logs, retentions || adj.: recorded, retained, written

  if (ENABLED(UNW_VALID)) {
    hpcrun_validation_summary();
  }
}

