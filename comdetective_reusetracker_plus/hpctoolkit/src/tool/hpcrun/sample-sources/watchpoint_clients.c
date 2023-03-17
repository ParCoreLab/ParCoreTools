// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/papi.c $
// $Id: papi.c 4027 2012-11-28 20:03:03Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2014, Rice University
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

//
// WATCHPOINT sample source oo interface
//


/******************************************************************************
 * system includes
 *****************************************************************************/
#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <linux/perf_event.h>
/******************************************************************************
 * libmonitor
 *****************************************************************************/
#include <monitor.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/threadmgr.h>
#include <hpcrun/files.h>
#include <hpcrun/env.h>

#include <sample-sources/blame-shift/blame-shift.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/splay-macros.h>


// necessary for breakpoints
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <asm/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>
#include <xmmintrin.h>
#include <immintrin.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <hpcrun/unwind/x86-family/x86-move.h>
#include <utilities/arch/context-pc.h>
#include "watchpoint_support.h"
#include <unwind/x86-family/x86-misc.h>
#include "perf/perf-util.h"
#include <hpcrun/handling_sample.h>
#if ADAMANT_USED
#include <adm_init_fini.h>
#endif
#include "matrix.h"
#include "myposix.h"
#include "mymapping.h"
#define REUSE_HISTO 0

#define MULTITHREAD_REUSE_HISTO 1

#ifdef MULTITHREAD_REUSE_HISTO
#define REUSE_HISTO 1
#endif
#ifdef REUSE_HISTO
#include "reuse.h"
#endif

#define WAIT_THRESHOLD 10
extern bool amd_ibs_flag;
int used_wp_count = 0;
int max_used_wp_count = 0;
extern __thread int wait_threshold;
extern int mapping_size;
extern __thread int original_sample_count; 

__thread uint64_t last_trapped_timestamp = 0;
__thread uint64_t last_monitored_wp_timestamp = 0;
__thread cct_node_t *monitored_node;
__thread int monitored_metric;

int bb_store_count = 0;
int all_store_count = 0;

MonitoredNodeStruct_t MonitoredNode;

//int wp_user_list[MAX_WP_SLOTS];
#define SAMPLES_POST_FULL_RESET_VAL (1)
extern int globalWPIsUsers[MAX_WP_SLOTS];
extern int L3GlobalWPIsUsers[MAX_WP_SLOTS];

int red_metric_id = -1;
int redApprox_metric_id = -1;
int load_metric_id = -1;
int dead_metric_id = -1;
int measured_metric_id = -1;
int latency_metric_id = -1;
int temporal_metric_id = -1;
int spatial_metric_id = -1;
int false_ww_metric_id = -1;
int false_rw_metric_id = -1;
int false_wr_metric_id = -1;
int true_ww_metric_id = -1;
int true_rw_metric_id = -1;
int true_wr_metric_id = -1;
double amd_global_sampling_period = 0;

int temporal_reuse_metric_id = -1;
int spatial_reuse_metric_id = -1;
int l3_temporal_reuse_metric_id = -1;
int l3_spatial_reuse_metric_id = -1;
int comm_temporal_reuse_metric_id = -1;
int comm_spatial_reuse_metric_id = -1;
int reuse_time_distance_metric_id = -1; // use rdtsc() to represent the reuse distance
int reuse_time_distance_count_metric_id = -1; // how many times reuse_time_distance_metric is incremented
int reuse_memory_distance_metric_id = -1; // use Loads+stores to reprent the reuse distance
int reuse_memory_distance_count_metric_id = -1; // how many times reuse_memory_distance_metric is incremented
int l3_reuse_memory_distance_metric_id = -1; // use Loads+stores to reprent the reuse distance
int l3_reuse_memory_distance_count_metric_id = -1; // how many times reuse_memory_distance_metric is incremented
int comm_reuse_memory_distance_metric_id = -1; // use Loads+stores to reprent the reuse distance
int comm_reuse_memory_distance_count_metric_id = -1; // how many times reuse_memory_distance_metric is incremented
int reuse_buffer_metric_ids[2] = {-1, -1}; // used to store temporal data for reuse client
int reuse_store_buffer_metric_id = -1; // store the last time we get an available value of stores

int *reuse_distance_events = NULL;
int reuse_distance_num_events = 0;
int l3_reuse_distance_event = 0;
int amd_reuse_distance_event = 0;
int l3_reuse_distance_event_rqsts = 0;

uint64_t inter_thread_invalidation_count = 0;
uint64_t inter_core_invalidation_count = 0;
uint64_t l3_coherence_miss_count = 0;

long global_l2_miss_sampling_period;

extern int global_thread_count;
extern int dynamic_global_thread_count;

int global_load_count = 0;
__thread int load_count = 0;
__thread int store_count = 0;
__thread int addr_valid_count = 0;
__thread int phy_addr_valid_count = 0;
__thread int micro_op_sample = 0;
__thread int mem_access_sample = 0;
__thread int valid_mem_access_sample = 0;
//uint64_t global_store_count = 0;
uint64_t sample_count_counter = 0;
extern uint64_t numWatchpointArmingAttempt[MAX_WP_SLOTS];

#ifdef REUSE_HISTO
bool reuse_output_trace = false;
double reuse_bin_start = 0;
double reuse_bin_ratio = 0;
uint64_t * reuse_bin_list = NULL;
double * reuse_bin_pivot_list = NULL; // store the bin intervals
int reuse_bin_size = 0;

uint64_t shared_reuse_counter = 0;
uint64_t * shared_reuse_bin_list = NULL;
double * shared_reuse_bin_pivot_list = NULL; // store the bin intervals
int shared_reuse_bin_size = 0;

uint64_t * communication_reuse_bin_list = NULL;
double * communication_reuse_bin_pivot_list = NULL; // store the bin intervals
int communication_reuse_bin_size = 0;

#else
#endif

extern WPConfig_t wpConfig;

AccessType reuse_monitor_type = LOAD_AND_STORE; // WP_REUSE: what kind of memory access can be used to subscribe the watchpoint
WatchPointType reuse_trap_type = WP_RW; // WP_REUSE: what kind of memory access can trap the watchpoint
ReuseType reuse_profile_type = REUSE_SPATIAL; // WP_REUSE: we want to collect temporal reuse, spatial reuse OR both?
bool reuse_concatenate_use_reuse = false; // WP_REUSE: how to concatentate the use and reuse
int profiling_mode = L1;
//#endif

#define NUM_WATERMARK_METRICS (4)
int curWatermarkId = 0;
int watermark_metric_id[NUM_WATERMARK_METRICS] = {-1, -1, -1, -1};
int pebs_metric_id[NUM_WATERMARK_METRICS] = {-1, -1, -1, -1};

extern long load_and_store_all_load;
extern long load_and_store_all_store;
extern long store_all_store;
extern __thread uint64_t create_wp_count;
extern __thread uint64_t arm_wp_count;
extern __thread uint64_t sub_wp_count1;
extern __thread uint64_t sub_wp_count2;
extern __thread uint64_t sub_wp_count3;
extern __thread uint64_t overlap_count;
extern __thread uint64_t none_available_count;
extern __thread uint64_t wp_count;
extern __thread uint64_t wp_count1;
extern __thread uint64_t wp_count2;
extern __thread uint64_t wp_dropped;
extern __thread uint64_t wp_active;

extern globalReuseTable_t globalReuseWPs;
extern globalReuseTable_t globalStoreReuseWPs;

__thread uint64_t prev_sample_timestamp = 0;
__thread uint64_t last_sample_timestamp = 0;

void SetupWatermarkMetric(int metricId){
  if (curWatermarkId == NUM_WATERMARK_METRICS) {
    EEMSG("curWatermarkId == NUM_WATERMARK_METRICS = %d", NUM_WATERMARK_METRICS);
    monitor_real_abort();
  }
  pebs_metric_id[curWatermarkId]=metricId;
  watermark_metric_id[curWatermarkId] = hpcrun_new_metric();
  char metricName [1000] = "IGNORE_ME";
  sprintf(metricName,"%s_%d","IGNORE_ME", metricId);
  hpcrun_set_metric_info_and_period(watermark_metric_id[curWatermarkId], strdup(metricName) /*never freed */, MetricFlags_ValFmt_Real, 1, metric_property_none);
  curWatermarkId++;
}

static dso_info_t * hpcrunLM;
static dso_info_t * libmonitorLM;

typedef struct WPStats{
  long numImpreciseSamples __attribute__((aligned(CACHE_LINE_SZ)));
  long numWatchpointsSet;
  char dummy[CACHE_LINE_SZ];
}WPStats_t;

__thread WPStats_t wpStats;
__thread uint64_t prev_event_count = 0;
uint64_t periodic_l2_load_miss_count = 0;
uint64_t next_periodic_l2_load_miss_count = 0;
uint64_t periodic_l2_store_miss_sample = 0;
uint64_t next_periodic_l2_store_miss_sample = 0;
uint64_t detected_l2_miss_counter = 0;
uint64_t detected_store_counter = 0;

/******************************************************************************
 * macros
 *****************************************************************************/

#define OVERFLOW_MODE 0
#define WEIGHT_METRIC 0
#define DEFAULT_THRESHOLD  2000000L
#define APPROX_RATE (0.01)
#define WP_DEADSPY_EVENT_NAME "WP_DEADSPY"
#define WP_REDSPY_EVENT_NAME "WP_REDSPY"
#define WP_LOADSPY_EVENT_NAME "WP_LOADSPY"
#define WP_REUSE_EVENT_NAME "WP_REUSE"
#define WP_REUSETRACKER_EVENT_NAME "WP_REUSETRACKER"
#define WP_AMD_COMM_EVENT_NAME "WP_AMD_COMM"
#define WP_AMD_REUSE_EVENT_NAME "WP_AMD_REUSE"
#define WP_AMD_REUSETRACKER_EVENT_NAME "WP_AMD_REUSETRACKER"
#define WP_TEMPORAL_REUSE_EVENT_NAME "WP_TEMPORAL_REUSE"
#define WP_SPATIAL_REUSE_EVENT_NAME "WP_SPATIAL_REUSE"
#define WP_FALSE_SHARING_EVENT_NAME "WP_FALSE_SHARING"
#define WP_TRUE_SHARING_EVENT_NAME "WP_TRUE_SHARING"
#define WP_ALL_SHARING_EVENT_NAME "WP_ALL_SHARING"
#define WP_COMDETECTIVE_EVENT_NAME "WP_COMDETECTIVE"
#define WP_IPC_FALSE_SHARING_EVENT_NAME "WP_IPC_FALSE_SHARING"
#define WP_IPC_TRUE_SHARING_EVENT_NAME "WP_IPC_TRUE_SHARING"
#define WP_IPC_ALL_SHARING_EVENT_NAME "WP_IPC_ALL_SHARING"

extern WP_CLIENT_ID event_id;

typedef struct WpClientConfig{
  WP_CLIENT_ID id;
  char * name;
  WatchPointUpCall_t wpCallback;
  WPTriggerActionType preWPAction;
  ClientConfigOverrideCall_t configOverrideCallback;
}WpClientConfig_t;

typedef struct SharedData{
  volatile uint64_t counter __attribute__((aligned(CACHE_LINE_SZ)));
  uint64_t time __attribute__((aligned(CACHE_LINE_SZ)));
  int tid;
  WatchPointType wpType;
  AccessType accessType;
  void *address;
  int accessLen;
  cct_node_t * node;
  char dummy[CACHE_LINE_SZ];
} SharedData_t;

SharedData_t gSharedData = {.counter = 0, .time=0, .wpType = -1, .accessType = UNKNOWN, .tid = -1, .address = 0};

HashTable_t bulletinBoard = {.counter = 0};
ReuseHashTable_t reuseBulletinBoard = {.counter = 0};

__thread uint64_t prev_timestamp = 0;

__thread int64_t lastTime = 0;
__thread int64_t storeLastTime = 0;
__thread int64_t storeOlderTime = 0;
__thread uint64_t writtenBytes = 0;
__thread uint64_t loadedBytes = 0;
__thread uint64_t usedBytes = 0;
__thread uint64_t deadBytes = 0;
__thread uint64_t oldBytes = 0;
__thread uint64_t oldAppxBytes = 0;
__thread uint64_t newBytes = 0;
__thread uint64_t accessedIns = 0;
__thread uint64_t falseWWIns = 0;
__thread uint64_t falseWRIns = 0;
__thread uint64_t falseRWIns = 0;
__thread uint64_t trueWWIns = 0;
__thread uint64_t trueWRIns = 0;
__thread uint64_t trueRWIns = 0;
__thread uint64_t reuse = 0;
__thread uint64_t reuseTemporal = 0;
__thread uint64_t reuseSpatial = 0;
__thread uint64_t mtReuse = 0;
__thread uint64_t mtReuseTemporal = 0;
__thread uint64_t mtReuseSpatial = 0;

// ComDetective stats begin
__thread uint64_t fs_num = 0;
__thread uint64_t inter_core_fs_num = 0;
__thread uint64_t ts_num = 0;
__thread uint64_t inter_core_ts_num = 0;
__thread uint64_t as_num = 0;
__thread uint64_t inter_core_as_num = 0;
__thread uint64_t line_transfer_num = 0;
__thread uint64_t sample_count = 0;
__thread uint64_t valid_sample_count = 1;
__thread uint64_t valid_sample_count1 = 0;
__thread uint64_t valid_sample_count2 = 0;
__thread uint64_t trap_count = 0;
__thread uint64_t wp_arming_count = 0;
// ComDetective stats end

// Some stats
__thread long int correct=0;
__thread long int incorrect=0;
__thread long int st1=0;
__thread long int ld1=0;
__thread long int mix1=0;
__thread long int unk1=0;
__thread long int st2=0;
__thread long int ld2=0;
__thread long int mix2=0;
__thread long int unk2=0;
__thread long difffunc=0;
__thread long samefunc=0;
__thread long unknwfunc=0;
__thread long ipSame=0;
__thread long ipDiff=0;

int event_type = 0;

/* private tool function
 *****************************************************************************/
static int OpenWitchTraceOutput(){
#define OUTPUT_TRACE_BUFFER_SIZE (1 <<10)
  char file_name[PATH_MAX];
  int ret = snprintf(file_name, PATH_MAX, "%s-%u.reuse.hpcrun", hpcrun_files_executable_name(), syscall(SYS_gettid));
  if ( ret < 0 || ret >= PATH_MAX){
    return -1;
  }
  int fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0){
    return -1;
  }
  ret = hpcio_outbuf_attach(&(TD_GET(witch_client_trace_output)), fd, hpcrun_malloc(OUTPUT_TRACE_BUFFER_SIZE), OUTPUT_TRACE_BUFFER_SIZE, HPCIO_OUTBUF_UNLOCKED);
  if (ret != HPCFMT_OK){
    return -1;
  }
  return 0;
}

static void CloseWitchTraceOutput(){
  hpcio_outbuf_t *out_ptr = &(TD_GET(witch_client_trace_output));
  if (out_ptr->fd >= 0){
    hpcio_outbuf_close(out_ptr);
  }
}

static int WriteWitchTraceOutput(const char *fmt, ...){
#define LOCAL_BUFFER_SIZE 1024
  va_list arg;
  char local_buf[LOCAL_BUFFER_SIZE];
  va_start(arg, fmt);
  int data_size = vsnprintf(local_buf, LOCAL_BUFFER_SIZE, fmt, arg);
  va_end(arg);
  if (data_size < 0 && data_size >= LOCAL_BUFFER_SIZE){
    return -1;
  }
  int ret = hpcio_outbuf_write(&(TD_GET(witch_client_trace_output)), local_buf, data_size);
  if (ret != data_size){
    return -1;
  }
  return 0;
}

int hashCode(void * key) {
  return (uint64_t) key % 54121 % HASHTABLESIZE;
}

#ifdef MULTITHREAD_REUSE_HISTO

ReuseBBEntry_t getEntryFromReuseBulletinBoard(void * cacheLineBaseAddress, int * item_not_found) {
  int hashIndex = hashCode(cacheLineBaseAddress);
  //fprintf(stderr, "cacheLineBaseAddress: %lx and reuseBulletinBoard.hashTable[hashIndex].cacheLineBaseAddress: %lx\n", cacheLineBaseAddress, reuseBulletinBoard.hashTable[hashIndex].cacheLineBaseAddress);
  if(cacheLineBaseAddress != reuseBulletinBoard.hashTable[hashIndex].cacheLineBaseAddress)
    *item_not_found = 1;
  return reuseBulletinBoard.hashTable[hashIndex];
}

void reuseHashInsert(ReuseBBEntry_t item) {
  void * cacheLineBaseAddress = item.cacheLineBaseAddress;
  int hashIndex = hashCode(cacheLineBaseAddress);
  //fprintf(stderr, "cache line %lx is inserted to index %d\n", (long) cacheLineBaseAddress, hashIndex);
  //if (reuseBulletinBoard.hashTable[hashIndex].cacheLineBaseAddress == -1) {
  if((reuseBulletinBoard.counter & 1) == 0)
  {
    uint64_t theCounter = reuseBulletinBoard.counter;
    if(__sync_bool_compare_and_swap(&reuseBulletinBoard.counter, theCounter, theCounter+1)){
      reuseBulletinBoard.hashTable[hashIndex] = item;
      __sync_synchronize();
      reuseBulletinBoard.counter++;
    }
  }
}

void prettyPrintReuseHash() {
  for(int i = 0; i < HASHTABLESIZE; i++) {
    fprintf(stderr, "reuseBulletinBoard.hashTable[%d].cacheLineBaseAddress: %lx, tid: %d, core id: %d, access type: %s, time: %ld\n", i, (long) reuseBulletinBoard.hashTable[i].cacheLineBaseAddress, (int) reuseBulletinBoard.hashTable[i].tid, (int) reuseBulletinBoard.hashTable[i].core_id, reuseBulletinBoard.hashTable[i].accessType == LOAD ? "LOAD": (reuseBulletinBoard.hashTable[i].accessType == STORE ? "STORE" : (reuseBulletinBoard.hashTable[i].accessType == LOAD_AND_STORE ? "LOAD_AND_STORE": "UNKNOWN")), reuseBulletinBoard.hashTable[i].time);
  }
}

#endif

#ifdef REUSE_HISTO
void ExpandReuseBinList(){
  // each time we double the size of reuse_bin_list
  uint64_t *old_reuse_bin_list = reuse_bin_list;
  double *old_reuse_bin_pivot_list = reuse_bin_pivot_list;
  int old_reuse_bin_size = reuse_bin_size;
  reuse_bin_size *= 2;

  reuse_bin_list = hpcrun_malloc(sizeof(uint64_t) * reuse_bin_size);
  memset(reuse_bin_list, 0, sizeof(uint64_t) * reuse_bin_size);
  memcpy(reuse_bin_list, old_reuse_bin_list, sizeof(uint64_t) * old_reuse_bin_size);

  reuse_bin_pivot_list = hpcrun_malloc(sizeof(double) * reuse_bin_size);
  memset(reuse_bin_pivot_list, 0, sizeof(double) * reuse_bin_size);
  memcpy(reuse_bin_pivot_list, old_reuse_bin_pivot_list, sizeof(double) * old_reuse_bin_size);
  for(int i=old_reuse_bin_size; i < reuse_bin_size; i++){
    reuse_bin_pivot_list[i] = reuse_bin_pivot_list[i-1] * reuse_bin_ratio;
  }

  //hpcrun_free(old_reuse_bin_list);
  //hpcrun_free(old_reuse_bin_pivot_list);
}

int FindReuseBinIndex(uint64_t distance){
  //fprintf(stderr, "distance: %ld, reuse_bin_pivot_list[0]: %0.2lf\n", distance, reuse_bin_pivot_list[0]);
  if (distance < reuse_bin_pivot_list[0]){
    //fprintf(stderr, "reuse_bin_pivot_list[0]: %0.2lf\n", reuse_bin_pivot_list[0]);
    return 0;
  }
  if (distance >= reuse_bin_pivot_list[reuse_bin_size - 1]){
    ExpandReuseBinList();
    return FindReuseBinIndex(distance);
  }

  int left = 0, right = reuse_bin_size - 1;
  while(left + 1 < right){
    int mid = (left + right) / 2;
    //fprintf(stderr, "distance: %ld, reuse_bin_pivot_list[%d]: %0.2lf\n", distance, mid, reuse_bin_pivot_list[mid]);
    if ( distance < reuse_bin_pivot_list[mid]){
      right = mid;
    } else {
      left = mid;
    }
  }
  assert(left + 1 == right);
  return left + 1;
}

void ReuseAddDistance(uint64_t distance, uint64_t inc ){
  int index = FindReuseBinIndex(distance);
  reuse_bin_list[index] += inc;
  //fprintf(stderr, "distance %ld has happened %ld times with index %d\n", distance, inc, index);
}

void ExpandSharedReuseBinList(){
  // each time we double the size of reuse_bin_list
  uint64_t *old_reuse_bin_list = shared_reuse_bin_list;
  double *old_reuse_bin_pivot_list = shared_reuse_bin_pivot_list;
  int old_reuse_bin_size = shared_reuse_bin_size;
  shared_reuse_bin_size *= 2;

  shared_reuse_bin_list = hpcrun_malloc(sizeof(uint64_t) * shared_reuse_bin_size);
  memset(shared_reuse_bin_list, 0, sizeof(uint64_t) * shared_reuse_bin_size);
  memcpy(shared_reuse_bin_list, old_reuse_bin_list, sizeof(uint64_t) * old_reuse_bin_size);

  shared_reuse_bin_pivot_list = hpcrun_malloc(sizeof(double) * shared_reuse_bin_size);
  memset(shared_reuse_bin_pivot_list, 0, sizeof(double) * shared_reuse_bin_size);
  memcpy(shared_reuse_bin_pivot_list, old_reuse_bin_pivot_list, sizeof(double) * old_reuse_bin_size);
  for(int i=old_reuse_bin_size; i < shared_reuse_bin_size; i++){
    shared_reuse_bin_pivot_list[i] = shared_reuse_bin_pivot_list[i-1] * reuse_bin_ratio;
  }

  //hpcrun_free(old_reuse_bin_list);
  //hpcrun_free(old_reuse_bin_pivot_list);
}

int FindSharedReuseBinIndex(uint64_t distance){
  //fprintf(stderr, "distance: %ld, reuse_bin_pivot_list[0]: %0.2lf\n", distance, reuse_bin_pivot_list[0]);
  if (distance < shared_reuse_bin_pivot_list[0]){
    //fprintf(stderr, "reuse_bin_pivot_list[0]: %0.2lf\n", reuse_bin_pivot_list[0]);
    return 0;
  }
  if (distance >= shared_reuse_bin_pivot_list[shared_reuse_bin_size - 1]){
    ExpandSharedReuseBinList();
    return FindSharedReuseBinIndex(distance);
  }

  int left = 0, right = shared_reuse_bin_size - 1;
  while(left + 1 < right){
    int mid = (left + right) / 2;
    //fprintf(stderr, "distance: %ld, reuse_bin_pivot_list[%d]: %0.2lf\n", distance, mid, reuse_bin_pivot_list[mid]);
    if ( distance < shared_reuse_bin_pivot_list[mid]){
      right = mid;
    } else {
      left = mid;
    }
  }
  assert(left + 1 == right);
  return left + 1;
}

void SharedReuseAddDistance(uint64_t distance, uint64_t inc ){
  int index = FindSharedReuseBinIndex(distance);
  shared_reuse_bin_list[index] += inc;
  //fprintf(stderr, "distance %ld has happened %ld times with index %d\n", distance, inc, index);
}

void ExpandCommunicationReuseBinList(){
  // each time we double the size of reuse_bin_list
  uint64_t *old_reuse_bin_list = communication_reuse_bin_list;
  double *old_reuse_bin_pivot_list = communication_reuse_bin_pivot_list;
  int old_reuse_bin_size = communication_reuse_bin_size;
  communication_reuse_bin_size *= 2;

  communication_reuse_bin_list = hpcrun_malloc(sizeof(uint64_t) * communication_reuse_bin_size);
  memset(communication_reuse_bin_list, 0, sizeof(uint64_t) * communication_reuse_bin_size);
  memcpy(communication_reuse_bin_list, old_reuse_bin_list, sizeof(uint64_t) * old_reuse_bin_size);

  communication_reuse_bin_pivot_list = hpcrun_malloc(sizeof(double) * communication_reuse_bin_size);
  memset(communication_reuse_bin_pivot_list, 0, sizeof(double) * communication_reuse_bin_size);
  memcpy(communication_reuse_bin_pivot_list, old_reuse_bin_pivot_list, sizeof(double) * old_reuse_bin_size);
  for(int i=old_reuse_bin_size; i < communication_reuse_bin_size; i++){
    communication_reuse_bin_pivot_list[i] = communication_reuse_bin_pivot_list[i-1] * reuse_bin_ratio;
  }

  //hpcrun_free(old_reuse_bin_list);
  //hpcrun_free(old_reuse_bin_pivot_list);
}

int FindCommunicationReuseBinIndex(uint64_t distance){
  //fprintf(stderr, "distance: %ld, reuse_bin_pivot_list[0]: %0.2lf\n", distance, reuse_bin_pivot_list[0]);
  if (distance < communication_reuse_bin_pivot_list[0]){
    //fprintf(stderr, "reuse_bin_pivot_list[0]: %0.2lf\n", reuse_bin_pivot_list[0]);
    return 0;
  }
  if (distance >= communication_reuse_bin_pivot_list[communication_reuse_bin_size - 1]){
    ExpandSharedReuseBinList();
    return FindSharedReuseBinIndex(distance);
  }

  int left = 0, right = communication_reuse_bin_size - 1;
  while(left + 1 < right){
    int mid = (left + right) / 2;
    //fprintf(stderr, "distance: %ld, reuse_bin_pivot_list[%d]: %0.2lf\n", distance, mid, reuse_bin_pivot_list[mid]);
    if ( distance < communication_reuse_bin_pivot_list[mid]){
      right = mid;
    } else {
      left = mid;
    }
  }
  assert(left + 1 == right);
  return left + 1;
}

void CommunicationReuseAddDistance(uint64_t distance, uint64_t inc ){
  int index = FindSharedReuseBinIndex(distance);
  communication_reuse_bin_list[index] += inc;
  //fprintf(stderr, "distance %ld has happened %ld times with index %d\n", distance, inc, index);
}

void ReuseSubDistance(uint64_t distance, uint64_t dec ){
  int index = FindReuseBinIndex(distance);
  reuse_bin_list[index] -= dec;
  //fprintf(stderr, "distance %ld has happened %ld times with index %d\n", distance, inc, index);
}
#endif

/******************************************************************************
 * sample source registration
 *****************************************************************************/

// Support for derived events (proxy sampling).
//static int derived[MAX_EVENTS];
//static int some_overflow;


/******************************************************************************
 * method functions
 *****************************************************************************/

static WPTriggerActionType DeadStoreWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType RedStoreWPCallback(WatchPointInfo_t *wpi, int startOffseti, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType TemporalReuseWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType ReuseWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType ReuseTrackerWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType SpatialReuseWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType LoadLoadWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType FalseSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType ComDetectiveWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType AMDCommWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType AMDReuseWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType AMDReuseTrackerWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType AllSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType TrueSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType IPCFalseSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType IPCAllSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
static WPTriggerActionType IPCTrueSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);


static WpClientConfig_t wpClientConfig[] = {
  /**** DeadSpy ***/
  {
    .id = WP_DEADSPY,
    .name = WP_DEADSPY_EVENT_NAME,
    .wpCallback = DeadStoreWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = NULL
  },
  /**** RedSpy ***/
  {
    .id = WP_REDSPY,
    .name = WP_REDSPY_EVENT_NAME,
    .wpCallback = RedStoreWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = RedSpyWPConfigOverride
  },
  /**** LoadSpy ***/
  {
    .id = WP_LOADSPY,
    .name = WP_LOADSPY_EVENT_NAME,
    .wpCallback = LoadLoadWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = LoadSpyWPConfigOverride
  },
  /**** Temporal Reuse ***/
  {
    .id = WP_TEMPORAL_REUSE,
    .name = WP_TEMPORAL_REUSE_EVENT_NAME,
    .wpCallback = TemporalReuseWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = TemporalReuseWPConfigOverride
  },
  /**** Spatial Reuse ***/
  {
    .id = WP_SPATIAL_REUSE,
    .name = WP_SPATIAL_REUSE_EVENT_NAME,
    .wpCallback = SpatialReuseWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = SpatialReuseWPConfigOverride
  },
  /**** False Sharing ***/
  {
    .id = WP_FALSE_SHARING,
    .name = WP_FALSE_SHARING_EVENT_NAME,
    .wpCallback = FalseSharingWPCallback,
    .preWPAction = DISABLE_ALL_WP,
    .configOverrideCallback = FalseSharingWPConfigOverride
  },
  /**** All Sharing ***/
  {
    .id = WP_ALL_SHARING,
    .name = WP_ALL_SHARING_EVENT_NAME,
    .wpCallback = AllSharingWPCallback,
    .preWPAction = DISABLE_ALL_WP,
    .configOverrideCallback = AllSharingWPConfigOverride
  },
  {    
    .id = WP_COMDETECTIVE,
    .name = WP_COMDETECTIVE_EVENT_NAME,
    .wpCallback = ComDetectiveWPCallback,
    .preWPAction = DISABLE_ALL_WP,
    .configOverrideCallback = ComDetectiveWPConfigOverride
  },
  /**** Reuse ***/
  {
    .id = WP_REUSE,
    .name = WP_REUSE_EVENT_NAME,
    .wpCallback = ReuseWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = ReuseWPConfigOverride
  },
  /**** Multithreaded Reuse ***/
  {
    .id = WP_REUSETRACKER,
    .name = WP_REUSETRACKER_EVENT_NAME,
    .wpCallback = ReuseTrackerWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = ReuseWPConfigOverride
  },
  /**** AMD Communication ***/
  {
    .id = WP_AMD_COMM,
    .name = WP_AMD_COMM_EVENT_NAME,
    .wpCallback = AMDCommWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = AMDCommWPConfigOverride
  },
  /**** AMD Communication ***/
  {
    .id = WP_AMD_REUSE,
    .name = WP_AMD_REUSE_EVENT_NAME,
    .wpCallback = AMDReuseWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = AMDReuseWPConfigOverride
  },
  /**** AMD Communication ***/
  {
    .id = WP_AMD_REUSETRACKER,
    .name = WP_AMD_REUSETRACKER_EVENT_NAME,
    .wpCallback = AMDReuseTrackerWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = AMDReuseTrackerWPConfigOverride
  },
  /**** Contention ***/
  {
    .id = WP_TRUE_SHARING,
    .name = WP_TRUE_SHARING_EVENT_NAME,
    .wpCallback = TrueSharingWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = TrueSharingWPConfigOverride
  },
  /**** IPC False Sharing ***/
  {
    .id = WP_IPC_FALSE_SHARING,
    .name = WP_IPC_FALSE_SHARING_EVENT_NAME,
    .wpCallback = IPCFalseSharingWPCallback,
    .preWPAction = DISABLE_ALL_WP,
    .configOverrideCallback = IPCFalseSharingWPConfigOverride
  },
  /**** IPC All Sharing ***/
  {
    .id = WP_IPC_ALL_SHARING,
    .name = WP_IPC_ALL_SHARING_EVENT_NAME,
    .wpCallback = IPCAllSharingWPCallback,
    .preWPAction = DISABLE_ALL_WP,
    .configOverrideCallback = IPCAllSharingWPConfigOverride
  },
  /**** IPC Contention ***/
  {
    .id = WP_IPC_TRUE_SHARING,
    .name = WP_IPC_TRUE_SHARING_EVENT_NAME,
    .wpCallback = IPCTrueSharingWPCallback,
    .preWPAction = DISABLE_WP,
    .configOverrideCallback = IPCTrueSharingWPConfigOverride
  }

};


static WpClientConfig_t * theWPConfig = NULL;

bool WatchpointClientActive(){
  return theWPConfig != NULL;
}

#define MAX_BLACK_LIST_ADDRESS (1024)

typedef struct BlackListAddressRange{
  void * startAddr;
  void * endAddr;
}BlackListAddressRange_t;
static BlackListAddressRange_t blackListAddresses [MAX_BLACK_LIST_ADDRESS];
static uint16_t numBlackListAddresses = 0;

static const char * blackListedModules[] = {"libmonitor.so", "libhpcrun.so", "libpfm.so", "libxed.so", "libpapi.so", "anon_inode:[perf_event]"};
static const int  numblackListedModules = 6;
static spinlock_t blackListLock = SPINLOCK_UNLOCKED;



static void PopulateBlackListAddresses() {
  spinlock_lock(&blackListLock);
  if(numBlackListAddresses == 0) {
    FILE* loadmap = fopen("/proc/self/maps", "r");
    if (! loadmap) {
      EMSG("Could not open /proc/self/maps");
      return;
    }
    char linebuf[1024 + 1];
    char tmpname[PATH_MAX];
    char* addr = NULL;
    for(;;) {
      char* l = fgets(linebuf, sizeof(linebuf), loadmap);
      if (feof(loadmap)) break;
      char* save = NULL;
      const char delim[] = " \n";
      addr = strtok_r(l, delim, &save);
      char* perms = strtok_r(NULL, delim, &save);
      // skip 3 tokens
      for (int i=0; i < 3; i++) { (void) strtok_r(NULL, delim, &save);}
      char* name = strtok_r(NULL, delim, &save);
      realpath(name, tmpname);
      for(int i = 0; i < numblackListedModules; i++) {
        if (strstr(tmpname, blackListedModules[i])){
          char* save = NULL;
          const char dash[] = "-";
          char* start_str = strtok_r(addr, dash, &save);
          char* end_str   = strtok_r(NULL, dash, &save);
          void *start = (void*) (uintptr_t) strtol(start_str, NULL, 16);
          void *end   = (void*) (uintptr_t) strtol(end_str, NULL, 16);
          blackListAddresses[numBlackListAddresses].startAddr = start;
          blackListAddresses[numBlackListAddresses].endAddr = end;
          numBlackListAddresses++;
        }
      }
    }
    fclose(loadmap);
    // No TLS
    extern void * __tls_get_addr (void *);
    blackListAddresses[numBlackListAddresses].startAddr = (void *) (((uint64_t) __tls_get_addr) - 1000);
    blackListAddresses[numBlackListAddresses].endAddr = ((void *)__tls_get_addr) + 1000;
    numBlackListAddresses++;

    // No first page
    blackListAddresses[numBlackListAddresses].startAddr = 0 ;
    blackListAddresses[numBlackListAddresses].endAddr = (void*) sysconf(_SC_PAGESIZE);
    numBlackListAddresses++;
  }
  spinlock_unlock(&blackListLock);
}


  static void
METHOD_FN(init)
{
  self->state = INIT;
}

  static void
METHOD_FN(thread_init)
{
  TMSG(WATCHPOINT, "thread init");
  TMSG(WATCHPOINT, "thread init OK");
}

  static void
METHOD_FN(thread_init_action)
{
  TMSG(WATCHPOINT, "register thread");
  wpStats.numImpreciseSamples = 0;
  wpStats.numWatchpointsSet = 0;
  WatchpointThreadInit(theWPConfig->wpCallback);
  TMSG(WATCHPOINT, "register thread ok");

}

  static void
METHOD_FN(start)
{
  thread_data_t* td = hpcrun_get_thread_data();
  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  if (my_state == START) {
    TMSG(WATCHPOINT,"*NOTE* WATCHPOINT start called when already in state START");
    return;
  }
  td->ss_state[self->sel_idx] = START;
  /*#ifdef REUSE_HISTO
    assert(OpenWitchTraceOutput()==0);
#endif*/
}

static void ClientTermination(){
  // Cleanup the watchpoint data
  //fprintf(stderr, "ClientTermination is executed here\n");
  hpcrun_stats_num_samples_imprecise_inc(wpStats.numImpreciseSamples);
  hpcrun_stats_num_watchpoints_set_inc(wpStats.numWatchpointsSet);
  WatchpointThreadTerminate();
  //fprintf(stderr, "after WatchpointThreadTerminate\n");
  switch (theWPConfig->id) {
    case WP_DEADSPY:
      hpcrun_stats_num_writtenBytes_inc(writtenBytes);
      hpcrun_stats_num_usedBytes_inc(usedBytes);
      hpcrun_stats_num_deadBytes_inc(deadBytes);
      break;
    case WP_REDSPY:
      hpcrun_stats_num_writtenBytes_inc(writtenBytes);
      hpcrun_stats_num_newBytes_inc(newBytes);
      hpcrun_stats_num_oldBytes_inc(oldBytes);
      hpcrun_stats_num_oldAppxBytes_inc(oldAppxBytes);
      break;
    case WP_LOADSPY:
      hpcrun_stats_num_loadedBytes_inc(loadedBytes);
      hpcrun_stats_num_newBytes_inc(newBytes);
      hpcrun_stats_num_oldBytes_inc(oldBytes);
      hpcrun_stats_num_oldAppxBytes_inc(oldAppxBytes);
      break;
    case WP_TEMPORAL_REUSE:
      hpcrun_stats_num_accessedIns_inc(accessedIns);
      hpcrun_stats_num_reuse_inc(reuse);
      break;
    case WP_SPATIAL_REUSE:
      hpcrun_stats_num_accessedIns_inc(accessedIns);
      hpcrun_stats_num_reuse_inc(reuse);
      break;
    case WP_FALSE_SHARING:
    case WP_IPC_FALSE_SHARING:
      hpcrun_stats_num_accessedIns_inc(accessedIns);
      hpcrun_stats_num_falseWWIns_inc(falseWWIns);
      hpcrun_stats_num_falseRWIns_inc(falseRWIns);
      hpcrun_stats_num_falseWRIns_inc(falseWRIns);
      //fprintf(stderr, "sample_count: %ld\n", sample_count);
      break;
    case WP_TRUE_SHARING:
    case WP_IPC_TRUE_SHARING:
      hpcrun_stats_num_accessedIns_inc(accessedIns);
      hpcrun_stats_num_trueWWIns_inc(trueWWIns);
      hpcrun_stats_num_trueRWIns_inc(trueRWIns);
      hpcrun_stats_num_trueWRIns_inc(trueWRIns);
      break;
    case WP_REUSE:
      {
#ifdef REUSE_HISTO
        uint64_t val[3];
        //fprintf(stderr, "FINAL_COUNTING:");
        if (reuse_output_trace == false){ //dump the bin info
          //fprintf(stderr, "the bin info is dumped\n");
          WriteWitchTraceOutput("BIN_START: %lf\n", reuse_bin_start);
          WriteWitchTraceOutput("BIN_RATIO: %lf\n", reuse_bin_ratio);

          for(int i=0; i < reuse_bin_size; i++){
            WriteWitchTraceOutput("BIN: %d %lu\n", i, reuse_bin_list[i]);
          }
        }

        WriteWitchTraceOutput("FINAL_COUNTING:");
        for (int i=0; i < MIN(2,reuse_distance_num_events); i++){
          assert(linux_perf_read_event_counter(reuse_distance_events[i], val) >= 0);
          //fprintf(stderr, " %lu %lu %lu,", val[0], val[1], val[2]);//jqswang
          WriteWitchTraceOutput(" %lu %lu %lu,", val[0], val[1], val[2]);
        }
        //fprintf(stderr, "\n");
        WriteWitchTraceOutput("\n");
        //close the trace output
        CloseWitchTraceOutput();
#endif
        hpcrun_stats_num_accessedIns_inc(accessedIns);
        hpcrun_stats_num_reuseTemporal_inc(reuseTemporal);
        hpcrun_stats_num_reuseSpatial_inc(reuseSpatial);
      }   break;
    case WP_AMD_REUSE:
    case WP_AMD_REUSETRACKER:
    case WP_REUSETRACKER:
      {
        /*
#ifdef REUSE_HISTO
uint64_t val[3];
        //fprintf(stderr, "FINAL_COUNTING:");
        if (reuse_output_trace == false){ //dump the bin info
        //fprintf(stderr, "the bin info is dumped\n");
        //fprintf(stderr, "inter_thread_invalidation_count: %ld\n", inter_thread_invalidation_count);
        //fprintf(stderr, "inter_core_invalidation_count: %ld\n", inter_core_invalidation_count);
        WriteWitchTraceOutput("BIN_START: %lf\n", reuse_bin_start);
        WriteWitchTraceOutput("BIN_RATIO: %lf\n", reuse_bin_ratio);

        for(int i=0; i < reuse_bin_size; i++){
        WriteWitchTraceOutput("BIN: %d %lu\n", i, reuse_bin_list[i]);
        }
        }

        //fprintf(stderr, "inter_thread_invalidation_count: %ld\n", inter_thread_invalidation_count);
        //fprintf(stderr, "inter_core_invalidation_count: %ld\n", inter_core_invalidation_count);
        WriteWitchTraceOutput("COHERENCE_MISS:");
        WriteWitchTraceOutput(" %ld\n", l3_coherence_miss_count);
        WriteWitchTraceOutput("FINAL_COUNTING:");
        for (int i=0; i < MIN(2,reuse_distance_num_events); i++){
        assert(linux_perf_read_event_counter(reuse_distance_events[i], val) >= 0);
        //fprintf(stderr, " %lu %lu %lu,", val[0], val[1], val[2]);//jqswang
        WriteWitchTraceOutput(" %lu %lu %lu,", val[0], val[1], val[2]);
        }
        //fprintf(stderr, "\n");
        WriteWitchTraceOutput("\n");
        //close the trace output
        CloseWitchTraceOutput();
#endif
         */
#if 0
	fprintf(stderr, "original_sample_count: %ld\n", original_sample_count);
	fprintf(stderr, "valid_sample_count1: %ld\n", valid_sample_count1);
	fprintf(stderr, "valid_sample_count2: %ld\n", valid_sample_count2);
	fprintf(stderr, "sample_count: %ld\n", sample_count);
	fprintf(stderr, "micro_op_sample: %ld\n", micro_op_sample);
	fprintf(stderr, "mem_access_sample: %ld\n", mem_access_sample);
	fprintf(stderr, "valid_mem_access_sample: %ld\n", valid_mem_access_sample);
#endif
        hpcrun_stats_num_accessedIns_inc(accessedIns);
        hpcrun_stats_num_reuseTemporal_inc(mtReuseTemporal);
        hpcrun_stats_num_reuseSpatial_inc(mtReuseSpatial);
      }   break;
    case WP_AMD_COMM:
    case WP_ALL_SHARING:
    case WP_COMDETECTIVE:
    case WP_IPC_ALL_SHARING:
      hpcrun_stats_num_accessedIns_inc(accessedIns);
      hpcrun_stats_num_falseWWIns_inc(falseWWIns);
      hpcrun_stats_num_falseRWIns_inc(falseRWIns);
      hpcrun_stats_num_falseWRIns_inc(falseWRIns);
      hpcrun_stats_num_trueWWIns_inc(trueWWIns);
      hpcrun_stats_num_trueRWIns_inc(trueRWIns);
      hpcrun_stats_num_trueWRIns_inc(trueWRIns);
#if 0
      fprintf(stderr, "original_sample_count: %ld\n", original_sample_count);
      fprintf(stderr, "valid_sample_count: %ld\n", valid_sample_count);
      fprintf(stderr, "valid_sample_count1: %ld\n", valid_sample_count1);
      fprintf(stderr, "sample_count: %ld\n", sample_count);
      fprintf(stderr, "load_count: %d\n", load_count);
      fprintf(stderr, "store_count: %d\n", store_count);
      fprintf(stderr, "addr_valid_count: %d\n", addr_valid_count);
      fprintf(stderr, "phy_addr_valid_count: %d\n", phy_addr_valid_count);
#endif
    default:
      break;
  }
}

  static void
METHOD_FN(thread_fini_action)
{
  TMSG(WATCHPOINT, "unregister thread");
}

#define N 100000
cct_node_t *topNNode[N]={NULL};

  static void
TopN(cct_node_t* node, cct_op_arg_t arg, size_t level)
{
  int i, t;
  uint64_t min;
  int metricID = (int)arg;
  if (node) {
    metric_set_t *set = hpcrun_get_metric_set(node);
    if (!set) return;
    hpcrun_metricVal_t *loc = hpcrun_metric_set_loc(set, metricID);
    if (!loc) return;

    uint64_t val = loc->i;
    if (val == 0) return;

    for (i=0; i<N; i++) {
      if (!topNNode[i]) {
        topNNode[i] = node;
        break;
      }
    }
    // if no empty slot
    if (i == N) {
      min = ULLONG_MAX;
      for (i=0; i<N; i++) {
        metric_set_t *seti = hpcrun_get_metric_set(topNNode[i]);
        hpcrun_metricVal_t *loci = hpcrun_metric_set_loc(seti, metricID);
        if (loci->i < min) {
          t = i;
          min = loci->i;
        }
      }
      if (val > min) topNNode[t] = node;
    }
  }
}

  static void
PrintTopN(int metricID)
{
  FILE *fd;
  char default_path[PATH_MAX];
  thread_data_t *td = hpcrun_get_thread_data();
  cct_node_t *root = td->core_profile_trace_data.epoch->csdata.tree_root;
  //TODO: partial? cct_node_t *partial = td->core_profile_trace_data.epoch->csdata.partial_unw_root;

  // trave root first and then partial second
  hpcrun_cct_walk_node_1st(root, TopN, (void *) metricID);

  int i, j;
  for (i=0; i<N; i++) {
    cct_node_t *node1 = topNNode[i];
    if (!node1) goto end;
    metric_set_t *set1 = hpcrun_get_metric_set(node1);
    hpcrun_metricVal_t *loc1 = hpcrun_metric_set_loc(set1, metricID);
    uint64_t val1 = loc1->i;
    for (j = i+1; j<N; j++) {
      cct_node_t *node2 = topNNode[j];
      if (!node2) break;
      metric_set_t *set2 = hpcrun_get_metric_set(node2);
      hpcrun_metricVal_t *loc2 = hpcrun_metric_set_loc(set2, metricID);
      uint64_t val2 = loc2->i;

      if (val2 > val1) {
        cct_node_t *tmp = topNNode[i];
        topNNode[i] = topNNode[j];
        topNNode[j] = tmp;
        val1 = val2;
      }
    }
  }
end:
  ;
  char *path = getenv(HPCRUN_OUT_PATH);
  if (path == NULL || strlen(path) == 0) {
    sprintf(default_path, "./hpctoolkit-%s-measurements", hpcrun_files_executable_name());
    path = default_path;
  }
  sprintf(path, "%s/%s", path, "topN.log");

  fd = fopen(path, "a+");

  int libmonitorId, libhpcrunId;
  // print loadmodule info first
  fprintf (fd, "<LOADMODULES>\n");
  hpcrun_loadmap_t *current_loadmap = td->core_profile_trace_data.epoch->loadmap;
  for (load_module_t* lm_src = current_loadmap->lm_end;
      (lm_src); lm_src = lm_src->prev)
  {
    if (strstr(lm_src->name, "libmonitor")) libmonitorId = lm_src->id;
    if (strstr(lm_src->name, "libhpcrun")) libhpcrunId = lm_src->id;
    fprintf(fd, "%d:%p:%s\n", lm_src->id, (void*) lm_src->dso_info->start_to_ref_dist, lm_src->name);
  }
  fprintf (fd, "</LOADMODULES>\n");
  fprintf (fd, "<TOPN>\n");
  // sort the top N from high to low
  for (i=0; i<N; i++) {
    cct_node_t *node = topNNode[i];
    if (!node) break;
    metric_set_t *set = hpcrun_get_metric_set(node);
    hpcrun_metricVal_t *loc = hpcrun_metric_set_loc(set, metricID);
    uint64_t val = loc->i;
    fprintf(fd, "%lu:%lf:", val, (double)val/deadBytes);
    //FIXME: +1 not needed
    fprintf(fd, "%d-%p", hpcrun_cct_addr(node)->ip_norm.lm_id, (void*) (hpcrun_cct_addr(node)->ip_norm.lm_ip+1));
    node = hpcrun_cct_parent(node);
    bool lastWasSeparator=false;
    while (node) {
      if (hpcrun_cct_addr(node)->ip_norm.lm_id == 0){
        break;
      }
      if (hpcrun_cct_addr(node)->ip_norm.lm_id !=  libmonitorId && hpcrun_cct_addr(node)->ip_norm.lm_id != libhpcrunId) {
        // if last node was a separator, +1 here
        if( lastWasSeparator ) {
          //FIXME: +1 not needed
          fprintf(fd, ",%d-%p", hpcrun_cct_addr(node)->ip_norm.lm_id, (void*)(hpcrun_cct_addr(node)->ip_norm.lm_ip+1));
          lastWasSeparator = false;
        } else
          fprintf(fd, ",%d-%p", hpcrun_cct_addr(node)->ip_norm.lm_id, (void*) hpcrun_cct_addr(node)->ip_norm.lm_ip);
      }
      else if (hpcrun_cct_addr(node)->ip_norm.lm_id == libhpcrunId) {
        fprintf(fd, ",SEP");
        lastWasSeparator = true;
      } else ;
      node = hpcrun_cct_parent(node);
    }
    fprintf(fd, "\n");
  }
  fprintf (fd, "</TOPN>\n");
  fclose (fd);
}

  static void
METHOD_FN(stop)
{
  TMSG(WATCHPOINT, "stop");
  //thread_data_t *td = hpcrun_get_thread_data();
  //int nevents = self->evl.nevents;
  source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

  if (my_state == STOP) {
    TMSG(WATCHPOINT,"*NOTE* WATCHPOINT stop called when already in state STOP");
    return;
  }

  if (my_state != START) {
    TMSG(WATCHPOINT,"*WARNING* WATCHPOINT stop called when not in state START");
    return;
  }
  ClientTermination();

  if (ENABLED(PRINTTOPN))
    PrintTopN(dead_metric_id);

  TD_GET(ss_state)[self->sel_idx] = STOP;
}

  static void
METHOD_FN(shutdown)
{
  TMSG(WATCHPOINT, "shutdown");

  METHOD_CALL(self, stop); // make sure stop has been called
  self->state = UNINIT;
}

// Return true if WATCHPOINT recognizes the name, whether supported or not.
// We'll handle unsupported events later.
  static bool
METHOD_FN(supports_event, const char *ev_str)
{
  //fprintf(stderr, "event is checked here\n");
  for(int i = 0; i < WP_MAX_CLIENTS; i++) {
    if (hpcrun_ev_is(ev_str, wpClientConfig[i].name)) {
      //fprintf(stderr, "event is supported\n");
      return true;
    }
  }
  return false;
}

static inline void SetUpFalseSharingMetrics(){
  false_ww_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(false_ww_metric_id, "FALSE_WW_CONFLICT", MetricFlags_ValFmt_Int, 1, metric_property_none);
  false_rw_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(false_rw_metric_id, "FALSE_RW_CONFLICT", MetricFlags_ValFmt_Int, 1, metric_property_none);
  false_wr_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(false_wr_metric_id, "FALSE_WR_CONFLICT", MetricFlags_ValFmt_Int, 1, metric_property_none);
}
static inline void SetUpTrueSharingMetrics(){
  true_ww_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(true_ww_metric_id, "TRUE_WW_CONFLICT", MetricFlags_ValFmt_Int, 1, metric_property_none);
  true_rw_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(true_rw_metric_id, "TRUE_RW_CONFLICT", MetricFlags_ValFmt_Int, 1, metric_property_none);
  true_wr_metric_id = hpcrun_new_metric();
  hpcrun_set_metric_info_and_period(true_wr_metric_id, "TRUE_WR_CONFLICT", MetricFlags_ValFmt_Int, 1, metric_property_none);
}

int locality_vector[4][50];
int l2_locality_vector[HASH_TABLE_SIZE][10];
int thread_to_l3_mapping[HASH_TABLE_SIZE];
int thread_to_l2_mapping[HASH_TABLE_SIZE];
int context_sample_count[256][13][3];
int context_watermark_sample_count[256][13][3];
int l2_count = 0;
int l3_count = 0;

int reading_locality_vector()
{
  int sum = 0;

  char *string = getenv("HPCRUN_THREAD_LOCALITY_MAPPING");

  int l3_size = 1;
  int l2_size = 1;

  if(string != NULL) {
    while (1) {
      char *tail;
      int next;

      /* Skip whitespace by hand, to detect the end.  */
      if (*string == '#') {
        locality_vector[l3_count][0] = l3_size-1;
        l2_locality_vector[l2_count][0] = l2_size-1;
        l3_count++;
        l2_count++;
        l3_size = 1;
        l2_size = 1;
      }
      if (*string == '%') {
        l2_locality_vector[l2_count][0] = l2_size-1;
        l2_count++;
        l2_size = 1;
      }
      while ((*string == ',') || (*string == '#') || (*string == '%')) string++;
      if (*string == 0)
        break;

      /* There is more nonwhitespace,  */
      /* so it ought to be another number.  */
      errno = 0;
      /* Parse it.  */
      next = strtol (string, &tail, 0);
      /* Add it in, if not overflow.  */
      if (errno)
        printf ("Overflow\n");
      else {
        locality_vector[l3_count][l3_size++] = next;
        thread_to_l3_mapping[next] = l3_count;
        l2_locality_vector[l2_count][l2_size++] = next;
        thread_to_l2_mapping[next] = l2_count;
        //printf("%d ", next);
      }
      /* Advance past it.  */
      string = tail;
    }
    //printf("\n");
  }
  locality_vector[l3_count][0] = l3_size-1;
  l2_locality_vector[l2_count][0] = l2_size-1;
  l3_count++;
  l2_count++;
  fprintf(stderr, "l3 affinity:\n");
    for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 40; j++) {
    fprintf(stderr, "%d ", locality_vector[i][j]);
    }
    fprintf(stderr, "\n");
    }
    fprintf(stderr, "l2 affinity:\n");
    for(int i = 0; i < 40; i++) {
    for(int j = 0; j < 10; j++) {
    fprintf(stderr, "%d ", l2_locality_vector[i][j]);
    }
    fprintf(stderr, "\n");
    }
  return 0;
}

  static void
METHOD_FN(process_event_list, int lush_metrics)
{
  // Only one WP client can be active at a time
  if (theWPConfig) {
    EEMSG("Only one watchpoint client can be active at a time \n");
    monitor_real_abort();
  }
  char* evlist = METHOD_CALL(self, get_event_str);
  char* event = start_tok(evlist);

  // only one supported
  for(int i = 0; i < WP_MAX_CLIENTS; i++) {
    if (hpcrun_ev_is(event, wpClientConfig[i].name)) {
      theWPConfig  = &wpClientConfig[i];
      //fprintf(stderr, "theWPConfig is initialized\n");
      if(theWPConfig->id == WP_COMDETECTIVE)
        //fprintf(stderr, "watchpoint client configuration is retrieved and the id is WP_COMDETECTIVE\n");
        break;
    }
  }

  wpStats.numImpreciseSamples = 0;
  wpStats.numWatchpointsSet = 0;
  //fprintf(stderr, "before WatchpointThreadInit\n");
  //WatchpointThreadInit(theWPConfig->wpCallback);
  //fprintf(stderr, "after WatchpointThreadInit\n");
  if(theWPConfig->configOverrideCallback){
    theWPConfig->configOverrideCallback(0);
  }
  event_id = theWPConfig->id;
  WatchpointThreadInit(theWPConfig->wpCallback);

  PopulateBlackListAddresses();

  event_type = theWPConfig->id;

  switch (theWPConfig->id) {
    case WP_DEADSPY:
      measured_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(measured_metric_id, "BYTES_USED", MetricFlags_ValFmt_Int, 1, metric_property_none);
      dead_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(dead_metric_id, "BYTES_DEAD", MetricFlags_ValFmt_Int, 1, metric_property_none);
      break;

    case WP_REDSPY:
    case WP_LOADSPY:
      measured_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(measured_metric_id, "BYTES_NEW", MetricFlags_ValFmt_Int, 1, metric_property_none);
      red_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(red_metric_id, "BYTES_RED", MetricFlags_ValFmt_Int, 1, metric_property_none);
      redApprox_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(redApprox_metric_id, "BYTES_RED_APPROX", MetricFlags_ValFmt_Int, 1, metric_property_none);
      break;

    case WP_TEMPORAL_REUSE:
      temporal_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(temporal_metric_id, "TEMPORAL", MetricFlags_ValFmt_Int, 1, metric_property_none);
      break;

    case WP_SPATIAL_REUSE:
      spatial_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(spatial_metric_id, "SPATIAL", MetricFlags_ValFmt_Int, 1, metric_property_none);
      break;

    case WP_ALL_SHARING:
    case WP_COMDETECTIVE:
    case WP_AMD_COMM:  
    case WP_IPC_ALL_SHARING:
      // must have a canonical load map across processes
      hpcrun_set_ipc_load_map(true);
      //measured_metric_id = hpcrun_new_metric();
      //hpcrun_set_metric_info_and_period(measured_metric_id, "COMMUNICATION", MetricFlags_ValFmt_Int, 1, metric_property_none);
      SetUpFalseSharingMetrics();
      SetUpTrueSharingMetrics();
      break;

    case WP_FALSE_SHARING:
    case WP_IPC_FALSE_SHARING:
      // must have a canonical load map across processes
      hpcrun_set_ipc_load_map(true);
      measured_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(measured_metric_id, "MONITORED", MetricFlags_ValFmt_Int, 1, metric_property_none);
      SetUpFalseSharingMetrics();
      break;

    case WP_REUSE:
      {
#ifdef REUSE_HISTO
        {
          char * bin_scheme_str = getenv("HPCRUN_WP_REUSE_BIN_SCHEME");
          if (bin_scheme_str){
            if ( 0 == strcasecmp(bin_scheme_str, "TRACE")){
              reuse_output_trace = true;
            }
            else { // it should be two numbers connected by ","
              // For example, 4000.0,2.0
              char *dup_str = strdup(bin_scheme_str);
              char *pos = strchr(dup_str, ',');
              if ( pos == NULL){
                EEMSG("Invalid value of the environmental variable HPCRUN_WP_REUSE_BIN_SCHEME");
                free(dup_str);
                monitor_real_abort();
              }
              pos[0] = '\0';
              pos += 1;

              char *endptr;
              reuse_bin_start = strtod(dup_str, &endptr);
              if (reuse_bin_start <= 0.0 || reuse_bin_start == HUGE_VAL || endptr[0] != '\0'){
                EEMSG("Invalid value of the environmental variable HPCRUN_WP_REUSE_BIN_SCHEME");
                free(dup_str);
                monitor_real_abort();
              }
              reuse_bin_ratio = strtod(pos, &endptr);
              if (reuse_bin_ratio <= 1.0 || reuse_bin_ratio == HUGE_VAL || endptr[0] != '\0'){
                EEMSG("Invalid value of the environmental variable HPCRUN_WP_REUSE_BIN_SCHEME");
                free(dup_str);
                monitor_real_abort();
              }
              free(dup_str);
              printf("HPCRUN: start %lf, ratio %lf\n", reuse_bin_start, reuse_bin_ratio);
            }
          } else { //default
            reuse_output_trace = false;
            //reuse_bin_start = 4000;
            reuse_bin_start = 82;
            reuse_bin_ratio = 2;
            //fprintf(stderr, "default configuration is applied\n");
          }
          if (reuse_output_trace == false){

            reuse_bin_size = 20;
            reuse_bin_list = hpcrun_malloc(sizeof(uint64_t)*reuse_bin_size);
            memset(reuse_bin_list, 0, sizeof(uint64_t)*reuse_bin_size);
            reuse_bin_pivot_list = hpcrun_malloc(sizeof(double)*reuse_bin_size);
            reuse_bin_pivot_list[0] = reuse_bin_start;
            for(int i=1; i < reuse_bin_size; i++){
              reuse_bin_pivot_list[i] = reuse_bin_pivot_list[i-1] * reuse_bin_ratio;
            }
          }

        }
#else
        {
          char * monitor_type_str = getenv("HPCRUN_WP_REUSE_PROFILE_TYPE");
          if(monitor_type_str){
            if(0 == strcasecmp(monitor_type_str, "TEMPORAL")) {
              reuse_profile_type = REUSE_TEMPORAL;
            } else if (0 == strcasecmp(monitor_type_str, "SPATIAL")) {
              reuse_profile_type = REUSE_SPATIAL;
            } else if ( 0 == strcasecmp(monitor_type_str, "ALL") ) {
              reuse_profile_type = REUSE_BOTH;
            } else {
              // default;
              reuse_profile_type = REUSE_CACHELINE;
            }
          } else{
            // default
            //fprintf(stderr, "reuse_profile_type is REUSE_BOTH\n");
            reuse_profile_type = REUSE_CACHELINE;
          }
        }

        {
          char * monitor_type_str = getenv("HPCRUN_WP_REUSE_MONITOR_TYPE");
          if(monitor_type_str){
            if(0 == strcasecmp(monitor_type_str, "LOAD")) {
              reuse_monitor_type = LOAD;
            } else if (0 == strcasecmp(monitor_type_str, "STORE")) {
              reuse_monitor_type = STORE;
            } else if (0 == strcasecmp(monitor_type_str, "LS") || 0 == strcasecmp(monitor_type_str, "ALL") ) {
              reuse_monitor_type = LOAD_AND_STORE;
            } else {
              // default;
              reuse_monitor_type = LOAD_AND_STORE;
            }
          } else{
            // defaul
            //fprintf(stderr, "reuse_monitor_type is LOAD_AND_STORE\n");
            reuse_monitor_type = LOAD_AND_STORE;
          }
        }
        {
          char *trap_type_str = getenv("HPCRUN_WP_REUSE_TRAP_TYPE");
          if(trap_type_str){
            if(0 == strcasecmp(trap_type_str, "LOAD")) {
              reuse_trap_type = WP_RW;  // NO WP_READ allowed
            } else if (0 == strcasecmp(trap_type_str, "STORE")) {
              reuse_trap_type = WP_WRITE;
            } else if (0 == strcasecmp(trap_type_str, "LS") || 0 == strcasecmp(trap_type_str, "ALL") ) {
              reuse_trap_type = WP_RW;
            } else {
              // default;
              reuse_trap_type = WP_RW;
            }
          } else{
            // default
            //fprintf(stderr, "reuse_trap_type is WP_RW\n");
            reuse_trap_type = WP_RW;
          }
        }

        {
          char *concatenate_order_str = getenv("HPCRUN_WP_REUSE_CONCATENATE_ORDER");
          if(concatenate_order_str && 0 == strcasecmp(concatenate_order_str, "USE_REUSE")){
            reuse_concatenate_use_reuse = true;
          } else{
            //fprintf(stderr, "reuse_concatenate_use_reuse is false\n");
            reuse_concatenate_use_reuse = false;
          }
        }
#endif
        temporal_reuse_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(temporal_reuse_metric_id, "TEMPORAL", MetricFlags_ValFmt_Int, 1, metric_property_none);
        spatial_reuse_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(spatial_reuse_metric_id, "SPATIAL", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_memory_distance_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_memory_distance_metric_id, "MEMORY_DISTANCE_SUM", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_memory_distance_count_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_memory_distance_count_metric_id, "MEMORY_DISTANCE_COUNT", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_time_distance_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_time_distance_metric_id, "TIME_DISTANCE_SUM", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_time_distance_count_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_time_distance_count_metric_id, "TIME_DISTANCE_COUNT", MetricFlags_ValFmt_Int, 1, metric_property_none);

        // the next two buffers only for internal use
        reuse_buffer_metric_ids[0] = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_buffer_metric_ids[0], "REUSE_BUFFER_1", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_buffer_metric_ids[1] = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_buffer_metric_ids[1],"REUSE_BUFFER_2", MetricFlags_ValFmt_Int, 1, metric_property_none);

      }
      break;
    case WP_AMD_REUSE:
    case WP_AMD_REUSETRACKER:
    case WP_REUSETRACKER:
      {
        //reuse_profile_type = REUSE_TEMPORAL;
#ifdef REUSE_HISTO
        {
          assert(OpenWitchTraceOutput()==0);
          char * bin_scheme_str = getenv("HPCRUN_WP_REUSE_BIN_SCHEME");
          if (bin_scheme_str){
            if ( 0 == strcasecmp(bin_scheme_str, "TRACE")){
              reuse_output_trace = true;
            }
            else { // it should be two numbers connected by ","
              // For example, 4000.0,2.0
              char *dup_str = strdup(bin_scheme_str);
              char *pos = strchr(dup_str, ',');
              if ( pos == NULL){
                EEMSG("Invalid value of the environmental variable HPCRUN_WP_REUSE_BIN_SCHEME");
                free(dup_str);
                monitor_real_abort();
              }
              pos[0] = '\0';
              pos += 1;

              char *endptr;
              reuse_bin_start = strtod(dup_str, &endptr);
              if (reuse_bin_start <= 0.0 || reuse_bin_start == HUGE_VAL || endptr[0] != '\0'){
                EEMSG("Invalid value of the environmental variable HPCRUN_WP_REUSE_BIN_SCHEME");
                free(dup_str);
                monitor_real_abort();
              }
              reuse_bin_ratio = strtod(pos, &endptr);
              if (reuse_bin_ratio <= 1.0 || reuse_bin_ratio == HUGE_VAL || endptr[0] != '\0'){
                EEMSG("Invalid value of the environmental variable HPCRUN_WP_REUSE_BIN_SCHEME");
                free(dup_str);
                monitor_real_abort();
              }
              free(dup_str);
              printf("HPCRUN: start %lf, ratio %lf\n", reuse_bin_start, reuse_bin_ratio);
            }
          } else { //default
            reuse_output_trace = false;
            reuse_bin_start = 67;
            reuse_bin_ratio = 2;
            fprintf(stderr, "default configuration is applied\n");
          }
          if (reuse_output_trace == false){
            //fprintf(stderr, "reuse_output_trace is false\n");
            reuse_bin_size = 20;
            reuse_bin_list = hpcrun_malloc(sizeof(uint64_t)*reuse_bin_size);
            memset(reuse_bin_list, 0, sizeof(uint64_t)*reuse_bin_size);
            reuse_bin_pivot_list = hpcrun_malloc(sizeof(double)*reuse_bin_size);
            reuse_bin_pivot_list[0] = reuse_bin_start;
            //	fprintf(stderr, "reuse_bin_pivot_list[0]: %0.2lf, reuse_bin_start: %0.2lf\n", reuse_bin_pivot_list[0], reuse_bin_start);
            for(int i=1; i < reuse_bin_size; i++){
              reuse_bin_pivot_list[i] = reuse_bin_pivot_list[i-1] * reuse_bin_ratio;
              //fprintf(stderr, "reuse_bin_pivot_list[%d]: %0.2lf\n", i, reuse_bin_pivot_list[i]);
            }

            shared_reuse_bin_size = 20;
            shared_reuse_bin_list = hpcrun_malloc(sizeof(uint64_t)*shared_reuse_bin_size);
            memset(shared_reuse_bin_list, 0, sizeof(uint64_t)*shared_reuse_bin_size);
            shared_reuse_bin_pivot_list = hpcrun_malloc(sizeof(double)*shared_reuse_bin_size);
            shared_reuse_bin_pivot_list[0] = reuse_bin_start;
            //  fprintf(stderr, "reuse_bin_pivot_list[0]: %0.2lf, reuse_bin_start: %0.2lf\n", reuse_bin_pivot_list[0], reuse_bin_start);
            for(int i=1; i < shared_reuse_bin_size; i++){
              shared_reuse_bin_pivot_list[i] = shared_reuse_bin_pivot_list[i-1] * reuse_bin_ratio;
              //fprintf(stderr, "reuse_bin_pivot_list[%d]: %0.2lf\n", i, reuse_bin_pivot_list[i]);
            }

            communication_reuse_bin_size = 20;
            communication_reuse_bin_list = hpcrun_malloc(sizeof(uint64_t)*communication_reuse_bin_size);
            memset(communication_reuse_bin_list, 0, sizeof(uint64_t)*communication_reuse_bin_size);
            communication_reuse_bin_pivot_list = hpcrun_malloc(sizeof(double)*communication_reuse_bin_size);
            communication_reuse_bin_pivot_list[0] = reuse_bin_start;
            //  fprintf(stderr, "reuse_bin_pivot_list[0]: %0.2lf, reuse_bin_start: %0.2lf\n", reuse_bin_pivot_list[0], reuse_bin_start);
            for(int i=1; i < communication_reuse_bin_size; i++){
              communication_reuse_bin_pivot_list[i] = communication_reuse_bin_pivot_list[i-1] * reuse_bin_ratio;
              //fprintf(stderr, "reuse_bin_pivot_list[%d]: %0.2lf\n", i, reuse_bin_pivot_list[i]);
            }

          }

          char * profileL3String = getenv("HPCRUN_PROFILE_L3");
          if(profileL3String){
            if (0 == strcasecmp(profileL3String, "true")) {
              profiling_mode = L3;
            } else if (0 == strcasecmp(profileL3String, "false")) {
              profiling_mode = L1;
              fprintf(stderr, "L1 profiling is activated\n");
            } else {
              // default;
              profiling_mode = MIXED;
            }
          } else {
            // default;
            profiling_mode = MIXED;
          }

          //until here
          //fprintf(stderr, "locality vector is being read\n");
          reading_locality_vector();	
          //fprintf(stderr, "l3_count: %d, l2_count: %d\n", l3_count, l2_count);

        }
        //#else
        {
          char * monitor_type_str = getenv("HPCRUN_WP_REUSE_PROFILE_TYPE");
          if(monitor_type_str){
            if(0 == strcasecmp(monitor_type_str, "TEMPORAL")) {
              reuse_profile_type = REUSE_TEMPORAL;
            } else if (0 == strcasecmp(monitor_type_str, "SPATIAL")) {
              reuse_profile_type = REUSE_SPATIAL;
            } else if ( 0 == strcasecmp(monitor_type_str, "ALL") ) {
              reuse_profile_type = REUSE_BOTH;
            } else {
              // default;
              reuse_profile_type = REUSE_CACHELINE;
            }
          } else{
            // default
            //fprintf(stderr, "reuse_profile_type is REUSE_BOTH\n");
            reuse_profile_type = REUSE_CACHELINE;
          }
        }

        {
          char * monitor_type_str = getenv("HPCRUN_WP_REUSE_MONITOR_TYPE");
          if(monitor_type_str){
            if(0 == strcasecmp(monitor_type_str, "LOAD")) {
              reuse_monitor_type = LOAD;
            } else if (0 == strcasecmp(monitor_type_str, "STORE")) {
              reuse_monitor_type = STORE;
            } else if (0 == strcasecmp(monitor_type_str, "LS") || 0 == strcasecmp(monitor_type_str, "ALL") ) {
              reuse_monitor_type = LOAD_AND_STORE;
            } else {
              // default;
              reuse_monitor_type = LOAD_AND_STORE;
            }
          } else{
            // defaul
            //fprintf(stderr, "reuse_monitor_type is LOAD_AND_STORE\n");
            reuse_monitor_type = LOAD_AND_STORE;
          }
        }
        {
          char *trap_type_str = getenv("HPCRUN_WP_REUSE_TRAP_TYPE");
          if(trap_type_str){
            if(0 == strcasecmp(trap_type_str, "LOAD")) {
              reuse_trap_type = WP_RW;  // NO WP_READ allowed
            } else if (0 == strcasecmp(trap_type_str, "STORE")) {
              reuse_trap_type = WP_WRITE;
            } else if (0 == strcasecmp(trap_type_str, "LS") || 0 == strcasecmp(trap_type_str, "ALL") ) {
              reuse_trap_type = WP_RW;
            } else {
              // default;
              reuse_trap_type = WP_RW;
            }
          } else{
            // default
            //fprintf(stderr, "reuse_trap_type is WP_RW\n");
            reuse_trap_type = WP_RW;
          }
        }

        {
          char *concatenate_order_str = getenv("HPCRUN_WP_REUSE_CONCATENATE_ORDER");
          if(concatenate_order_str && 0 == strcasecmp(concatenate_order_str, "USE_REUSE")){
            reuse_concatenate_use_reuse = true;
          } else{
            //fprintf(stderr, "reuse_concatenate_use_reuse is false\n");
            reuse_concatenate_use_reuse = false;
          }
        }
#endif
        temporal_reuse_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(temporal_reuse_metric_id, "TEMPORAL", MetricFlags_ValFmt_Int, 1, metric_property_none);
        spatial_reuse_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(spatial_reuse_metric_id, "SPATIAL", MetricFlags_ValFmt_Int, 1, metric_property_none);

        l3_temporal_reuse_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(l3_temporal_reuse_metric_id, "L3_TEMPORAL", MetricFlags_ValFmt_Int, 1, metric_property_none);
        l3_spatial_reuse_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(l3_spatial_reuse_metric_id, "L3_SPATIAL", MetricFlags_ValFmt_Int, 1, metric_property_none);

        comm_temporal_reuse_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(comm_temporal_reuse_metric_id, "COMM_TEMPORAL", MetricFlags_ValFmt_Int, 1, metric_property_none);
        comm_spatial_reuse_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(comm_spatial_reuse_metric_id, "COMM_SPATIAL", MetricFlags_ValFmt_Int, 1, metric_property_none);


        reuse_memory_distance_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_memory_distance_metric_id, "MEMORY_DISTANCE_SUM", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_memory_distance_count_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_memory_distance_count_metric_id, "MEMORY_DISTANCE_COUNT", MetricFlags_ValFmt_Int, 1, metric_property_none);
        l3_reuse_memory_distance_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(l3_reuse_memory_distance_metric_id, "L3_MEMORY_DISTANCE_SUM", MetricFlags_ValFmt_Int, 1, metric_property_none);
        l3_reuse_memory_distance_count_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(l3_reuse_memory_distance_count_metric_id, "L3_MEMORY_DISTANCE_COUNT", MetricFlags_ValFmt_Int, 1, metric_property_none); 
        comm_reuse_memory_distance_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(comm_reuse_memory_distance_metric_id, "COMM_MEMORY_DISTANCE_SUM", MetricFlags_ValFmt_Int, 1, metric_property_none);
        comm_reuse_memory_distance_count_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(comm_reuse_memory_distance_count_metric_id, "COMM_MEMORY_DISTANCE_COUNT", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_time_distance_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_time_distance_metric_id, "TIME_DISTANCE_SUM", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_time_distance_count_metric_id = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_time_distance_count_metric_id, "TIME_DISTANCE_COUNT", MetricFlags_ValFmt_Int, 1, metric_property_none);

        // the next two buffers only for internal use
        reuse_buffer_metric_ids[0] = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_buffer_metric_ids[0], "REUSE_BUFFER_1", MetricFlags_ValFmt_Int, 1, metric_property_none);
        reuse_buffer_metric_ids[1] = hpcrun_new_metric();
        hpcrun_set_metric_info_and_period(reuse_buffer_metric_ids[1],"REUSE_BUFFER_2", MetricFlags_ValFmt_Int, 1, metric_property_none);

      }
      break;
    case WP_TRUE_SHARING:
    case WP_IPC_TRUE_SHARING:
      // must have a canonical load map across processes
      hpcrun_set_ipc_load_map(true);
      measured_metric_id = hpcrun_new_metric();
      hpcrun_set_metric_info_and_period(measured_metric_id, "MONITORED", MetricFlags_ValFmt_Int, 1, metric_property_none);
      SetUpTrueSharingMetrics();
      break;

    default:
      break;
  }
}

  static void
METHOD_FN(gen_event_set, int lush_metrics)
{
}

  static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Watchpoint events\n");
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_DEADSPY_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_REDSPY_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_LOADSPY_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_REUSE_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_REUSETRACKER_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_AMD_COMM_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_AMD_REUSE_EVENT_NAME); 
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_AMD_REUSETRACKER_EVENT_NAME); 
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_TEMPORAL_REUSE_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_SPATIAL_REUSE_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_FALSE_SHARING_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_TRUE_SHARING_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_ALL_SHARING_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_COMDETECTIVE_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_IPC_FALSE_SHARING_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_IPC_TRUE_SHARING_EVENT_NAME);
  printf("---------------------------------------------------------------------------\n");
  printf("%s\n", WP_IPC_ALL_SHARING_EVENT_NAME);
  printf("===========================================================================\n");
  printf("\n");
}


/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name witch
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

// **************************************************************************
// * public operations
// **************************************************************************

/******************************************************************************
 * private operations
 *****************************************************************************/

enum JoinNodeType {
  E_KILLED=0,
  E_USED,
  E_NEW_VAL,
  E_TEMPORALLY_REUSED_FROM,
  E_TEMPORALLY_REUSED_BY,
  E_SPATIALLY_REUSED_FROM,
  E_SPATIALLY_REUSED_BY,
  E_L3_TEMPORALLY_REUSED_FROM,
  E_L3_TEMPORALLY_REUSED_BY,
  E_L3_SPATIALLY_REUSED_FROM,
  E_L3_SPATIALLY_REUSED_BY,
  E_COMM_TEMPORALLY_REUSED_FROM,
  E_COMM_TEMPORALLY_REUSED_BY,
  E_COMM_SPATIALLY_REUSED_FROM,
  E_COMM_SPATIALLY_REUSED_BY,
  E_TEPORALLY_REUSED,
  E_SPATIALLY_REUSED,
  E_TRUE_WW_SHARE,
  E_TRUE_WR_SHARE,
  E_TRUE_RW_SHARE,
  E_FALSE_WW_SHARE,
  E_FALSE_WR_SHARE,
  E_FALSE_RW_SHARE,
  E_IPC_TRUE_WW_SHARE,
  E_IPC_TRUE_WR_SHARE,
  E_IPC_TRUE_RW_SHARE,
  E_IPC_FALSE_WW_SHARE,
  E_IPC_FALSE_WR_SHARE,
  E_IPC_FALSE_RW_SHARE,
  E_INVALID_JOIN_NODE_TYPE
};

enum JoinNodeIdx {
  E_ACCURATE_JOIN_NODE_IDX=0,
  E_INACCURATE_JOIN_NODE_IDX=1,
};

static void KILLED_BY(void) {}
static void KILLED_BY_INACCURATE_PC(void) {}

static void USED_BY(void) {}
static void USED_BY_INACCURATE_PC(void) {}

static void NEW_VAL_BY(void) {}
static void NEW_VAL_BY_INACCURATE_PC(void) {}

static void TEPORALLY_REUSED_BY(void) {}
static void TEPORALLY_REUSED_BY_INACCURATE_PC(void) {}

static void SPATIALLY_REUSED_BY(void) {}
static void SPATIALLY_REUSED_BY_INACCURATE_PC(void) {}

static void TRUE_WW_SHARE(void) {}
static void TRUE_WW_SHARE_INACCURATE_PC(void) {}

static void TRUE_WR_SHARE(void) {}
static void TRUE_WR_SHARE_INACCURATE_PC(void) {}

static void TRUE_RW_SHARE(void) {}
static void TRUE_RW_SHARE_INACCURATE_PC(void) {}

static void FALSE_WW_SHARE(void) {}
static void FALSE_WW_SHARE_INACCURATE_PC(void) {}

static void FALSE_WR_SHARE(void) {}
static void FALSE_WR_SHARE_INACCURATE_PC(void) {}

static void FALSE_RW_SHARE(void) {}
static void FALSE_RW_SHARE_INACCURATE_PC(void) {}

static void IPC_TRUE_WW_SHARE(void) {}
static void IPC_TRUE_WW_SHARE_INACCURATE_PC(void) {}

static void IPC_TRUE_WR_SHARE(void) {}
static void IPC_TRUE_WR_SHARE_INACCURATE_PC(void) {}

static void IPC_TRUE_RW_SHARE(void) {}
static void IPC_TRUE_RW_SHARE_INACCURATE_PC(void) {}

static void IPC_FALSE_WW_SHARE(void) {}
static void IPC_FALSE_WW_SHARE_INACCURATE_PC(void) {}

static void IPC_FALSE_WR_SHARE(void) {}
static void IPC_FALSE_WR_SHARE_INACCURATE_PC(void) {}

static void IPC_FALSE_RW_SHARE(void) {}
static void IPC_FALSE_RW_SHARE_INACCURATE_PC(void) {}

// Create a 2D array of Join node functions
#define GET_FUN_ADDR(a) {(&a + 1), (&(a ## _INACCURATE_PC) +1)}

static const void * joinNodes[][2] = {
  [E_KILLED] = GET_FUN_ADDR(KILLED_BY),
  [E_USED] = GET_FUN_ADDR(USED_BY),
  [E_NEW_VAL] = GET_FUN_ADDR(NEW_VAL_BY),
  [E_TEPORALLY_REUSED] = GET_FUN_ADDR(TEPORALLY_REUSED_BY),
  [E_SPATIALLY_REUSED] = GET_FUN_ADDR(SPATIALLY_REUSED_BY),
  [E_TRUE_WW_SHARE] = GET_FUN_ADDR(TRUE_WW_SHARE),
  [E_TRUE_WR_SHARE] = GET_FUN_ADDR(TRUE_WR_SHARE),
  [E_TRUE_RW_SHARE] = GET_FUN_ADDR(TRUE_RW_SHARE),
  [E_FALSE_WW_SHARE] = GET_FUN_ADDR(FALSE_WW_SHARE),
  [E_FALSE_WR_SHARE] = GET_FUN_ADDR(FALSE_WR_SHARE),
  [E_FALSE_RW_SHARE] = GET_FUN_ADDR(FALSE_RW_SHARE),
  [E_IPC_TRUE_WW_SHARE] = GET_FUN_ADDR(IPC_TRUE_WW_SHARE),
  [E_IPC_TRUE_WR_SHARE] = GET_FUN_ADDR(IPC_TRUE_WR_SHARE),
  [E_IPC_TRUE_RW_SHARE] = GET_FUN_ADDR(IPC_TRUE_RW_SHARE),
  [E_IPC_FALSE_WW_SHARE] = GET_FUN_ADDR(IPC_FALSE_WW_SHARE),
  [E_IPC_FALSE_WR_SHARE] = GET_FUN_ADDR(IPC_FALSE_WR_SHARE),
  [E_IPC_FALSE_RW_SHARE] = GET_FUN_ADDR(IPC_FALSE_RW_SHARE)
};

static inline int GetMatchingWatermarkId(int pebsMetricId){
  // Get the correct watermark_metric_id
  for (int i=0; i<NUM_WATERMARK_METRICS; i++) {
    if(pebs_metric_id[i] == pebsMetricId) {
      return watermark_metric_id[i];
    }
  }
  assert(0);
}

static inline uint64_t GetWeightedMetricDiffAndReset(cct_node_t * ctxtNode, int pebsMetricId, double proportion){
  assert(ctxtNode);
  metric_set_t* set = hpcrun_get_metric_set(ctxtNode);
  cct_metric_data_t diffWithPeriod;
  cct_metric_data_t diff;
  int catchUpMetricId = GetMatchingWatermarkId(pebsMetricId);
  hpcrun_get_weighted_metric_diff(pebsMetricId, catchUpMetricId, set, &diff, &diffWithPeriod);
  // catch up metric: up catchUpMetricId to macth pebsMetricId proportionally
 // fprintf(stderr, "diff.r as long: %ld, diffWithPeriod.r as long: %ld, diff.r as double: %0.2lf, diffWithPeriod.r as double: %0.2lf\n", diff.r, diffWithPeriod.r, diff.r, diffWithPeriod.r);
  diff.r = diff.r * proportion;
  cct_metric_data_increment(catchUpMetricId, ctxtNode, diff);
  return (uint64_t) (diffWithPeriod.r * proportion);
}

static void UpdateWatermarkMetric(cct_node_t * ctxtNode, int pebsMetricId, uint64_t metricInc){
  assert(ctxtNode);
  metric_set_t* set = hpcrun_get_metric_set(ctxtNode);
  cct_metric_data_t diffWithPeriod;
  cct_metric_data_t diff;
  int catchUpMetricId = GetMatchingWatermarkId(pebsMetricId);
  //hpcrun_get_weighted_metric_diff(pebsMetricId, catchUpMetricId, set, &diff, &diffWithPeriod);
  // catch up metric: up catchUpMetricId to macth pebsMetricId proportionally
  //fprintf(stderr, "diff.r as long: %ld, diffWithPeriod.r as long: %ld, diff.r as double: %0.2lf, diffWithPeriod.r as double: %0.2lf\n", diff.r, diffWithPeriod.r, diff.r, diffWithPeriod.r);
  //diff.r = diff.r * proportion;
  cct_metric_data_increment(catchUpMetricId, ctxtNode, (cct_metric_data_t){.r = metricInc});
}

static inline uint64_t GetWeightedMetricDiff(cct_node_t * ctxtNode, int pebsMetricId, double proportion){
  assert(ctxtNode);
  metric_set_t* set = hpcrun_get_metric_set(ctxtNode);
  cct_metric_data_t diffWithPeriod;
  cct_metric_data_t diff;
  int catchUpMetricId = GetMatchingWatermarkId(pebsMetricId);
  hpcrun_get_weighted_metric_diff(pebsMetricId, catchUpMetricId, set, &diff, &diffWithPeriod);
  //diff.r = diff.r * proportion;
  //uint64_t nodeSpecificEventCount = diff.r;
  return (uint64_t) (diffWithPeriod.r * proportion);
  //return (uint64_t) (diff.r * proportion);
}


static void UpdateFoundMetrics(cct_node_t * ctxtNode, cct_node_t * oldNode, void * joinNode, int foundMetric, int foundMetricInc){
  // insert a special node
  cct_node_t *node = hpcrun_insert_special_node(oldNode, joinNode);
  // concatenate call paths
  node = hpcrun_cct_insert_path_return_leaf(ctxtNode, node);
  // update the foundMetric
  cct_metric_data_increment(foundMetric, node, (cct_metric_data_t){.i = foundMetricInc});
}


#define SAMPLE_NO_INC ((hpcrun_metricVal_t){.i=0})
#define SAMPLE_UNIT_INC ((hpcrun_metricVal_t){.i=1})

static cct_node_t * UpdateMetrics(void *ctxt, cct_node_t * oldNode, void * joinNode, int checkedMetric, int foundMetric, int checkedMetricInc, int foundMetricInc){
  // unwind call stack once
  sample_val_t v = hpcrun_sample_callpath(ctxt, checkedMetric, (hpcrun_metricVal_t){.i=checkedMetricInc}, 0/*skipInner*/, 1/*isSync*/, NULL);
  if(foundMetricInc) {
    UpdateFoundMetrics(v.sample_node, oldNode, joinNode, foundMetric, foundMetricInc);
  }
  return v.sample_node;
}

static inline void UpdateConcatenatedPathPair(void *ctxt, cct_node_t * oldNode, const void * joinNode, int metricId, uint64_t metricInc){
  // unwind call stack once
  sample_val_t v = hpcrun_sample_callpath(ctxt, metricId, SAMPLE_NO_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
  // insert a special node
  cct_node_t *node = hpcrun_insert_special_node(oldNode, joinNode);
  // concatenate call paths
  node = hpcrun_cct_insert_path_return_leaf(v.sample_node, node);
  // update the foundMetric
  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = metricInc});
}


static inline cct_node_t *getConcatenatedNode(cct_node_t *bottomNode, cct_node_t * topNode, const void * joinNode){
  // insert a special node
  cct_node_t *node = hpcrun_insert_special_node(topNode, joinNode);
  // concatenate call paths
  node = hpcrun_cct_insert_path_return_leaf(bottomNode, node);
  return node;
}

double thread_coefficient(int as_matrix_size) {
	double thread_count = (double) as_matrix_size + 1;
	return 50.6 * pow(thread_count, -0.434);
}

static WPTriggerActionType DeadStoreWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  if(!wt->pc) {
    // if the ip is 0, let's drop the WP
    return ALREADY_DISABLED;
  }

  // This is a approximation.
  // If we took N samples at wpi->sample.node since the last time a WP triggered here,
  // If this a dead write, we'll update the dead_writes metric at the call path <wpi->sample.node:KILLED_BY:curctxt>
  // Otherwise (not dead), we'll update the used_writes metric at the call path <wpi->sample.node:USED_BY:curctxt>
  // In either case, the increment will be (N * overlapBytes)
  // Bump up watermark_metric_id to match sampledMetricId

  double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
  uint64_t numDiffSamples = GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
  int overlapBytes = GET_OVERLAP_BYTES(wpi->sample.va, wpi->sample.wpLength, wt->va, wt->accessLength);
  if(overlapBytes <= 0){
    //fprintf(stderr, "\n wpi->sample.va=%p, wpi->sample.wpLength = %d,  wt->va = %p, wt->accessLength=%d\n", wpi->sample.va, wpi->sample.wpLength, wt->va, wt->accessLength);
    monitor_real_abort();
  }

  // Now increment dead_metric_id by numDiffSamples * wpi->sample.accessLength
  // I could have done numDiffSamples * overlapBytes, but it will cause misattribution when access sizes are not same at dead and kill sites.
  // Basically, we are assuming that whatever happened in the observed watchpoints is applicable to the entire access length
  uint64_t inc = numDiffSamples * wpi->sample.accessLength;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  // if the access is a LOAD/LOAD_AND_STORE we are done! not a dead write :)
  if(wt->accessType == LOAD || wt->accessType == LOAD_AND_STORE) {
    // update the measured (i.e. not dead)
    usedBytes += inc;
    UpdateConcatenatedPathPair(wt->ctxt, wpi->sample.node /* oldNode*/, joinNodes[E_USED][joinNodeIdx] /* joinNode*/, measured_metric_id /* checkedMetric */, inc);
  } else {
    deadBytes += inc;
    UpdateConcatenatedPathPair(wt->ctxt, wpi->sample.node /* oldNode*/, joinNodes[E_KILLED][joinNodeIdx] /* joinNode*/, dead_metric_id /* checkedMetric */, inc);
  }
  return ALREADY_DISABLED;
}

static inline bool IsAddressReadable(void * addr){
  bool retVal = true;
  thread_data_t * td =  hpcrun_get_thread_data();
  hpcrun_set_handling_sample(td);
  sigjmp_buf_t* it = &(td->bad_unwind);
  int ljmp = sigsetjmp(it->jb, 1);
  if (ljmp == 0){
    volatile char i = *(char*)(addr);
  } else {
    // longjmp here
    retVal = false;
  }
  hpcrun_clear_handling_sample(td);
  return retVal;
}

static WPTriggerActionType RedStoreWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  void *pip = wt->pc;
  if(!pip) {
    // if the ip is 0, let's drop the WP
    return ALREADY_DISABLED;
  }

  bool isFloatOperation = wt->floatType == ELEM_TYPE_UNKNOWN? false: true;
  bool redBytes = 0;

  // check integer instructions
  int overlapLen = GET_OVERLAP_BYTES(wt->va, safeAccessLen, wpi->sample.va, wpi->sample.wpLength);
  if(overlapLen <= 0){
    //fprintf(stderr, "\n wpi->sample.va=%p, wpi->sample.wpLength = %d,  wt->va = %p, wt->accessLength=%d\n", wpi->sample.va, wpi->sample.wpLength, wt->va, wt->accessLength);
    monitor_real_abort();
  }

  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;
  int firstOffest = FIRST_OVERLAPPED_BYTE_OFFSET_IN_FIRST(wt->va, safeAccessLen, wpi->sample.va, wpi->sample.wpLength);
  int secondOffest = FIRST_OVERLAPPED_BYTE_OFFSET_IN_FIRST(wt->va, safeAccessLen, wpi->sample.va, wpi->sample.wpLength);

  void * wpiStartByte = wpi->sample.va + secondOffest;
  void * wtStartByte = wt->va + firstOffest;
  // if the overlapLen is not 4 or 8, we cannot do any FP, DP approximation.
  //wpiStartByte and wtStartByte are not 4 or 8 byte aligned, we cannot do any FP, DP approximation.

  // If we got an insane address that cannot be read, return silently
  if(!IsAddressReadable(wtStartByte)){
    return ALREADY_DISABLED;
  }

  if(isFloatOperation){
    switch (wt->floatType) {
      case ELEM_TYPE_SINGLE:{
                              if(overlapLen < sizeof(float)){
                                goto TreatLikeInteger;
                              }
                              if(!IS_4_BYTE_ALIGNED(wpiStartByte)){
                                goto TreatLikeInteger;
                              }
                              if(!IS_4_BYTE_ALIGNED(wtStartByte)){
                                goto TreatLikeInteger;
                              }
                              // Sanity passed, now we can compare approximate equality.
                              // the value of old is present in wpi->value[secondOffest]
                              //float old = *((float*)(wpiStartByte));
                              float old = *((float*)(wpi->value + secondOffest));
                              float new = *((float*)(wtStartByte));
                              if(old != new){
                                float rate = (old-new)/old;/////shasha:in case old may be 0
                                if(rate > APPROX_RATE || rate < -APPROX_RATE)
                                  redBytes = 0;
                                else
                                  // Amplify the observation by the access length
                                  redBytes = sizeof(float);
                              } else {
                                redBytes = sizeof(float);
                              }
                            }
                            break;

      case ELEM_TYPE_DOUBLE:{
                              if(overlapLen < sizeof(double)){
                                goto TreatLikeInteger;
                              }
                              if(!IS_8_BYTE_ALIGNED(wpiStartByte)){
                                goto TreatLikeInteger;
                              }
                              if(!IS_8_BYTE_ALIGNED(wtStartByte)){
                                goto TreatLikeInteger;
                              }
                              // Sanity passed, now we can compare approximate equality.
                              //double old = *((double*)(wpiStartByte));
                              double old = *((double*)(wpi->value + secondOffest));
                              double new = *((double*)(wtStartByte));
                              if(old != new){
                                double rate = (old-new)/old;/////shasha:in case old may be 0
                                if(rate > APPROX_RATE || rate < -APPROX_RATE)
                                  redBytes = 0;
                                else
                                  redBytes = sizeof(double);
                              } else {
                                redBytes = sizeof(double);
                              }
                            }
                            break;

      default: // unhandled!!
                            goto TreatLikeInteger;
                            break;
    }
    double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
    uint64_t numDiffSamples = GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
    if(redBytes != 0) {
      // Now increment metric by numDiffSamples * wpi->sample.accessLength
      // This is an approximation of what might have happened.
      // If I observe that 4 bytes are redundant out of 128 accessed bytes, I amplify it to 128 bytes.
      uint64_t inc = numDiffSamples * wpi->sample.accessLength;
      oldAppxBytes += inc;
      UpdateConcatenatedPathPair(wt->ctxt, wpi->sample.node /* oldNode*/, joinNodes[E_KILLED][joinNodeIdx] /* joinNode*/, redApprox_metric_id /* checkedMetric */, inc);
    } else {
      // Now increment metric by numDiffSamples * wpi->sample.accessLength
      // This is an approximation of what might have happened.
      // If I observe that 4 bytes are NOT redundant out of 128 accessed bytes, I amplify it to 128 bytes.
      uint64_t inc = numDiffSamples * wpi->sample.accessLength;
      newBytes += inc;
      UpdateConcatenatedPathPair(wt->ctxt, wpi->sample.node /* oldNode*/, joinNodes[E_NEW_VAL][joinNodeIdx] /* joinNode*/, measured_metric_id /* checkedMetric */, inc);
    }
  }else /* non float */{

TreatLikeInteger:
    ;

    for(int i = firstOffest, k = secondOffest ; i < firstOffest + overlapLen; i++, k++){
      if(((uint8_t*)(wt->va))[i] == wpi->value[k]) {
        redBytes ++;
      } else{
        redBytes = 0;
        break;
      }
    }
    double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
    uint64_t numDiffSamples = GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);

    if(redBytes != 0) {
      // Now increment metric: if the entire overlap is redundant, amplify to numDiffSamples * wpi->sample.accessLength
      // This is an approximation of what might have happened.
      // If I observe that 4 bytes are redundant out of 128 accessed bytes, I amplify it to 128 bytes.
      uint64_t inc = numDiffSamples * wpi->sample.accessLength;
      oldBytes += inc;
      UpdateConcatenatedPathPair(wt->ctxt, wpi->sample.node /* oldNode*/, joinNodes[E_KILLED][joinNodeIdx] /* joinNode*/, red_metric_id /* checkedMetric */, inc);
    } else {
      // Now increment metric: if the entire overlap is redundant, amplify to numDiffSamples * wpi->sample.accessLength
      // This is an approximation of what might have happened.
      // If I observe that 4 bytes are NOT redundant out of 128 accessed bytes, I amplify it to 128 bytes.
      uint64_t inc = numDiffSamples * wpi->sample.accessLength;
      newBytes += inc;
      UpdateConcatenatedPathPair(wt->ctxt, wpi->sample.node /* oldNode*/,  joinNodes[E_NEW_VAL][joinNodeIdx] /* joinNode*/, measured_metric_id /* checkedMetric */, inc);
    }
  }
  return ALREADY_DISABLED;
}

static WPTriggerActionType TemporalReuseWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  if(!wt->pc) {
    // if the ip is 0, let's retain the WP
    return RETAIN_WP;
  }
  // Report a reuse
  double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
  uint64_t numDiffSamples = GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  // Now increment temporal_metric_id by numDiffSamples * overlapBytes
  uint64_t inc = numDiffSamples;
  reuse += inc;
  //fprintf(stderr, "in TemporalReuseWPCallback, reuse distance: %ld\n", inc);
  UpdateConcatenatedPathPair(wt->ctxt, wpi->sample.node /* oldNode*/, joinNodes[E_TEPORALLY_REUSED][joinNodeIdx] /* joinNode*/, temporal_metric_id /* checkedMetric */, inc);
  return ALREADY_DISABLED;
}

static WPTriggerActionType SpatialReuseWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  if(!wt->pc) {
    // if the ip is 0, drop the WP
    return ALREADY_DISABLED;
  }
  // Report a reuse
  double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
  uint64_t numDiffSamples = GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;
  // Now increment dead_metric_id by numDiffSamples * overlapBytes
  uint64_t inc = numDiffSamples;
  reuse += inc;

  UpdateConcatenatedPathPair(wt->ctxt, wpi->sample.node /* oldNode*/, joinNodes[E_SPATIALLY_REUSED][joinNodeIdx] /* joinNode*/, spatial_metric_id /* checkedMetric */, inc);
  return ALREADY_DISABLED;
}

static WPTriggerActionType LoadLoadWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  void *pip = wt->pc;
  if(!pip) {
    // if the ip is 0, let's drop the WP
    return ALREADY_DISABLED;
  }
  // If  this is a STORE ignore the WP and retain it.
  if(wt->accessType == STORE){
    return RETAIN_WP;
  }
  return RedStoreWPCallback(wpi, startOffset, safeAccessLen, wt);
}

static WPTriggerActionType FalseSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  int metricId = -1;
  const void* joinNode;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  //fprintf(stderr, "wt->va: %lx, wt->accessType: %d\n", wt->va, wt->accessType);
  if(wt->accessType == LOAD){
    falseWRIns ++;
    metricId = false_wr_metric_id;
    joinNode = joinNodes[E_FALSE_WR_SHARE][joinNodeIdx];
  } else {
    if(wpi->sample.accessType == LOAD) {
      falseRWIns ++;
      metricId = false_rw_metric_id;
      joinNode = joinNodes[E_FALSE_RW_SHARE][joinNodeIdx];
    } else{
      falseWWIns ++;
      metricId =  false_ww_metric_id;
      joinNode = joinNodes[E_FALSE_WW_SHARE][joinNodeIdx];
    }
  }

  sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
  // insert a special node
  cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
  node = hpcrun_cct_insert_path_return_leaf(wpi->sample.node, node);
  // update the metricId
  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
  return ALREADY_DISABLED;
}

static WPTriggerActionType ReuseWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  //fprintf(stderr, "in ReuseWPCallback\n");
#if 0  // jqswang:TODO, how to handle it?
  if(!wt->pc) {
    // if the ip is 0, let's drop the WP
    //return RETAIN_WP;
    return ALREADY_DISABLED;
  }
#endif //jqswang

  uint64_t val[2][3];
  for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
    assert(linux_perf_read_event_counter( reuse_distance_events[i], val[i]) >= 0);
    //fprintf(stderr, "USE: %lu %lu %lu,  REUSE: %lu %lu %lu\n", wpi->sample.reuseDistance[i][0], wpi->sample.reuseDistance[i][1], wpi->sample.reuseDistance[i][2], val[i][0], val[i][1], val[i][2]);
    //fprintf(stderr, "DIFF: %lu\n", val[i][0] - wpi->sample.reuseDistance[i][0]);
    for(int j=0; j < 3; j++){
      if (val[i][j] >= wpi->sample.reuseDistance[i][j]){
        //fprintf(stderr, "before subtraction: val[%d][%d]: %ld, wpi->sample.reuseDistance[%d][%d]: %ld\n", i, j, val[i][j], i, j, wpi->sample.reuseDistance[i][j]);
        val[i][j] -= wpi->sample.reuseDistance[i][j];
        //fprintf(stderr, "after subtraction: val[%d][%d]: %ld, wpi->sample.reuseDistance[%d][%d]: %ld\n", i, j, val[i][j], i, j, wpi->sample.reuseDistance[i][j]);
      }
      else { //Something wrong happens here and the record is not reliable. Drop it!
        return ALREADY_DISABLED;
      }
    }
  }
  // Report a reuse
  // returns 1.0 now but previously returns 1/sharer s.t. sharer is #wp sharing the same context as the trapped wp 
  double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
  //fprintf(stderr, "myProportion: %0.2lf\n", myProportion);
  uint64_t numDiffSamples = 100000; //GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
  uint64_t inc = numDiffSamples;
  //fprintf(stderr, "inc: %ld\n", inc);
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  uint64_t time_distance = rdtsc() - wpi->startTime;

#ifdef REUSE_HISTO
  //fprintf(stderr, "inside REUSE_HISTO\n");
  //cct_node_t *reuseNode = getPreciseNode(wt->ctxt, wt->pc, temporal_reuse_metric_id );
  sample_val_t v = hpcrun_sample_callpath(wt->ctxt, temporal_reuse_metric_id, SAMPLE_NO_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
  cct_node_t *reuseNode = v.sample_node;

  if (reuse_output_trace){
    WriteWitchTraceOutput("REUSE_DISTANCE: %d %d %lu,", hpcrun_cct_persistent_id(wpi->sample.node), hpcrun_cct_persistent_id(reuseNode), inc);
    for(int i=0; i < MIN(2, reuse_distance_num_events); i++){
      WriteWitchTraceOutput(" %lu %lu %lu,", val[i][0], val[i][1], val[i][2]);
    }
    WriteWitchTraceOutput("\n");
  } else{
    uint64_t rd = 0;
    for(int i=0; i < MIN(2, reuse_distance_num_events); i++){
      assert(val[i][1] == 0 && val[i][2] == 0); // no counter multiplexing allowed
      rd += val[i][0];
    }
    ReuseAddDistance(rd, inc);
  }

#else

  cct_node_t *reusePairNode;
  if (wpi->sample.reuseType == REUSE_TEMPORAL){
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, temporal_reuse_metric_id, SAMPLE_NO_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
    cct_node_t *reuseNode = v.sample_node;
    //fprintf(stderr, "reuse of REUSE_TEMPORAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode /*bottomNode*/, wpi->sample.node /*topNode*/, joinNodes[E_TEMPORALLY_REUSED_BY][joinNodeIdx] /* joinNode*/);
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node /*bottomNode*/, reuseNode /*topNode*/, joinNodes[E_TEMPORALLY_REUSED_FROM][joinNodeIdx] /* joinNode*/);
    }
  }
  else { // REUSE_SPATIAL
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, spatial_reuse_metric_id, SAMPLE_NO_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
    cct_node_t *reuseNode = v.sample_node;
    //fprintf(stderr, "reuse of REUSE_SPATIAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode /*bottomNode*/, wpi->sample.node /*topNode*/, joinNodes[E_SPATIALLY_REUSED_BY][joinNodeIdx] /* joinNode*/);
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node /*bottomNode*/, reuseNode /*topNode*/, joinNodes[E_SPATIALLY_REUSED_FROM][joinNodeIdx] /* joinNode*/);
    }
  }
  cct_metric_data_increment(reuse_memory_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = (val[0][0] + val[1][0]) });
  //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
  cct_metric_data_increment(reuse_memory_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});

  reuseTemporal += inc;
  if (wpi->sample.reuseType == REUSE_TEMPORAL){
    cct_metric_data_increment(temporal_reuse_metric_id, reusePairNode, (cct_metric_data_t){.i = inc});
    //fprintf(stderr, "reuse distance temporal: %ld\n", inc);
  } else {
    cct_metric_data_increment(spatial_reuse_metric_id, reusePairNode, (cct_metric_data_t){.i = inc});
    //fprintf(stderr, "reuse distance spatial: %ld\n", inc);
  }
  cct_metric_data_increment(reuse_time_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = time_distance});
  cct_metric_data_increment(reuse_time_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});
#endif
  return ALREADY_DISABLED;
}

typedef struct FalseSharingLocs{
  size_t va;
  int wpLen;
}FalseSharingLocs;

// getting all false sharing memory regions
static inline void GetAllFalseSharingLocations(size_t va, int accessLen, size_t baseAddr, int maxFSLength, int * wpSizes, int curWPSizeIdx, int totalWPSizes, FalseSharingLocs * fsl, int * numFSLocs){
  int curWPSize = wpSizes[curWPSizeIdx];
  for(int i = 0; i < maxFSLength/curWPSize; i ++) {
    size_t curAddr = baseAddr + i * curWPSize;
    int overlapLen = GET_OVERLAP_BYTES(curAddr, curWPSize, va, accessLen);
    if(0 >= overlapLen) {
      fsl[*numFSLocs].va = curAddr;
      fsl[*numFSLocs].wpLen = curWPSize;
      (*numFSLocs)++;
    }else if (curWPSize != overlapLen) {
      if(curWPSizeIdx+1 < totalWPSizes) {
        GetAllFalseSharingLocations(va, accessLen, curAddr, curWPSize, wpSizes, curWPSizeIdx+1, totalWPSizes, fsl, numFSLocs);
      }
    } else {
      // Nop
    }
  }
}

static WPTriggerActionType ReuseTrackerWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  //fprintf(stderr, "in ReuseTrackerWPCallback\n");
  trap_count++;
  int reuse_type = REUSE_NONE;
  bool l3_inc_attribute = false;
  bool time_distance_attribute = false;

  uint64_t numDiffSamples = 0;
  bool comm_reuse = false;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  bool source_code_line_attribution = false;
  uint64_t attributed_rd = 0;
  uint64_t attributed_inc = 0;
  uint64_t l3_attributed_rd = 0;
  uint64_t l3_attributed_inc = 0;
  uint64_t time_distance;
  int me = TD_GET(core_profile_trace_data.id);
  int monitored_tid = globalReuseWPs.table[wt->location].monitored_tid; 

  if(me == monitored_tid) {
    double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
    numDiffSamples = GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
    double inc_scale = dynamic_global_thread_count / (double) max_used_wp_count;
    uint64_t inc = numDiffSamples * inc_scale;
    uint64_t trapTime = rdtsc();
    // before
    bool handle_trap = false;
    bool post_inc_flag = false;
    uint64_t theCounter = globalReuseWPs.table[wt->location].counter;
    if((theCounter & 1) == 0) {
      if(__sync_bool_compare_and_swap(&globalReuseWPs.table[wt->location].counter, theCounter, theCounter+1)) {
        if(globalReuseWPs.table[wt->location].active) {

          //globalReuseWPs.table[wt->location].trap_just_happened = true;
          if(profiling_mode == L1 || profiling_mode == MIXED)
            numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
          //numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
          globalReuseWPs.table[wt->location].active = false;
          handle_trap = true;	
        }
        if(globalReuseWPs.table[wt->location].sharedActive) {
          post_inc_flag = true;	
        }
        globalReuseWPs.table[wt->location].counter++;
      }
    }
    // after

    if(handle_trap) {
      uint64_t rd = 0;
      uint64_t val[2][3];

      for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
        assert(linux_perf_read_event_counter_l1( reuse_distance_events[i], val[i], false) >= 0); 
        val[i][1] = 0; 
        wpi->sample.reuseDistance[i][1] = 0;

        for(int j=0; j < 3; j++){
          if (val[i][j] >= wpi->sample.reuseDistance[i][j]){
            val[i][j] -= wpi->sample.reuseDistance[i][j];
          }
          else { //Something wrong happens here and the record is not reliable. Drop it! 
            return ALREADY_DISABLED;
          }
        }
      }

      for(int i=0; i < MIN(2, reuse_distance_num_events); i++){
        //fprintf(stderr, "before assert %d\n", i);
        assert(val[i][1] == 0 && val[i][2] == 0); // no counter multiplexing allowed
        //fprintf(stderr, "after assert %d\n", i);
        rd += val[i][0];
      }
      // Report a reuse
      // returns 1.0 now but previously returns 1/sharer s.t. sharer is #wp sharing the same context as the trapped wp

      //fprintf(stderr, "before time_distance\n");
      time_distance = rdtsc() - wpi->startTime;
      //fprintf(stderr, "private reuse is detected me: %d, monitored_tid: %d\n", me, monitored_tid);
      ReuseAddDistance(rd, inc);
      reuse_type = wpi->sample.reuseType;
      attributed_rd = rd;
      attributed_inc = inc;	
      source_code_line_attribution = true;
    }
    if(post_inc_flag) {
      globalReuseWPs.table[wt->location].inc = inc;
      //fprintf(stderr, "inc is posted, sharedReuse: %d, published_rd: %d\n", globalReuseWPs.table[wt->location].sharedActive, globalReuseWPs.table[wt->location].rd);
    } else if(globalReuseWPs.table[wt->location].rd > 0) {
      SharedReuseAddDistance(globalReuseWPs.table[wt->location].rd, inc);
      if(globalReuseWPs.table[wt->location].is_rar == false) {
        CommunicationReuseAddDistance(globalReuseWPs.table[wt->location].rd, inc);
        comm_reuse = true;
      }
      if(profiling_mode == L3)
        numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
      l3_attributed_inc = inc;
      l3_attributed_rd = globalReuseWPs.table[wt->location].rd;
      l3_inc_attribute = true;	
    }
    globalReuseWPs.table[wt->location].self_trap = false;
    theCounter = MonitoredNode.counter;
    if((theCounter & 1) == 0) {
      if(__sync_bool_compare_and_swap(&MonitoredNode.counter, theCounter, theCounter+1)) {
        if(globalReuseWPs.table[wt->location].time == MonitoredNode.timestamp) {
          MonitoredNode.self_trap = false;
          MonitoredNode.trap_timestamp = trapTime;
        }
        MonitoredNode.counter++;
      }
    }
  } else {
    if(/*(wt->accessType == STORE) || (wt->accessType == LOAD_AND_STORE)*/ wpi->sample.type == WP_WRITE) {
      uint64_t theCounter = globalReuseWPs.table[wt->location].counter;
      if((theCounter & 1) == 0) {
        if(__sync_bool_compare_and_swap(&globalReuseWPs.table[wt->location].counter, theCounter, theCounter+1)) {
          if(globalReuseWPs.table[wt->location].active) {

            //globalReuseWPs.table[wt->location].trap_just_happened = true;
            if(profiling_mode == L1 || profiling_mode == MIXED)
              numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
            //numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
            //fprintf(stderr, "L1 invalidation is detected in the same L3\n");
            globalReuseWPs.table[wt->location].active = false;
          }
          globalReuseWPs.table[wt->location].counter++;
        }
      }
    }

    if(profiling_mode == L3 || profiling_mode == MIXED) {
      int affinity_l3 = 0;
      int my_core = sched_getcpu();
      if(l3_count > 1)
        affinity_l3 = thread_to_l3_mapping[my_core];
      bool post_rd_flag = false;
      bool handle_trap = false;
      uint64_t theCounter = globalReuseWPs.table[wt->location].counter;
      if((theCounter & 1) == 0) {
        if(__sync_bool_compare_and_swap(&globalReuseWPs.table[wt->location].counter, theCounter, theCounter+1)) {
          if(globalReuseWPs.table[wt->location].sharedActive) {

            //globalReuseWPs.table[wt->location].trap_just_happened = true;
            //numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
            if((l3_count == 1) || (wpi->sample.L3Id == affinity_l3)) {
              globalReuseWPs.table[wt->location].sharedActive = false;
              handle_trap = true;
              if(globalReuseWPs.table[wt->location].inc == 0) {
                post_rd_flag = true;
              }
              /*if(wpi->sample.type == WP_WRITE)
                fprintf(stderr, "L1 invalidation is detected in the same L3\n");*/
            } else if(/*(wt->accessType == STORE) || (wt->accessType == LOAD_AND_STORE)*/wpi->sample.type == WP_WRITE) {
              //fprintf(stderr, "invalidation is detected, me: %d, monitored_tid: %d\n", me, monitored_tid);
              globalReuseWPs.table[wt->location].sharedActive = false;
              //fprintf(stderr, "L3 invalidation is detected\n");
            }

          }
          globalReuseWPs.table[wt->location].counter++;
        }
      }

      if(handle_trap) {
        double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
        numDiffSamples = hpcrun_id2metric(wpi->sample.sampledMetricId)->period; //GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
        double inc_scale = dynamic_global_thread_count / (double) max_used_wp_count;
        uint64_t inc = numDiffSamples * inc_scale;
        uint64_t rd = 0;
        uint64_t val[2][3];
        //uint64_t global_val[3];
        for(int i=0; i < 2; i++) {
          for(int j=0; j < 3; j++) {
            val[i][j] = 0;
            //global_val[i] = 0;
          }
        }

        //fprintf(stderr, "a trap happens in the same L3\n");
        // fprintf(stderr, "a trap due to load use happens in thread %d mapped to core %d located in L3 %d armed by thread %d in L3 %d\n", me, my_core, affinity_l3, globalReuseWPs.table[wt->location].tid, wpi->sample.L3Id);
        // before
        int cur_global_thread_count = global_thread_count;
        for(int i = 0; i < cur_global_thread_count; i++) {
          if((mapping_size == 0) || (l3_count == 1) || (thread_to_l3_mapping[mapping_vector[i % mapping_size]] == affinity_l3)) {
            for (int j=0; j < MIN(2, reuse_distance_num_events); j++){
              uint64_t val1[3];
              linux_perf_read_event_counter_shared( reuse_distance_events[j], val1, i/*locality_vector[affinity_l3][i+1]*/);
              for(int k=0; k < 3; k++) {
                val[j][k] += val1[k];
              }
            }
          }
        }
        // after	

        // before
        for(int i=0; i < 2; i++) {
          if(val[i][0] >= wpi->sample.sharedReuseDistance[i][0]) {

            val[i][0] -= wpi->sample.sharedReuseDistance[i][0];
          }
          rd += val[i][0];
        }

        if (wpi->sample.reuseType == REUSE_TEMPORAL)
          reuse_type = L3_REUSE_TEMPORAL;
        else if (wpi->sample.reuseType == REUSE_SPATIAL)
          reuse_type = L3_REUSE_SPATIAL;	

        if(post_rd_flag) {
          globalReuseWPs.table[wt->location].rd = rd;
          if(wpi->sample.accessType == LOAD /*&& wt->accessType == LOAD*/)	
            globalReuseWPs.table[wt->location].is_rar = true;
        } else if(globalReuseWPs.table[wt->location].inc > 0) {
          SharedReuseAddDistance(rd, globalReuseWPs.table[wt->location].inc);
          if(wpi->sample.accessType != LOAD /*|| wt->accessType != LOAD*/) {
            CommunicationReuseAddDistance(rd, globalReuseWPs.table[wt->location].inc);
            comm_reuse = true;
          }
          if(profiling_mode == L3)
            numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
          //shared_reuse_counter++;
          l3_attributed_inc = globalReuseWPs.table[wt->location].inc;
          l3_attributed_rd = rd;
          l3_inc_attribute = true;	
        }
        //attributed_inc = inc;
        source_code_line_attribution = true;
        attributed_rd = rd; //rd_with_store;
        time_distance = rdtsc() - wpi->startTime;
        // after
      }
      // after
    }

  }

  cct_node_t *reusePairNode;
  cct_node_t *commReusePairNode;
  if (reuse_type == REUSE_TEMPORAL){
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, temporal_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *reuseNode = v.sample_node;
    //fprintf(stderr, "reuse of REUSE_TEMPORAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode, wpi->sample.node, joinNodes[E_TEMPORALLY_REUSED_BY][joinNodeIdx]);
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node, reuseNode, joinNodes[E_TEMPORALLY_REUSED_FROM][joinNodeIdx]);
    }
  }
  else if (reuse_type == REUSE_SPATIAL) { // REUSE_SPATIAL
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, spatial_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *reuseNode = v.sample_node;
    //fprintf(stderr, "reuse of REUSE_SPATIAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode, wpi->sample.node, joinNodes[E_SPATIALLY_REUSED_BY][joinNodeIdx]);
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node, reuseNode, joinNodes[E_SPATIALLY_REUSED_FROM][joinNodeIdx]);
    }
  } else if (reuse_type == L3_REUSE_TEMPORAL){
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, l3_temporal_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *reuseNode = v.sample_node;
    sample_val_t v1 = hpcrun_sample_callpath(wt->ctxt, comm_temporal_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *commReuseNode = v1.sample_node;
    //fprintf(stderr, "reuse of REUSE_TEMPORAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode, wpi->sample.node, joinNodes[E_L3_TEMPORALLY_REUSED_BY][joinNodeIdx]);
      if (comm_reuse) {
        commReusePairNode = getConcatenatedNode(commReuseNode, wpi->sample.node, joinNodes[E_COMM_TEMPORALLY_REUSED_BY][joinNodeIdx]);
      }
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node, reuseNode, joinNodes[E_L3_TEMPORALLY_REUSED_FROM][joinNodeIdx]);
      if (comm_reuse) {
        commReusePairNode = getConcatenatedNode(wpi->sample.node, commReuseNode, joinNodes[E_COMM_TEMPORALLY_REUSED_FROM][joinNodeIdx]); 
      }
    }
    globalReuseWPs.table[wt->location].reusePairNode = reusePairNode;
    globalReuseWPs.table[wt->location].commReusePairNode = commReusePairNode;
  }
  else if (reuse_type == L3_REUSE_SPATIAL) { // REUSE_SPATIAL
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, l3_spatial_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *reuseNode = v.sample_node;
    sample_val_t v1 = hpcrun_sample_callpath(wt->ctxt, comm_spatial_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *commReuseNode = v1.sample_node;
    //fprintf(stderr, "reuse of REUSE_SPATIAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode, wpi->sample.node, joinNodes[E_L3_SPATIALLY_REUSED_BY][joinNodeIdx]);
      if(comm_reuse) {
        commReusePairNode = getConcatenatedNode(commReuseNode, wpi->sample.node, joinNodes[E_COMM_SPATIALLY_REUSED_BY][joinNodeIdx]);
      }
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node, reuseNode, joinNodes[E_L3_SPATIALLY_REUSED_FROM][joinNodeIdx]);
      if(comm_reuse) {
        commReusePairNode = getConcatenatedNode(wpi->sample.node, commReuseNode, joinNodes[E_COMM_SPATIALLY_REUSED_FROM][joinNodeIdx]);
      }
    }
    globalReuseWPs.table[wt->location].reusePairNode = reusePairNode;
    globalReuseWPs.table[wt->location].commReusePairNode = commReusePairNode;
  }
  if ((reuse_type == REUSE_TEMPORAL) || (reuse_type == REUSE_SPATIAL)) {
    cct_metric_data_increment(reuse_memory_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = (attributed_rd) });
    //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
    cct_metric_data_increment(reuse_memory_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});
  } else if ((reuse_type == L3_REUSE_TEMPORAL) || (reuse_type == L3_REUSE_SPATIAL)) {
    cct_metric_data_increment(l3_reuse_memory_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = (attributed_rd) });
    //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
    cct_metric_data_increment(l3_reuse_memory_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});
    if(comm_reuse) {
      cct_metric_data_increment(comm_reuse_memory_distance_metric_id, commReusePairNode, (cct_metric_data_t){.i = (l3_attributed_rd) });
      //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
      cct_metric_data_increment(comm_reuse_memory_distance_count_metric_id, commReusePairNode, (cct_metric_data_t){.i = 1});
    }
  }

  reuseTemporal += attributed_inc;
  if (reuse_type == REUSE_TEMPORAL){
    cct_metric_data_increment(temporal_reuse_metric_id, reusePairNode, (cct_metric_data_t){.i = attributed_inc});
    //fprintf(stderr, "reuse distance temporal: %ld\n", inc);
  } else if (reuse_type == REUSE_SPATIAL) {
    cct_metric_data_increment(spatial_reuse_metric_id, reusePairNode, (cct_metric_data_t){.i = attributed_inc});
    //fprintf(stderr, "reuse distance spatial: %ld\n", inc);
  } else if ((wpi->sample.reuseType == REUSE_TEMPORAL) && l3_inc_attribute) {
    cct_metric_data_increment(l3_temporal_reuse_metric_id, globalReuseWPs.table[wt->location].reusePairNode, (cct_metric_data_t){.i = l3_attributed_inc});
    if(comm_reuse) {
      cct_metric_data_increment(comm_temporal_reuse_metric_id, globalReuseWPs.table[wt->location].commReusePairNode, (cct_metric_data_t){.i = l3_attributed_inc});
    }
  } else if ((wpi->sample.reuseType == REUSE_SPATIAL) && l3_inc_attribute) {
    cct_metric_data_increment(l3_spatial_reuse_metric_id, globalReuseWPs.table[wt->location].reusePairNode, (cct_metric_data_t){.i = l3_attributed_inc});
    if(comm_reuse) {
      cct_metric_data_increment(comm_spatial_reuse_metric_id, globalReuseWPs.table[wt->location].commReusePairNode, (cct_metric_data_t){.i = l3_attributed_inc});
    }
  }
  cct_metric_data_increment(reuse_time_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = time_distance});
  cct_metric_data_increment(reuse_time_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});
  return ALREADY_DISABLED;
}

//#if 0
static WPTriggerActionType AMDReuseWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  //fprintf(stderr, "in ReuseWPCallback\n");
  //fprintf(stderr, "trap in AMDReuseWPCallback happens on address %lx\n", wpi->sample.va);
#if 0  // jqswang:TODO, how to handle it?
  if(!wt->pc) {
    // if the ip is 0, let's drop the WP
    //return RETAIN_WP;
    return ALREADY_DISABLED;
  }
#endif //jqswang

  uint64_t val[2][3];
  //for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
  //fprintf(stderr, "reading counter in WP Trap\n");
    assert(linux_perf_read_event_counter(amd_reuse_distance_event /*reuse_distance_events[i]*/, val[0]) >= 0);
    //assert(linux_perf_read_event_counter_l1_amd( amd_reuse_distance_event, val[0]) >= 0);
    //fprintf(stderr, "USE: %lu %lu %lu,  REUSE: %lu %lu %lu\n", wpi->sample.reuseDistance[i][0], wpi->sample.reuseDistance[i][1], wpi->sample.reuseDistance[i][2], val[i][0], val[i][1], val[i][2]);
    //fprintf(stderr, "DIFF: %lu\n", val[i][0] - wpi->sample.reuseDistance[i][0]);
    for(int j=0; j < 3; j++){
      if (val[0][j] >= wpi->sample.reuseDistance[0][j]){
        //fprintf(stderr, "before subtraction: val[%d][%d]: %ld, wpi->sample.reuseDistance[%d][%d]: %ld\n", i, j, val[i][j], i, j, wpi->sample.reuseDistance[i][j]);
	//fprintf(stderr, "before subtraction: val[0][%d]: %ld, wpi->sample.reuseDistance[0][%d]: %ld\n", j, val[0][j], j, wpi->sample.reuseDistance[0][j]);
        val[0][j] -= wpi->sample.reuseDistance[0][j];
        //fprintf(stderr, "after subtraction: val[0][%d]: %ld, wpi->sample.reuseDistance[0][%d]: %ld\n", j, val[0][j], j, wpi->sample.reuseDistance[0][j]);
      }
      else { //Something wrong happens here and the record is not reliable. Drop it!
        return ALREADY_DISABLED;
      }
    }
  //}
  // Report a reuse
  // returns 1.0 now but previously returns 1/sharer s.t. sharer is #wp sharing the same context as the trapped wp 
  double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
  //fprintf(stderr, "myProportion: %0.2lf\n", myProportion);
  uint64_t numDiffSamples = /*100000;*/ GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
  uint64_t inc = numDiffSamples;
  //fprintf(stderr, "inc: %ld\n", inc);
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  uint64_t time_distance = rdtsc() - wpi->startTime;

#ifdef REUSE_HISTO
  //fprintf(stderr, "inside REUSE_HISTO\n");
  //cct_node_t *reuseNode = getPreciseNode(wt->ctxt, wt->pc, temporal_reuse_metric_id );
  //sample_val_t v = hpcrun_sample_callpath(wt->ctxt, temporal_reuse_metric_id, SAMPLE_NO_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
  //cct_node_t *reuseNode = v.sample_node;
#if 0
  if (reuse_output_trace){
    WriteWitchTraceOutput("REUSE_DISTANCE: %d %d %lu,", hpcrun_cct_persistent_id(wpi->sample.node), hpcrun_cct_persistent_id(reuseNode), inc);
    for(int i=0; i < MIN(2, reuse_distance_num_events); i++){
      WriteWitchTraceOutput(" %lu %lu %lu,", val[i][0], val[i][1], val[i][2]);
    }
    WriteWitchTraceOutput("\n");
  } else{
#endif
    uint64_t rd = 0;
    //for(int i=0; i < MIN(2, reuse_distance_num_events); i++){
      //assert(val[0][1] == 0 && val[0][2] == 0); // no counter multiplexing allowed
      rd += val[0][0];
    //}
    //fprintf(stderr, "reuse distance %ld is detected %d times on address %lx\n", rd, inc, wpi->sample.va);
    ReuseAddDistance(rd, inc);
  //}

#else

  cct_node_t *reusePairNode;
  if (wpi->sample.reuseType == REUSE_TEMPORAL){
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, temporal_reuse_metric_id, SAMPLE_NO_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
    cct_node_t *reuseNode = v.sample_node;
    //fprintf(stderr, "reuse of REUSE_TEMPORAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode /*bottomNode*/, wpi->sample.node /*topNode*/, joinNodes[E_TEMPORALLY_REUSED_BY][joinNodeIdx] /* joinNode*/);
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node /*bottomNode*/, reuseNode /*topNode*/, joinNodes[E_TEMPORALLY_REUSED_FROM][joinNodeIdx] /* joinNode*/);
    }
  }
  else { // REUSE_SPATIAL
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, spatial_reuse_metric_id, SAMPLE_NO_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
    cct_node_t *reuseNode = v.sample_node;
    //fprintf(stderr, "reuse of REUSE_SPATIAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode /*bottomNode*/, wpi->sample.node /*topNode*/, joinNodes[E_SPATIALLY_REUSED_BY][joinNodeIdx] /* joinNode*/);
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node /*bottomNode*/, reuseNode /*topNode*/, joinNodes[E_SPATIALLY_REUSED_FROM][joinNodeIdx] /* joinNode*/);
    }
  }
  cct_metric_data_increment(reuse_memory_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = (val[0][0] + val[1][0]) });
  //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
  cct_metric_data_increment(reuse_memory_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});

  reuseTemporal += inc;
  if (wpi->sample.reuseType == REUSE_TEMPORAL){
    cct_metric_data_increment(temporal_reuse_metric_id, reusePairNode, (cct_metric_data_t){.i = inc});
    //fprintf(stderr, "reuse distance temporal: %ld\n", inc);
  } else {
    cct_metric_data_increment(spatial_reuse_metric_id, reusePairNode, (cct_metric_data_t){.i = inc});
    //fprintf(stderr, "reuse distance spatial: %ld\n", inc);
  }
  cct_metric_data_increment(reuse_time_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = time_distance});
  cct_metric_data_increment(reuse_time_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});
#endif
  return ALREADY_DISABLED;
}
//#endif

//#if 0
static WPTriggerActionType AMDReuseTrackerWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
	//fprintf(stderr, "trap in AMDReuseWPCallback happens\n");
	trap_count++;
#if 0  // jqswang:TODO, how to handle it?
  if(!wt->pc) {
    // if the ip is 0, let's drop the WP
    //return RETAIN_WP;
    return ALREADY_DISABLED;
  }
#endif //jqswang

  int reuse_type = REUSE_NONE;
  bool l3_inc_attribute = false;
  bool time_distance_attribute = false;

  uint64_t numDiffSamples = 0;
  bool comm_reuse = false;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  bool source_code_line_attribution = false;
  uint64_t attributed_rd = 0;
  uint64_t attributed_inc = 0;
  uint64_t l3_attributed_rd = 0;
  uint64_t l3_attributed_inc = 0;
  uint64_t time_distance;
  int me = TD_GET(core_profile_trace_data.id);
  int monitored_tid = globalReuseWPs.table[wt->location].monitored_tid; 

  if(me == monitored_tid) {
    double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
    numDiffSamples = GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
    double inc_scale = dynamic_global_thread_count / (double) max_used_wp_count;
    uint64_t inc = numDiffSamples * inc_scale;
    uint64_t trapTime = rdtsc();
    // before
    bool handle_trap = false;
    bool post_inc_flag = false;
    uint64_t theCounter = globalReuseWPs.table[wt->location].counter;
    if((theCounter & 1) == 0) {
      if(__sync_bool_compare_and_swap(&globalReuseWPs.table[wt->location].counter, theCounter, theCounter+1)) {
        if(globalReuseWPs.table[wt->location].active) {

          //globalReuseWPs.table[wt->location].trap_just_happened = true;
          if(profiling_mode == L1 || profiling_mode == MIXED)
            numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
          //numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
          globalReuseWPs.table[wt->location].active = false;
          handle_trap = true;
  	  //fprintf(stderr, "critical region is entered at trap\n");	  
        }
        if(globalReuseWPs.table[wt->location].sharedActive) {
          post_inc_flag = true;	
        }
        globalReuseWPs.table[wt->location].counter++;
      }
    }
    // after

    if(handle_trap) {
      uint64_t rd = 0;
      uint64_t val[2][3];

      //for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
        assert(linux_perf_read_event_counter_l1_amd( amd_reuse_distance_event, val[0]) >= 0); 
        //val[0][1] = 0; 
        //wpi->sample.reuseDistance[0][1] = 0;

        for(int j=0; j < 3; j++){
          if (val[0][j] >= wpi->sample.reuseDistance[0][j]){
            val[0][j] -= wpi->sample.reuseDistance[0][j];
          }
          /*else { //Something wrong happens here and the record is not reliable. Drop it! 
            return ALREADY_DISABLED;
          }*/
        }
      //}

      //for(int i=0; i < MIN(2, reuse_distance_num_events); i++){
        //fprintf(stderr, "before assert %d\n", i);
        //assert(val[0][1] == 0 && val[0][2] == 0); // no counter multiplexing allowed
        //fprintf(stderr, "after assert %d\n", i);
        rd += val[0][0];
      //}
      // Report a reuse
      // returns 1.0 now but previously returns 1/sharer s.t. sharer is #wp sharing the same context as the trapped wp

      //fprintf(stderr, "before time_distance\n");
      time_distance = rdtsc() - wpi->startTime;
      //fprintf(stderr, "private reuse %ld is detected me: %d, monitored_tid: %d\n", rd, me, monitored_tid);
      ReuseAddDistance(rd, inc);
      reuse_type = wpi->sample.reuseType;
      attributed_rd = rd;
      attributed_inc = inc;	
      source_code_line_attribution = true;
    }
    if(post_inc_flag) {
      globalReuseWPs.table[wt->location].inc = inc;
      //fprintf(stderr, "inc is posted, sharedReuse: %d, published_rd: %d\n", globalReuseWPs.table[wt->location].sharedActive, globalReuseWPs.table[wt->location].rd);
    } else if(globalReuseWPs.table[wt->location].rd > 0) {
      SharedReuseAddDistance(globalReuseWPs.table[wt->location].rd, inc);
      if(globalReuseWPs.table[wt->location].is_rar == false) {
        CommunicationReuseAddDistance(globalReuseWPs.table[wt->location].rd, inc);
        comm_reuse = true;
      }
      if(profiling_mode == L3)
        numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
      l3_attributed_inc = inc;
      l3_attributed_rd = globalReuseWPs.table[wt->location].rd;
      l3_inc_attribute = true;	
    }
    globalReuseWPs.table[wt->location].self_trap = false;
    theCounter = MonitoredNode.counter;
    if((theCounter & 1) == 0) {
      if(__sync_bool_compare_and_swap(&MonitoredNode.counter, theCounter, theCounter+1)) {
        if(globalReuseWPs.table[wt->location].time == MonitoredNode.timestamp) {
          MonitoredNode.self_trap = false;
          MonitoredNode.trap_timestamp = trapTime;
        }
        MonitoredNode.counter++;
      }
    }
  } else {
    if(/*(wt->accessType == STORE) || (wt->accessType == LOAD_AND_STORE)*/ wpi->sample.type == WP_WRITE) {
      uint64_t theCounter = globalReuseWPs.table[wt->location].counter;
      if((theCounter & 1) == 0) {
        if(__sync_bool_compare_and_swap(&globalReuseWPs.table[wt->location].counter, theCounter, theCounter+1)) {
          if(globalReuseWPs.table[wt->location].active) {

            //globalReuseWPs.table[wt->location].trap_just_happened = true;
            if(profiling_mode == L1 || profiling_mode == MIXED)
              numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
            //numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
            //fprintf(stderr, "L1 invalidation is detected\n");
            globalReuseWPs.table[wt->location].active = false;
          }
          globalReuseWPs.table[wt->location].counter++;
        }
      }
    }

    if(profiling_mode == L3 || profiling_mode == MIXED) {
      int affinity_l3 = 0;
      int my_core = sched_getcpu();
      if(l3_count > 1)
        affinity_l3 = thread_to_l3_mapping[my_core];
      bool post_rd_flag = false;
      bool handle_trap = false;
      uint64_t theCounter = globalReuseWPs.table[wt->location].counter;
      if((theCounter & 1) == 0) {
        if(__sync_bool_compare_and_swap(&globalReuseWPs.table[wt->location].counter, theCounter, theCounter+1)) {
          if(globalReuseWPs.table[wt->location].sharedActive) {

            //globalReuseWPs.table[wt->location].trap_just_happened = true;
            //numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
            if((l3_count == 1) || (wpi->sample.L3Id == affinity_l3)) {
              globalReuseWPs.table[wt->location].sharedActive = false;
              handle_trap = true;
              if(globalReuseWPs.table[wt->location].inc == 0) {
                post_rd_flag = true;
              }
	      //fprintf(stderr, "L3 reuse is detected, me: %d, monitored_tid: %d\n", me, monitored_tid);
              /*if(wpi->sample.type == WP_WRITE)
                fprintf(stderr, "L1 invalidation is detected in the same L3\n");*/
            } else if(/*(wt->accessType == STORE) || (wt->accessType == LOAD_AND_STORE)*/wpi->sample.type == WP_WRITE) {
              //fprintf(stderr, "L3 invalidation is detected, me: %d, monitored_tid: %d\n", me, monitored_tid);
              globalReuseWPs.table[wt->location].sharedActive = false;
              //fprintf(stderr, "L3 invalidation is detected\n");
            }

          }
          globalReuseWPs.table[wt->location].counter++;
        }
      }

      if(handle_trap) {
        double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
        numDiffSamples = hpcrun_id2metric(wpi->sample.sampledMetricId)->period; //GetWeightedMetricDiffAndReset(wpi->sample.node, wpi->sample.sampledMetricId, myProportion);
        double inc_scale = dynamic_global_thread_count / (double) max_used_wp_count;
        uint64_t inc = numDiffSamples * inc_scale;
        uint64_t rd = 0;
        uint64_t val[2][3];
        //uint64_t global_val[3];
        for(int i=0; i < 2; i++) {
          for(int j=0; j < 3; j++) {
            val[i][j] = 0;
            //global_val[i] = 0;
          }
        }

        //fprintf(stderr, "a trap happens in the same L3\n");
        // fprintf(stderr, "a trap due to load use happens in thread %d mapped to core %d located in L3 %d armed by thread %d in L3 %d\n", me, my_core, affinity_l3, globalReuseWPs.table[wt->location].tid, wpi->sample.L3Id);
        // before
        int cur_global_thread_count = global_thread_count;
        for(int i = 0; i < cur_global_thread_count; i++) {
          if((mapping_size == 0) || (l3_count == 1) || (thread_to_l3_mapping[mapping_vector[i % mapping_size]] == affinity_l3)) {
            //for (int j=0; j < MIN(2, reuse_distance_num_events); j++){
              uint64_t val1[3];
              linux_perf_read_event_counter_shared( amd_reuse_distance_event, val1, i/*locality_vector[affinity_l3][i+1]*/);
              for(int k=0; k < 3; k++) {
                val[0][k] += val1[k];
              }
            //}
          }
        }
        // after	

        // before
        //for(int i=0; i < 2; i++) {
          if(val[0][0] >= wpi->sample.sharedReuseDistance[0][0]) {

            val[0][0] -= wpi->sample.sharedReuseDistance[0][0];
          }
          rd += val[0][0];
        //}

        if (wpi->sample.reuseType == REUSE_TEMPORAL)
          reuse_type = L3_REUSE_TEMPORAL;
        else if (wpi->sample.reuseType == REUSE_SPATIAL)
          reuse_type = L3_REUSE_SPATIAL;	

        if(post_rd_flag) {
          globalReuseWPs.table[wt->location].rd = rd;
          if(wpi->sample.accessType == LOAD /*&& wt->accessType == LOAD*/)	
            globalReuseWPs.table[wt->location].is_rar = true;
        } else if(globalReuseWPs.table[wt->location].inc > 0) {
          SharedReuseAddDistance(rd, globalReuseWPs.table[wt->location].inc);
          if(wpi->sample.accessType != LOAD /*|| wt->accessType != LOAD*/) {
            CommunicationReuseAddDistance(rd, globalReuseWPs.table[wt->location].inc);
            comm_reuse = true;
          }
          if(profiling_mode == L3)
            numWatchpointArmingAttempt[wt->location] = SAMPLES_POST_FULL_RESET_VAL;
          //shared_reuse_counter++;
          l3_attributed_inc = globalReuseWPs.table[wt->location].inc;
          l3_attributed_rd = rd;
          l3_inc_attribute = true;	
        }
        //attributed_inc = inc;
        source_code_line_attribution = true;
        attributed_rd = rd; //rd_with_store;
        time_distance = rdtsc() - wpi->startTime;
        // after
      }
      // after
    }

  }
//#if 0
  cct_node_t *reusePairNode;
  cct_node_t *commReusePairNode;
  if (reuse_type == REUSE_TEMPORAL){
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, temporal_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *reuseNode = v.sample_node;
    //fprintf(stderr, "reuse of REUSE_TEMPORAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode, wpi->sample.node, joinNodes[E_TEMPORALLY_REUSED_BY][joinNodeIdx]);
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node, reuseNode, joinNodes[E_TEMPORALLY_REUSED_FROM][joinNodeIdx]);
    }
  }
  else if (reuse_type == REUSE_SPATIAL) { // REUSE_SPATIAL
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, spatial_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *reuseNode = v.sample_node;
    //fprintf(stderr, "reuse of REUSE_SPATIAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode, wpi->sample.node, joinNodes[E_SPATIALLY_REUSED_BY][joinNodeIdx]);
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node, reuseNode, joinNodes[E_SPATIALLY_REUSED_FROM][joinNodeIdx]);
    }
  } else if (reuse_type == L3_REUSE_TEMPORAL){
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, l3_temporal_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *reuseNode = v.sample_node;
    sample_val_t v1 = hpcrun_sample_callpath(wt->ctxt, comm_temporal_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *commReuseNode = v1.sample_node;
    //fprintf(stderr, "reuse of REUSE_TEMPORAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode, wpi->sample.node, joinNodes[E_L3_TEMPORALLY_REUSED_BY][joinNodeIdx]);
      if (comm_reuse) {
        commReusePairNode = getConcatenatedNode(commReuseNode, wpi->sample.node, joinNodes[E_COMM_TEMPORALLY_REUSED_BY][joinNodeIdx]);
      }
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node, reuseNode, joinNodes[E_L3_TEMPORALLY_REUSED_FROM][joinNodeIdx]);
      if (comm_reuse) {
        commReusePairNode = getConcatenatedNode(wpi->sample.node, commReuseNode, joinNodes[E_COMM_TEMPORALLY_REUSED_FROM][joinNodeIdx]); 
      }
    }
    globalReuseWPs.table[wt->location].reusePairNode = reusePairNode;
    globalReuseWPs.table[wt->location].commReusePairNode = commReusePairNode;
  }
  else if (reuse_type == L3_REUSE_SPATIAL) { // REUSE_SPATIAL
    sample_val_t v = hpcrun_sample_callpath(wt->ctxt, l3_spatial_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *reuseNode = v.sample_node;
    sample_val_t v1 = hpcrun_sample_callpath(wt->ctxt, comm_spatial_reuse_metric_id, SAMPLE_NO_INC, 0, 1, NULL);
    cct_node_t *commReuseNode = v1.sample_node;
    //fprintf(stderr, "reuse of REUSE_SPATIAL is detected\n");
    if (reuse_concatenate_use_reuse){
      reusePairNode = getConcatenatedNode(reuseNode, wpi->sample.node, joinNodes[E_L3_SPATIALLY_REUSED_BY][joinNodeIdx]);
      if(comm_reuse) {
        commReusePairNode = getConcatenatedNode(commReuseNode, wpi->sample.node, joinNodes[E_COMM_SPATIALLY_REUSED_BY][joinNodeIdx]);
      }
    }else{
      reusePairNode = getConcatenatedNode(wpi->sample.node, reuseNode, joinNodes[E_L3_SPATIALLY_REUSED_FROM][joinNodeIdx]);
      if(comm_reuse) {
        commReusePairNode = getConcatenatedNode(wpi->sample.node, commReuseNode, joinNodes[E_COMM_SPATIALLY_REUSED_FROM][joinNodeIdx]);
      }
    }
    globalReuseWPs.table[wt->location].reusePairNode = reusePairNode;
    globalReuseWPs.table[wt->location].commReusePairNode = commReusePairNode;
  }
  if ((reuse_type == REUSE_TEMPORAL) || (reuse_type == REUSE_SPATIAL)) {
    cct_metric_data_increment(reuse_memory_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = (attributed_rd) });
    //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
    cct_metric_data_increment(reuse_memory_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});
  } else if ((reuse_type == L3_REUSE_TEMPORAL) || (reuse_type == L3_REUSE_SPATIAL)) {
    cct_metric_data_increment(l3_reuse_memory_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = (attributed_rd) });
    //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
    cct_metric_data_increment(l3_reuse_memory_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});
    if(comm_reuse) {
      cct_metric_data_increment(comm_reuse_memory_distance_metric_id, commReusePairNode, (cct_metric_data_t){.i = (l3_attributed_rd) });
      //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
      cct_metric_data_increment(comm_reuse_memory_distance_count_metric_id, commReusePairNode, (cct_metric_data_t){.i = 1});
    }
  }

  reuseTemporal += attributed_inc;
  if (reuse_type == REUSE_TEMPORAL){
    cct_metric_data_increment(temporal_reuse_metric_id, reusePairNode, (cct_metric_data_t){.i = attributed_inc});
    //fprintf(stderr, "reuse distance temporal: %ld\n", inc);
  } else if (reuse_type == REUSE_SPATIAL) {
    cct_metric_data_increment(spatial_reuse_metric_id, reusePairNode, (cct_metric_data_t){.i = attributed_inc});
    //fprintf(stderr, "reuse distance spatial: %ld\n", inc);
  } else if ((wpi->sample.reuseType == REUSE_TEMPORAL) && l3_inc_attribute) {
    cct_metric_data_increment(l3_temporal_reuse_metric_id, globalReuseWPs.table[wt->location].reusePairNode, (cct_metric_data_t){.i = l3_attributed_inc});
    if(comm_reuse) {
      cct_metric_data_increment(comm_temporal_reuse_metric_id, globalReuseWPs.table[wt->location].commReusePairNode, (cct_metric_data_t){.i = l3_attributed_inc});
    }
  } else if ((wpi->sample.reuseType == REUSE_SPATIAL) && l3_inc_attribute) {
    cct_metric_data_increment(l3_spatial_reuse_metric_id, globalReuseWPs.table[wt->location].reusePairNode, (cct_metric_data_t){.i = l3_attributed_inc});
    if(comm_reuse) {
      cct_metric_data_increment(comm_spatial_reuse_metric_id, globalReuseWPs.table[wt->location].commReusePairNode, (cct_metric_data_t){.i = l3_attributed_inc});
    }
  }
  cct_metric_data_increment(reuse_time_distance_metric_id, reusePairNode, (cct_metric_data_t){.i = time_distance});
  cct_metric_data_increment(reuse_time_distance_count_metric_id, reusePairNode, (cct_metric_data_t){.i = 1});
//#endif
	return ALREADY_DISABLED;
}
//#endif

static WPTriggerActionType AMDCommWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
	//fprintf(stderr, "AMDCommWPCallback is called and sample period is %ld\n", hpcrun_id2metric(wpi->sample.sampledMetricId)->period);
	int metricId = -1;
  const void* joinNode;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  number_of_traps++;
  int max_thread_num = wpi->sample.first_accessing_tid;
  if(max_thread_num < TD_GET(core_profile_trace_data.id))
  {
    max_thread_num = TD_GET(core_profile_trace_data.id);
  }
  if(fs_matrix_size < max_thread_num)
  {
    fs_matrix_size =  max_thread_num; // false sharing
    ts_matrix_size =  max_thread_num; // true sharing
    as_matrix_size =  max_thread_num; // any sharing
  }

  //fprintf(stderr, "AMDCommWPCallback is called, wt->va: %lx, wt->accessType: %d, wpi->sample.samplerAccessType: %d, wpi->sample.sampleType: %d, LOAD: %d, STORE: %d, LOAD_AND_STORE: %d\n", wt->va, wt->accessType, wpi->sample.samplerAccessType, wpi->sample.sampleType, LOAD, STORE, LOAD_AND_STORE);
  int64_t trapTime = rdtsc();
  int max_core_num = wpi->sample.first_accessing_core_id;

  if(max_core_num < sched_getcpu()) // sched_getcpu() finds the cpu on which the thread is running
  {   
    max_core_num = sched_getcpu(); 
  }
  if(fs_core_matrix_size < max_core_num)
  {
    fs_core_matrix_size =  max_core_num;
    ts_core_matrix_size =  max_core_num;
    as_core_matrix_size =  max_core_num;
  }

  long global_sampling_period = hpcrun_id2metric(wpi->sample.sampledMetricId)->period;

  int index1 = wpi->sample.first_accessing_tid; 
  int index2 = TD_GET(core_profile_trace_data.id); 

  int core_id1 = wpi->sample.first_accessing_core_id;  
  int core_id2 = sched_getcpu();  
  int flag = 0;
  // if ts2 > tprev then
//#if 0
if((prev_timestamp < wpi->sample.bulletinBoardTimestamp) && ((trapTime - wpi->sample.bulletinBoardTimestamp)  <=  (2 * wpi->sample.expirationPeriod))) { 
    if(wt->accessType == LOAD && wpi->sample.samplerAccessType == LOAD){
      if(wpi->sample.sampleType == ALL_LOAD) {
        flag = 1;
        number_of_caught_read_traps++;
      }
    } else if ((wt->accessType == STORE || wt->accessType == LOAD_AND_STORE) && wpi->sample.samplerAccessType == STORE) {
      if(wpi->sample.sampleType == ALL_STORE) {
        flag = 2;
        number_of_caught_write_traps++;
      }
    }
 #if 0 
    else if (wt->accessType == LOAD_AND_STORE && wpi->sample.samplerAccessType == LOAD_AND_STORE){
      if(wpi->sample.sampleType == ALL_LOAD) {
        flag = 1;
        number_of_caught_read_write_traps++;
      }
      if(wpi->sample.sampleType == ALL_STORE) {
        flag = 2;
        number_of_caught_read_write_traps++;
      }
    }
#endif
}
//#endif

#if 0
//if((prev_timestamp < wpi->sample.bulletinBoardTimestamp) && ((trapTime - wpi->sample.bulletinBoardTimestamp)  <  wpi->sample.expirationPeriod)) {
  if(wt->accessType == LOAD) {
	  flag = 1;
  } else if (wt->accessType == STORE || wt->accessType == LOAD_AND_STORE) {
	  flag = 2;
  }
//}
#endif

  if (flag == 1) { // Load trap (WAR)
    void * cacheLineBaseAddress = (void *) ALIGN_TO_CACHE_LINE((size_t)wt->va);    
    double increment = (double) /*valid_sample_count1 / valid_sample_count **/ /*thread_coefficient(as_matrix_size) **/ CACHE_LINE_SZ/MAX_WP_LENGTH / wpConfig.maxWP * global_sampling_period; 
#if 0    
    if(global_thread_count > 2)
    	valid_sample_count = 0;
#endif
    // if [M1 , M1 + δ1 ) overlaps with [M2 , M2 + δ2 ) then
    if(GET_OVERLAP_BYTES(wpi->sample.target_va, wpi->sample.accessLength, wt->va, wt->accessLength) > 0) {
      int id = -1;
      // Record true sharing
      trueWRIns ++;
      metricId =  true_wr_metric_id;
      joinNode = joinNodes[E_TRUE_WR_SHARE][joinNodeIdx];
      ts_matrix[index1][index2] = ts_matrix[index1][index2] + increment;
      //fprintf(stderr, "RAW true sharing is detected at WP trap\n");
      war_ts_matrix[index1][index2] = war_ts_matrix[index1][index2] + increment;
      if(core_id1 != core_id2) {
        ts_core_matrix[core_id1][core_id2] = ts_core_matrix[core_id1][core_id2] + increment;
        war_ts_core_matrix[core_id1][core_id2] = war_ts_core_matrix[core_id1][core_id2] + increment;
      }

    } else {
      int id = -1;
      // Record false sharing
      falseWRIns ++;
      metricId =  false_wr_metric_id;
      joinNode = joinNodes[E_FALSE_WR_SHARE][joinNodeIdx];
      fs_matrix[index1][index2] = fs_matrix[index1][index2] + increment;
      war_fs_matrix[index1][index2] = war_fs_matrix[index1][index2] + increment;
      //fprintf(stderr, "false sharing is detected at WP trap\n");
      if(core_id1 != core_id2) {
        fs_core_matrix[core_id1][core_id2] = fs_core_matrix[core_id1][core_id2] + increment;
        war_fs_core_matrix[core_id1][core_id2] = war_fs_core_matrix[core_id1][core_id2] + increment;
      }
    }
    as_matrix[index1][index2] = as_matrix[index1][index2] + increment;
    war_as_matrix[index1][index2] = war_as_matrix[index1][index2] + increment;
    if(core_id1 != core_id2) {
      as_core_matrix[core_id1][core_id2] = as_core_matrix[core_id1][core_id2] + increment; 
      war_as_core_matrix[core_id1][core_id2] = war_as_core_matrix[core_id1][core_id2] + increment;
    }
    // tprev = ts2
    prev_timestamp = wpi->sample.bulletinBoardTimestamp;
  }
  else if (flag == 2) { // Store trap (WAW)
    void * cacheLineBaseAddress = (void *) ALIGN_TO_CACHE_LINE((size_t)wt->va);    
    double increment = (double) /*valid_sample_count1 / valid_sample_count **/ /*thread_coefficient(as_matrix_size) **/ CACHE_LINE_SZ/MAX_WP_LENGTH / wpConfig.maxWP * global_sampling_period; 
#if 0
    if(global_thread_count > 2)
    	valid_sample_count = 0;
#endif
    // if [M1 , M1 + δ1 ) overlaps with [M2 , M2 + δ2 ) then
    if(GET_OVERLAP_BYTES(wpi->sample.target_va, wpi->sample.accessLength, wt->va, wt->accessLength) > 0) {
      int id = -1;
      // Record true sharing
      trueWWIns ++;
      metricId =  true_ww_metric_id;
      joinNode = joinNodes[E_TRUE_WW_SHARE][joinNodeIdx];
      ts_matrix[index1][index2] = ts_matrix[index1][index2] + increment;
      waw_ts_matrix[index1][index2] = waw_ts_matrix[index1][index2] + increment;
      //fprintf(stderr, "WAW true sharing is detected at WP trap\n");
      if(core_id1 != core_id2) {
        ts_core_matrix[core_id1][core_id2] = ts_core_matrix[core_id1][core_id2] + increment;
        waw_ts_core_matrix[core_id1][core_id2] = waw_ts_core_matrix[core_id1][core_id2] + increment;
      }

    } else {
      int id = -1;
      // Record false sharing
      falseWWIns ++;
      metricId =  false_ww_metric_id;
      joinNode = joinNodes[E_FALSE_WW_SHARE][joinNodeIdx];
      fs_matrix[index1][index2] = fs_matrix[index1][index2] + increment;
      waw_fs_matrix[index1][index2] = waw_fs_matrix[index1][index2] + increment;
      //fprintf(stderr, "false sharing is detected at WP trap\n");
      if(core_id1 != core_id2) {
        fs_core_matrix[core_id1][core_id2] = fs_core_matrix[core_id1][core_id2] + increment;
        waw_fs_core_matrix[core_id1][core_id2] = waw_fs_core_matrix[core_id1][core_id2] + increment;
      }
    }
    as_matrix[index1][index2] = as_matrix[index1][index2] + increment;
    waw_as_matrix[index1][index2] = waw_as_matrix[index1][index2] + increment;
    if(core_id1 != core_id2) {
      as_core_matrix[core_id1][core_id2] = as_core_matrix[core_id1][core_id2] + increment; 
      waw_as_core_matrix[core_id1][core_id2] = waw_as_core_matrix[core_id1][core_id2] + increment;
    }
    // tprev = ts2
    prev_timestamp = wpi->sample.bulletinBoardTimestamp;
  }
//#if 0
  sample_val_t v = hpcrun_sample_callpath(wt->ctxt, /*measured_metric_id*/metricId, SAMPLE_UNIT_INC, 0, 1, NULL);
  cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
  node = hpcrun_cct_insert_path_return_leaf(wpi->sample.node, node);
  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
  //fprintf(stderr, "source code line attribution here\n");
//#endif	
	return ALREADY_DISABLED;
}

// Handles the debug register trap (callback). When the PC reaches an adress (breakpoint) or accesses a designated adress (watchpoint), the cpu is trapped.
static WPTriggerActionType ComDetectiveWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  int metricId = -1;
  const void* joinNode;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  number_of_traps++;
  int max_thread_num = wpi->sample.first_accessing_tid;
  if(max_thread_num < TD_GET(core_profile_trace_data.id))
  {
    max_thread_num = TD_GET(core_profile_trace_data.id);
  }
  if(fs_matrix_size < max_thread_num)
  {
#if ADAMANT_USED
    matrix_size_set(max_thread_num);
#endif
    fs_matrix_size =  max_thread_num; // false sharing
    ts_matrix_size =  max_thread_num; // true sharing
    as_matrix_size =  max_thread_num; // any sharing
  }

  //fprintf(stderr, "wt->va: %lx, wt->accessType: %d\n", wt->va, wt->accessType);
  int64_t trapTime = rdtsc();
  int max_core_num = wpi->sample.first_accessing_core_id;

  if(max_core_num < sched_getcpu()) // sched_getcpu() finds the cpu on which the thread is running
  {   
    max_core_num = sched_getcpu(); 
  }
  if(fs_core_matrix_size < max_core_num)
  {
#if ADAMANT_USED
    core_matrix_size_set(max_core_num);
#endif
    fs_core_matrix_size =  max_core_num;
    ts_core_matrix_size =  max_core_num;
    as_core_matrix_size =  max_core_num;
  }

  long global_sampling_period = 0;

  int index1 = wpi->sample.first_accessing_tid; 
  int index2 = TD_GET(core_profile_trace_data.id); 

  int core_id1 = wpi->sample.first_accessing_core_id;  
  int core_id2 = sched_getcpu();  
  int flag = 0;
  // if ts2 > tprev then
  if((prev_timestamp < wpi->sample.bulletinBoardTimestamp) && ((trapTime - wpi->sample.bulletinBoardTimestamp)  <  wpi->sample.expirationPeriod)) { 
    if(wt->accessType == LOAD && wpi->sample.samplerAccessType == LOAD){
      if(wpi->sample.sampleType == ALL_LOAD) {
        global_sampling_period = global_load_sampling_period;
        flag = 1;
        number_of_caught_read_traps++;
      }
    } else if (wt->accessType == STORE && wpi->sample.samplerAccessType == STORE) {
      if(wpi->sample.sampleType == ALL_STORE) {
        global_sampling_period = global_store_sampling_period;
        flag = 2;
        number_of_caught_write_traps++;
      }
    } else if (wt->accessType == LOAD_AND_STORE && wpi->sample.samplerAccessType == LOAD_AND_STORE){
      if(wpi->sample.sampleType == ALL_LOAD) {
        global_sampling_period = global_load_sampling_period;
        flag = 1;
        number_of_caught_read_write_traps++;
      }
      if(wpi->sample.sampleType == ALL_STORE) {
        global_sampling_period = global_store_sampling_period;
        flag = 2;
        number_of_caught_read_write_traps++;
      }
    }
  }


  if (flag == 1) { // Load trap (WAR)
    void * cacheLineBaseAddress = (void *) ALIGN_TO_CACHE_LINE((size_t)wt->va);    
    double increment = (double) CACHE_LINE_SZ/MAX_WP_LENGTH / wpConfig.maxWP * global_sampling_period; 

    // if [M1 , M1 + δ1 ) overlaps with [M2 , M2 + δ2 ) then
    if(GET_OVERLAP_BYTES(wpi->sample.target_va, wpi->sample.accessLength, wt->va, wt->accessLength) > 0) {
      int id = -1;
      // Record true sharing
      trueWRIns ++;
      metricId =  true_wr_metric_id;
      joinNode = joinNodes[E_TRUE_WR_SHARE][joinNodeIdx];
#if ADAMANT_USED
      if(getenv(HPCRUN_OBJECT_LEVEL)) {
        inc_true_matrix( (uint64_t) wt->va, index1, index2, increment);
        inc_true_count((uint64_t) wt->va, increment);
        int obj_id1 = get_object_id_by_address(wpi->sample.target_va);
        int obj_id2 = get_object_id_by_address(wt->va);
        if(obj_id1 == 0 && obj_id2 == 0) {
          id = get_id_after_backtrace();
          //fprintf(stderr, "true sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
          inc_true_matrix_by_object_id(id, core_id1, core_id2, increment);
          inc_true_count_by_object_id(id, increment);
        }
        if(obj_id1 == 1 && obj_id2 == 1) {
          if(id == -1)
            id = get_id_after_backtrace();
          //fprintf(stderr, "true sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
          inc_true_matrix_by_object_id(id, core_id1, core_id2, increment);
          inc_true_count_by_object_id(id, increment);
        }
      }
#endif
      ts_matrix[index1][index2] = ts_matrix[index1][index2] + increment;
      war_ts_matrix[index1][index2] = war_ts_matrix[index1][index2] + increment;
      if(core_id1 != core_id2) {
#if ADAMANT_USED
        if(getenv(HPCRUN_OBJECT_LEVEL)) {
          inc_true_core_matrix( (uint64_t) wt->va, core_id1, core_id2, increment);
          inc_true_core_count((uint64_t) wt->va, increment);
          int obj_id1 = get_object_id_by_address(wpi->sample.target_va);
          int obj_id2 = get_object_id_by_address(wt->va);
          if(obj_id1 == 0 && obj_id2 == 0) {
            if(id == -1)
              id = get_id_after_backtrace();
            //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
            inc_true_core_matrix_by_object_id(id, core_id1, core_id2, increment);
            inc_true_core_count_by_object_id(id, increment);
          }
          if(obj_id1 == 1 && obj_id2 == 1) {
            if(id == -1)
              id = get_id_after_backtrace();
            //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
            inc_true_core_matrix_by_object_id(id, core_id1, core_id2, increment);
            inc_true_core_count_by_object_id(id, increment);
          }
        }
#endif
        ts_core_matrix[core_id1][core_id2] = ts_core_matrix[core_id1][core_id2] + increment;
        war_ts_core_matrix[core_id1][core_id2] = war_ts_core_matrix[core_id1][core_id2] + increment;
      }

    } else {
      int id = -1;
      // Record false sharing
      falseWRIns ++;
      metricId =  false_wr_metric_id;
      joinNode = joinNodes[E_FALSE_WR_SHARE][joinNodeIdx];
#if ADAMANT_USED
      if(getenv(HPCRUN_OBJECT_LEVEL)) {
        inc_false_matrix((uint64_t) wpi->sample.target_va, (uint64_t) wt->va, index1, index2, increment);
        inc_false_count((uint64_t) wpi->sample.target_va, (uint64_t) wt->va, increment);
        int obj_id1 = get_object_id_by_address(wpi->sample.target_va);
        int obj_id2 = get_object_id_by_address(wt->va);
        // debugging starts
        if((obj_id1 == obj_id2) && (obj_id1 == 998)) {
          //fprintf(stderr, "false sharing is detected between threads %d and %d on address %ld and address %ld\n", index1, index2, wpi->sample.target_va, wt->va);
          //sleep(4);
        }
        // debugging ends
        if(obj_id1 == 0 && obj_id2 == 0) {
          id = get_id_after_backtrace();
          //fprintf(stderr, "false sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
          inc_false_matrix_by_object_id(id, core_id1, core_id2, increment);
          inc_false_count_by_object_id(id, increment);
        }
        if(obj_id1 == 1 && obj_id2 == 1) {
          if(id == -1)
            id = get_id_after_backtrace();
          //fprintf(stderr, "false sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
          inc_false_matrix_by_object_id(id, core_id1, core_id2, increment);
          inc_false_count_by_object_id(id, increment);
        }
      }
#endif
      fs_matrix[index1][index2] = fs_matrix[index1][index2] + increment;
      war_fs_matrix[index1][index2] = war_fs_matrix[index1][index2] + increment;
      if(core_id1 != core_id2) {
#if ADAMANT_USED
        if(getenv(HPCRUN_OBJECT_LEVEL)) {
          inc_false_core_matrix((uint64_t) wpi->sample.target_va, (uint64_t) wt->va, core_id1, core_id2, increment);
          inc_false_core_count((uint64_t) wpi->sample.target_va, (uint64_t) wt->va, increment);
          int obj_id1 = get_object_id_by_address(wpi->sample.target_va);
          int obj_id2 = get_object_id_by_address(wt->va);
          if(obj_id1 == 0 && obj_id2 == 0) {
            if(id == -1)
              id = get_id_after_backtrace();
            //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
            inc_false_core_matrix_by_object_id(id, core_id1, core_id2, increment);
            inc_false_core_count_by_object_id(id, increment);
          }
          if(obj_id1 == 1 && obj_id2 == 1) {
            if(id == -1)
              id = get_id_after_backtrace();
            //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
            inc_false_core_matrix_by_object_id(id, core_id1, core_id2, increment);
            inc_false_core_count_by_object_id(id, increment);
          }
        }
#endif
        fs_core_matrix[core_id1][core_id2] = fs_core_matrix[core_id1][core_id2] + increment;
        war_fs_core_matrix[core_id1][core_id2] = war_fs_core_matrix[core_id1][core_id2] + increment;
      }
    }
    as_matrix[index1][index2] = as_matrix[index1][index2] + increment;
    war_as_matrix[index1][index2] = war_as_matrix[index1][index2] + increment;
    if(core_id1 != core_id2) {
      as_core_matrix[core_id1][core_id2] = as_core_matrix[core_id1][core_id2] + increment; 
      war_as_core_matrix[core_id1][core_id2] = war_as_core_matrix[core_id1][core_id2] + increment;
    }
    // tprev = ts2
    prev_timestamp = wpi->sample.bulletinBoardTimestamp;
  }
  else if (flag == 2) { // Store trap (WAW)
    void * cacheLineBaseAddress = (void *) ALIGN_TO_CACHE_LINE((size_t)wt->va);    
    double increment = (double) CACHE_LINE_SZ/MAX_WP_LENGTH / wpConfig.maxWP * global_sampling_period; 

    // if [M1 , M1 + δ1 ) overlaps with [M2 , M2 + δ2 ) then
    if(GET_OVERLAP_BYTES(wpi->sample.target_va, wpi->sample.accessLength, wt->va, wt->accessLength) > 0) {
      int id = -1;
      // Record true sharing
      trueWWIns ++;
      metricId =  true_ww_metric_id;
      joinNode = joinNodes[E_TRUE_WW_SHARE][joinNodeIdx];
#if ADAMANT_USED
      if(getenv(HPCRUN_OBJECT_LEVEL)) {
        inc_true_matrix( (uint64_t) wt->va, index1, index2, increment);
        inc_true_count((uint64_t) wt->va, increment);
        int obj_id1 = get_object_id_by_address(wpi->sample.target_va);
        int obj_id2 = get_object_id_by_address(wt->va);
        if(obj_id1 == 0 && obj_id2 == 0) {
          id = get_id_after_backtrace();
          //fprintf(stderr, "true sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
          inc_true_matrix_by_object_id(id, core_id1, core_id2, increment);
          inc_true_count_by_object_id(id, increment);
        }
        if(obj_id1 == 1 && obj_id2 == 1) {
          if(id == -1)
            id = get_id_after_backtrace();
          //fprintf(stderr, "true sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
          inc_true_matrix_by_object_id(id, core_id1, core_id2, increment);
          inc_true_count_by_object_id(id, increment);
        }
      }
#endif
      ts_matrix[index1][index2] = ts_matrix[index1][index2] + increment;
      waw_ts_matrix[index1][index2] = waw_ts_matrix[index1][index2] + increment;
      if(core_id1 != core_id2) {
#if ADAMANT_USED
        if(getenv(HPCRUN_OBJECT_LEVEL)) {
          inc_true_core_matrix( (uint64_t) wt->va, core_id1, core_id2, increment);
          inc_true_core_count((uint64_t) wt->va, increment);
          int obj_id1 = get_object_id_by_address(wpi->sample.target_va);
          int obj_id2 = get_object_id_by_address(wt->va);
          if(obj_id1 == 0 && obj_id2 == 0) {
            if(id == -1)
              id = get_id_after_backtrace();
            //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
            inc_true_core_matrix_by_object_id(id, core_id1, core_id2, increment);
            inc_true_core_count_by_object_id(id, increment);
          }
          if(obj_id1 == 1 && obj_id2 == 1) {
            if(id == -1)
              id = get_id_after_backtrace();
            //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
            inc_true_core_matrix_by_object_id(id, core_id1, core_id2, increment);
            inc_true_core_count_by_object_id(id, increment);
          }
        }
#endif
        ts_core_matrix[core_id1][core_id2] = ts_core_matrix[core_id1][core_id2] + increment;
        waw_ts_core_matrix[core_id1][core_id2] = waw_ts_core_matrix[core_id1][core_id2] + increment;
      }

    } else {
      int id = -1;
      // Record false sharing
      falseWWIns ++;
      metricId =  false_ww_metric_id;
      joinNode = joinNodes[E_FALSE_WW_SHARE][joinNodeIdx];
#if ADAMANT_USED
      if(getenv(HPCRUN_OBJECT_LEVEL)) {
        inc_false_matrix((uint64_t) wpi->sample.target_va, (uint64_t) wt->va, index1, index2, increment);
        inc_false_count((uint64_t) wpi->sample.target_va, (uint64_t) wt->va, increment);
        int obj_id1 = get_object_id_by_address(wpi->sample.target_va);
        int obj_id2 = get_object_id_by_address(wt->va);
        // debugging starts
        if((obj_id1 == obj_id2) && (obj_id1 == 998)) {
          //fprintf(stderr, "false sharing is detected between threads %d and %d on address %ld and address %ld\n", index1, index2, wpi->sample.target_va, wt->va);
          //sleep(4);
        }
        // debugging ends
        if(obj_id1 == 0 && obj_id2 == 0) {
          id = get_id_after_backtrace();
          //fprintf(stderr, "false sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
          inc_false_matrix_by_object_id(id, core_id1, core_id2, increment);
          inc_false_count_by_object_id(id, increment);
        }
        if(obj_id1 == 1 && obj_id2 == 1) {
          if(id == -1)
            id = get_id_after_backtrace();
          //fprintf(stderr, "false sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
          inc_false_matrix_by_object_id(id, core_id1, core_id2, increment);
          inc_false_count_by_object_id(id, increment);
        }
      }
#endif
      fs_matrix[index1][index2] = fs_matrix[index1][index2] + increment;
      waw_fs_matrix[index1][index2] = waw_fs_matrix[index1][index2] + increment;
      if(core_id1 != core_id2) {
#if ADAMANT_USED
        if(getenv(HPCRUN_OBJECT_LEVEL)) {
          inc_false_core_matrix((uint64_t) wpi->sample.target_va, (uint64_t) wt->va, core_id1, core_id2, increment);
          inc_false_core_count((uint64_t) wpi->sample.target_va, (uint64_t) wt->va, increment);
          int obj_id1 = get_object_id_by_address(wpi->sample.target_va);
          int obj_id2 = get_object_id_by_address(wt->va);
          if(obj_id1 == 0 && obj_id2 == 0) {
            if(id == -1)
              id = get_id_after_backtrace();
            //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
            inc_false_core_matrix_by_object_id(id, core_id1, core_id2, increment);
            inc_false_core_count_by_object_id(id, increment);
          }
          if(obj_id1 == 1 && obj_id2 == 1) {
            if(id == -1)
              id = get_id_after_backtrace();
            //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
            inc_false_core_matrix_by_object_id(id, core_id1, core_id2, increment);
            inc_false_core_count_by_object_id(id, increment);
          }
        }
#endif
        fs_core_matrix[core_id1][core_id2] = fs_core_matrix[core_id1][core_id2] + increment;
        waw_fs_core_matrix[core_id1][core_id2] = waw_fs_core_matrix[core_id1][core_id2] + increment;
      }
    }
    as_matrix[index1][index2] = as_matrix[index1][index2] + increment;
    waw_as_matrix[index1][index2] = waw_as_matrix[index1][index2] + increment;
    if(core_id1 != core_id2) {
      as_core_matrix[core_id1][core_id2] = as_core_matrix[core_id1][core_id2] + increment; 
      waw_as_core_matrix[core_id1][core_id2] = waw_as_core_matrix[core_id1][core_id2] + increment;
    }
    // tprev = ts2
    prev_timestamp = wpi->sample.bulletinBoardTimestamp;
  }

  sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0, 1, NULL);
  cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
  node = hpcrun_cct_insert_path_return_leaf(wpi->sample.node, node);
  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
  return ALREADY_DISABLED;
}

static WPTriggerActionType AllSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  assert(0);
}

static WPTriggerActionType TrueSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  int metricId = -1;
  const void* joinNode;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  if(wt->accessType == LOAD){
    trueWRIns ++;
    metricId = true_wr_metric_id;
    joinNode = joinNodes[E_TRUE_WR_SHARE][joinNodeIdx];
  } else {
    if(wpi->sample.accessType == LOAD) {
      trueRWIns ++;
      metricId = true_rw_metric_id;
      joinNode = joinNodes[E_TRUE_RW_SHARE][joinNodeIdx];
    } else{
      trueWWIns ++;
      metricId =  true_ww_metric_id;
      joinNode = joinNodes[E_TRUE_WW_SHARE][joinNodeIdx];
    }
  }

  sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
  // insert a special node
  cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
  node = hpcrun_cct_insert_path_return_leaf(wpi->sample.node, node);
  // update the metricId
  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
  return ALREADY_DISABLED;
}


static WPTriggerActionType IPCFalseSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  int metricId = -1;
  const void* joinNode;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

  if(wt->accessType == LOAD){
    falseWRIns ++;
    metricId = false_wr_metric_id;
    joinNode = joinNodes[E_IPC_FALSE_WR_SHARE][joinNodeIdx];
  } else {
    if(wpi->sample.accessType == LOAD) {
      falseRWIns ++;
      metricId = false_rw_metric_id;
      joinNode = joinNodes[E_IPC_FALSE_RW_SHARE][joinNodeIdx];
    } else{
      falseWWIns ++;
      metricId =  false_ww_metric_id;
      joinNode = joinNodes[E_IPC_FALSE_WW_SHARE][joinNodeIdx];
    }
  }

  sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
  // insert a special node
  cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
  node = hpcrun_cct_insert_array_path_return_leaf(wpi->sample.node, node);
  // update the metricId
  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
  return ALREADY_DISABLED;
}
static WPTriggerActionType IPCTrueSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  int metricId = -1;
  const void* joinNode;
  int joinNodeIdx = wpi->sample.isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;
  int i = 1;
  if(wt->accessType == LOAD){
    trueWRIns ++;
    metricId = true_wr_metric_id;
    joinNode = joinNodes[E_IPC_TRUE_WR_SHARE][joinNodeIdx];
  } else {
    if(wpi->sample.accessType == LOAD) {
      trueRWIns ++;
      metricId = true_rw_metric_id;
      joinNode = joinNodes[E_IPC_TRUE_RW_SHARE][joinNodeIdx];
    } else{
      trueWWIns ++;
      metricId =  true_ww_metric_id;
      joinNode = joinNodes[E_IPC_TRUE_WW_SHARE][joinNodeIdx];
    }
  }

  sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
  // insert a special node
  cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
  node = hpcrun_cct_insert_array_path_return_leaf(wpi->sample.node, node);
  // update the metricId
  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
  return ALREADY_DISABLED;
}
static WPTriggerActionType IPCAllSharingWPCallback(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt){
  return ALREADY_DISABLED;
}


static inline bool IsLibMonitorAddress(void * addr) {
  // race is ok,
  if(!libmonitorLM){
    libmonitorLM = hpcrun_loadmap_findByName(hpcrun_loadmap_findLoadName("libmonitor.so"))->dso_info;
  }

  if (addr >= libmonitorLM->start_addr && addr < libmonitorLM->end_addr){
    return true;
  }
  return false;
}

static inline bool IsHPCRunAddress(void * addr) {
  if(!hpcrunLM){
    hpcrunLM = hpcrun_loadmap_findByName(hpcrun_loadmap_findLoadName("libhpcrun.so"))->dso_info;
  }

  if (addr >= hpcrunLM->start_addr && addr < hpcrunLM->end_addr){
    return true;
  }
  return false;
}


static inline bool isTdataAddress(void *addr) {
  void *tdata = &inside_hpcrun;
  if ((addr > tdata-100) && (addr < tdata+100)) return true;
  return false;
}

static inline bool IsBlackListedWatchpointAddress(void *addr){
  for(int i = 0; i < numBlackListAddresses; i++){
    if (addr >= blackListAddresses[i].startAddr && addr < blackListAddresses[i].endAddr){
      return true;
    }
  }
  return false;
}

// Avoids Kernel address and zeros
static inline bool IsValidAddress(void * addr, void * pc){
  thread_data_t * td =  hpcrun_get_thread_data();
  if( (addr == 0) )
    return false;

  //fprintf(stderr, "failed here 1\n");
  if( (pc == 0) )
    return false;

  //fprintf(stderr, "failed here 2\n");
  if(( (void*)(td-1) <= addr) && (addr < (void*)(td+2))) // td data
    return false;
  if(IsAltStackAddress(addr))
    return false;
  if(IsFSorGS(addr))
    return false;   

  //fprintf(stderr, "failed here 3\n");
  if(IsBlackListedWatchpointAddress(addr) || IsBlackListedWatchpointAddress(pc)){
    return false;
  }

  if (isTdataAddress(addr))
    return false;

  if((addr && !(((unsigned long)addr) & 0xF0000000000000)) &&
      (pc && !(((unsigned long)pc) & 0xF0000000000000)))
    return true;
  return false;
}


void ReadSharedDataTransactionally(SharedData_t *localSharedData){
  // Laport's STM
  do{
    int64_t startCounter = gSharedData.counter;
    if(startCounter & 1)
      continue; // Some writer is updating

    __sync_synchronize();
    *localSharedData = gSharedData;
    __sync_synchronize();
    int64_t endCounter = gSharedData.counter;
    if(startCounter == endCounter)
      break;
  }while(1);
}
/*
#ifdef REUSE_HISTO
void ReadBulletinBoardTransactionally(ReuseBBEntry_t * prev_access, uint64_t data_addr, int * item_not_found_flag){
// Laport's STM
do{
int64_t startCounter = reuseBulletinBoard.counter;
if(startCounter & 1)
continue; // Some writer is updating

__sync_synchronize();
 *prev_access = getEntryFromReuseBulletinBoard(ALIGN_TO_CACHE_LINE((size_t)(data_addr)), item_not_found_flag);
 __sync_synchronize();
 int64_t endCounter = reuseBulletinBoard.counter;
 if(startCounter == endCounter)
 break;
 }while(1);
 }
#endif
 */
int static inline GetFloorWPLength(int accessLen){
  switch (accessLen) {
    default:
    case 8: return 8;
    case 7:case 6: case 5: case 4: return 4;
    case 3:case 2: return 2;
    case 1: return 1;
  }
}

int static inline GetFloorWPLengthAtAddress(void * address, int accessLen){
  uint8_t alignment = ((size_t) address) & (MAX_WP_LENGTH -1);

  switch (alignment) {
    case 1: case 3: case 5: case 7: /* 1-byte aligned */ return 1;
    case 2: case 6: /* 2-byte aligned */ return MIN(2, accessLen);
    case 0: /* 8-byte aligned */ return MIN(8, accessLen);
    case 4: /* 8-byte aligned */ return MIN(4, accessLen);
    default:
                                 assert(0 && "Should never reach here");
                                 return 1;
  }
}

/*
   typedef struct FalseSharingLocs{
   size_t va;
   int wpLen;
   }FalseSharingLocs;

// getting all false sharing memory regions
static inline void GetAllFalseSharingLocations(size_t va, int accessLen, size_t baseAddr, int maxFSLength, int * wpSizes, int curWPSizeIdx, int totalWPSizes, FalseSharingLocs * fsl, int * numFSLocs){
int curWPSize = wpSizes[curWPSizeIdx];
for(int i = 0; i < maxFSLength/curWPSize; i ++) {
size_t curAddr = baseAddr + i * curWPSize;
int overlapLen = GET_OVERLAP_BYTES(curAddr, curWPSize, va, accessLen);
if(0 >= overlapLen) {
fsl[*numFSLocs].va = curAddr;
fsl[*numFSLocs].wpLen = curWPSize;
(*numFSLocs)++;
}else if (curWPSize != overlapLen) {
if(curWPSizeIdx+1 < totalWPSizes) {
GetAllFalseSharingLocations(va, accessLen, curAddr, curWPSize, wpSizes, curWPSizeIdx+1, totalWPSizes, fsl, numFSLocs);
}
} else {
// Nop
}
}
}*/

#define PAGEMAP_ENTRY 8
#define GET_BIT(X,Y) (X & ((uint64_t)1<<Y)) >> Y
#define GET_PFN(X) X & 0x7FFFFFFFFFFFFF
const int __endian_bit = 1;
#define is_bigendian() ( (*(char*)&__endian_bit) == 0 )
#define MAX_BACKTRACE_LEN (1024)
#define MAX_IPC_PROCS (1024)
#define MAX_LOAD_MODULES (1024)
#define MAX_LM_PATH_LEN (1024)
#define INVALID_PHYSICAL_ADDRESS (0L)
#define INVALID_VIRUAL_ADDRESS ((void*) 0)
#define PAGE_SZ (4096)
#define PAGE_OFFSET(a) ((size_t)(a) & (PAGE_SZ-1))
#define GET_VA_PAGE(p) (void*)((size_t)(p) & ~(PAGE_SZ-1))
#define GET_PA_PAGE(p) (unsigned long)((p) & ~(PAGE_SZ-1))
#define PA_PATH "/proc/self/pagemap"
#define VA_PATH "/proc/self/maps"
#define VM_MAP_CHECK_FREQUENCY (128)


typedef enum LM_ENTRY_STATUS {UNUSED=0, TRANSIENT=1, STABLE=2} LM_ENTRY_STATUS_t;

typedef struct LM_ID_PATH{
  volatile LM_ENTRY_STATUS_t status;
  //    uint16_t lmId;
  char realPath[MAX_LM_PATH_LEN];
}LM_ID_PATH;


typedef struct IPC_FSInfo{
  char dummy2[CACHE_LINE_SZ];
  uint64_t time;
  pid_t tid;
  WatchPointType wpType;
  AccessType accessType;
  unsigned long address;
  unsigned long offset;
  int accessLen;
  uint16_t btLength;
  struct cct_addr_t backtrace[MAX_BACKTRACE_LEN];
  char dummy4[CACHE_LINE_SZ];
}IPC_FSInfo;

typedef struct IPC_SharedData{
  char dummy1[CACHE_LINE_SZ];
  volatile uint32_t curThreadsSubsribed;
  char dummy2[CACHE_LINE_SZ];
  volatile uint32_t numLMIds;
  char dummy3[CACHE_LINE_SZ];
  LM_ID_PATH sharedLMInfo[MAX_LOAD_MODULES];
  char dummy4[CACHE_LINE_SZ];
  volatile uint64_t counter;
  char dummy5[CACHE_LINE_SZ];
  IPC_FSInfo fsInfo;
  char dummy6[CACHE_LINE_SZ];
}IPC_SharedData;

typedef struct VAPAMap {
  // Id given by us
  void * virtualAddress;
  unsigned long physicalAddress;
  struct VAPAMap *left;
  struct VAPAMap *right;
} VAPAMap_t;

__thread VAPAMap_t * vaToPAMap = NULL;
__thread VAPAMap_t * paToVAMap = NULL;
__thread pid_t myTid = -1;
__thread bool ipcDataInited = false;
__thread IPC_FSInfo localIPCInfo;
__thread time_t lastMapChangeTime;
__thread struct stat mapsStat;
__thread uint64_t lastVMMAPCheck = 0;


IPC_SharedData * ipcSharedData;

static const char * shared_key = "/FALSE_SHARING_KEY";





void ReadIPCSharedDataTransactionally(IPC_FSInfo *ipcFSInfo){
  // Laport's STM
  do{
    int64_t startCounter = ipcSharedData->counter;
    if(startCounter & 1)
      continue; // Some writer is updating

    __sync_synchronize();
    *ipcFSInfo = ipcSharedData->fsInfo;
    __sync_synchronize();
    int64_t endCounter = ipcSharedData->counter;
    if(startCounter == endCounter)
      break;
  }while(1);
}

static void destroy_shared_memory(void * p) {
  // we should munmap, but I will not do since we dont do it in so many other places in hpcrun
  // munmap(ipc_data);
  shm_unlink((char *)shared_key);
}

static inline void create_shared_memory() {
  int fd ;
  if ( (fd = shm_open(shared_key, O_RDWR | O_CREAT, 0666)) < 0 ) {
    EEMSG("Failed to shm_open (%s), retval = %d", shared_key, fd);
    monitor_real_abort();
  }
  if ( ftruncate(fd, sizeof(IPC_SharedData)) < 0 ) {
    EEMSG("Failed to ftruncate()");
    monitor_real_abort();
  }
  void * ptr = mmap(NULL, sizeof(IPC_SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
  if(ptr == MAP_FAILED ) {
    EEMSG("Failed to mmap() IPC_SharedData");
    monitor_real_abort();
  }
  if(__sync_bool_compare_and_swap(&ipcSharedData, 0, ptr)){
    hpcrun_process_aux_cleanup_add(destroy_shared_memory, NULL);
  }

}

uint16_t GetOrCreateIPCSharedLMEntry(const char * realPath){

  if(ipcSharedData == NULL)
    create_shared_memory();
  // start from 1; leave 0 out;
  for(uint16_t i = 1 ; i < MAX_LOAD_MODULES; i++){
    switch (ipcSharedData->sharedLMInfo[i].status) {
      case STABLE:
        if(0==strncmp(realPath, ipcSharedData->sharedLMInfo[i].realPath, MAX_LM_PATH_LEN))
          return i;
        break;
      case TRANSIENT:
TRANSIENT_CASE:
        while(ipcSharedData->sharedLMInfo[i].status != STABLE) ; // spin
        if(0==strncmp(realPath, ipcSharedData->sharedLMInfo[i].realPath, MAX_LM_PATH_LEN))
          return i;
        break;
      case UNUSED:
        // Attempt to install
        if(__sync_bool_compare_and_swap(&(ipcSharedData->sharedLMInfo[i].status), UNUSED, TRANSIENT))  {
          strncpy(ipcSharedData->sharedLMInfo[i].realPath, realPath, MAX_LM_PATH_LEN);
          // need a fence on weak memory here.
          ipcSharedData->sharedLMInfo[i].status = STABLE;
          return i;
        } else {
          goto TRANSIENT_CASE;
        }
      default:
        assert(0 && "SHOULD NEVER REACH HERE");
        monitor_real_abort();
    }
  }
  assert(0 && "Should never reach here");
  monitor_real_abort();
  return 0;
}

unsigned long GetPFN(unsigned long virt_addr){
  FILE * f = fopen(PA_PATH, "rb");
  if(!f){
    printf("Error! Cannot open %s\n", PA_PATH);
    goto ErrExit;
  }

  //Shifting by virt-addr-offset number of bytes
  //and multiplying by the size of an address (the size of an entry in pagemap file)
  uint64_t file_offset = virt_addr / getpagesize() * PAGEMAP_ENTRY;
  int status = fseek(f, file_offset, SEEK_SET);
  if(status){
    perror("Failed to do fseek!");
    goto ErrExit;
  }
  errno = 0;
  uint64_t read_val = 0;
  unsigned char c_buf[PAGEMAP_ENTRY];
  for(int i=0; i < PAGEMAP_ENTRY; i++){
    unsigned char c = getc(f);
    if(c==EOF){
      //printf("\nReached end of the file\n");
      goto ErrExit;
    }
    if(is_bigendian())
      c_buf[i] = c;
    else
      c_buf[PAGEMAP_ENTRY - i - 1] = c;
  }

  fclose(f);

  for(int i=0; i < PAGEMAP_ENTRY; i++){
    //printf("%d ",c_buf[i]);
    read_val = (read_val << 8) + c_buf[i];
  }
  //
  if(GET_BIT(read_val, 63)) {
    return (unsigned long long) GET_PFN(read_val);
  }
  //    else
  //        printf("Page not present\n");
  //    if(GET_BIT(read_val, 62))
  //        printf("Page swapped\n");

  return INVALID_PHYSICAL_ADDRESS;
ErrExit:
  if(f){
    fclose(f);
  }
  return INVALID_PHYSICAL_ADDRESS;

}



static inline struct VAPAMap* splayVAtoPAMap(struct VAPAMap* root, void* vPage) {
  REGULAR_SPLAY_TREE(VAPAMap, root, vPage, virtualAddress, left, right);
  return root;
}

static inline struct VAPAMap* splayPAtoVAMap(struct VAPAMap* root, unsigned long pPage) {
  REGULAR_SPLAY_TREE(VAPAMap, root, pPage, physicalAddress, left, right);
  return root;
}


static void InsertVAtoPAMap(void * va, unsigned long pa){
  VAPAMap_t * found    = splayVAtoPAMap(vaToPAMap, va);

  // Check if a trace node with traceKey already exists under this context node
  if(found && (va == found->virtualAddress)) {
    vaToPAMap = found;
    if(found->physicalAddress != pa)
      found->physicalAddress = pa;
  } else {
    VAPAMap_t* newNode = hpcrun_malloc(sizeof(VAPAMap_t));
    newNode->virtualAddress = va;
    newNode->physicalAddress = pa;
    if(!found) {
      newNode->left = NULL;
      newNode->right = NULL;
    } else if(va < found->virtualAddress) {
      newNode->left = found->left;
      newNode->right = found;
      found->left = NULL;
    } else { // addr > addr of found
      newNode->left = found;
      newNode->right = found->right;
      found->right = NULL;
    }
    vaToPAMap = newNode;
  }
}


static void InsertPAtoVAMap(unsigned long pa, void * va){
  VAPAMap_t * found    = splayPAtoVAMap(paToVAMap, pa);

  // Check if a trace node with traceKey already exists under this context node
  if(found && (pa == found->physicalAddress)) {
    paToVAMap = found;
    if(found->virtualAddress != va)
      found->virtualAddress = va;
  } else {
    VAPAMap_t * newNode = hpcrun_malloc(sizeof(VAPAMap_t));
    newNode->virtualAddress = va;
    newNode->physicalAddress = pa;
    if(!found) {
      newNode->left = NULL;
      newNode->right = NULL;
    } else if(pa < found->physicalAddress) {
      newNode->left = found->left;
      newNode->right = found;
      found->left = NULL;
    } else { // addr > addr of found
      newNode->left = found;
      newNode->right = found->right;
      found->right = NULL;
    }
    paToVAMap = newNode;
  }
}



unsigned long GetPAfromVA(void * va){
  void * pageBaseAddr = GET_VA_PAGE(va);
  REGULAR_SPLAY_TREE(VAPAMap, vaToPAMap, pageBaseAddr, virtualAddress, left, right);
  if(vaToPAMap && (pageBaseAddr == vaToPAMap->virtualAddress)) {
    return vaToPAMap->physicalAddress;
  }
  return INVALID_PHYSICAL_ADDRESS;
}

void * GetVAfromPA(unsigned long pa){
  unsigned long  pageBaseAddr = pa;
  REGULAR_SPLAY_TREE(VAPAMap, paToVAMap, pageBaseAddr, physicalAddress, left, right);
  if(paToVAMap && (pageBaseAddr == paToVAMap->physicalAddress)) {
    return paToVAMap->virtualAddress;
  }
  return INVALID_VIRUAL_ADDRESS;
}



static void GetAllSharedPages() {
  FILE* loadmap = fopen(VA_PATH, "r");
  if (! loadmap) {
    EMSG("Could not open /proc/self/maps");
    return;
  }
  char linebuf[1024 + 1];
  char* addr = NULL;
  for(;;) {
    char* l = fgets(linebuf, sizeof(linebuf), loadmap);
    if (feof(loadmap)) break;
    char* save = NULL;
    const char delim[] = " \n";
    addr = strtok_r(l, delim, &save);
    char* perms = strtok_r(NULL, delim, &save);
    // if write is not allowed skip
    if (perms[1] == '-') {
      continue;
    }
    if (perms[3] == 's') {
      const char dash[] = "-";
      char* start_str = strtok_r(addr, dash, &save);
      char* end_str   = strtok_r(NULL, dash, &save);
      void *start = (void*) (uintptr_t) strtol(start_str, NULL, 16);
      void *end   = (void*) (uintptr_t) strtol(end_str, NULL, 16);
      const long pgSz = sysconf(_SC_PAGESIZE);
      for(void *va = start; va < end; va += pgSz) {
        // Get PA for each VA page
        unsigned long pa =  (unsigned long) GetPFN((unsigned long)va);
        if(pa != INVALID_PHYSICAL_ADDRESS) {
          //                    printf("\n VA:%xl = %xl", va, pa);
          InsertVAtoPAMap(va, pa);
          InsertPAtoVAMap(pa, va);
        }
      }
    }
  }
  fclose(loadmap);
}

static void UpdateVMMap(){
  int s = stat(VA_PATH, &mapsStat);
  if(s != 0){
    fprintf(stderr, "\n Failed to STAT %s", VA_PATH);
  }

  if( ((lastVMMAPCheck % VM_MAP_CHECK_FREQUENCY) == 0)
      && (lastMapChangeTime != mapsStat.st_mtime)) {
    // New mapping
    GetAllSharedPages();
  }
  lastMapChangeTime = mapsStat.st_mtime;
  lastVMMAPCheck++;
}

static void HandleIPCFalseSharing(void * data_addr, void * pc, cct_node_t *node, int accessLen, AccessType accessType, int sampledMetricId, bool isSamplePointAccurate){
  if(ipcSharedData == NULL){
    create_shared_memory();
  }
  if (ipcDataInited == false) {
    myTid = syscall(SYS_gettid);
    lastVMMAPCheck = 0;
    ipcDataInited = true;
  }
  // is address on shared page?
  unsigned long pa = GetPAfromVA(data_addr);
  // Ok, on a shared page!
  // Ok to publish new data?

  // Is the published address old enough (stayed for > 1 sample time span)
  int64_t curTime = rdtsc();
  volatile IPC_FSInfo * globalIPCInfo = &(ipcSharedData->fsInfo);

  pid_t me = myTid;
  // Get the time, tid, and counter
  // This is definately racy but benign.
  uint64_t theCounter = ipcSharedData->counter;
  if( ((curTime-globalIPCInfo->time) > 2 * (curTime-lastTime)) // Sufficient time passed since the last time somebody published
      &&
      ( (theCounter & 1) == 0) // Nobody is in the process of publishing
      && (pa != INVALID_PHYSICAL_ADDRESS) // my PA is a valid address
    ) {
    // Attempt to lockout

    if(__sync_bool_compare_and_swap(&(ipcSharedData->counter), theCounter, theCounter+1)){
    } else {
      // Failed to update ==> someone else succeeded ==> Fetch that address and set a WP for that
      goto SET_FS_WP;
    }


    globalIPCInfo->time = rdtsc();
    globalIPCInfo->tid = myTid;
    globalIPCInfo->wpType = accessType == LOAD ? WP_WRITE : WP_RW;
    globalIPCInfo->accessType = accessType;
    globalIPCInfo->address = pa;
    globalIPCInfo->offset = PAGE_OFFSET(data_addr);
    globalIPCInfo->accessLen = accessLen;

    int btLen = 0;
    for(; btLen < MAX_BACKTRACE_LEN - 1; btLen++){
      if (node == NULL)
        break;
      globalIPCInfo->backtrace[btLen] = *hpcrun_cct_addr(node);
      node = hpcrun_cct_parent(node);
    }

    // unlikely; if btLen == MAX_BACKTRACE_LEN; drop the WP by invalidating it
    if (btLen == MAX_BACKTRACE_LEN -1 ) {
      globalIPCInfo->tid = -1;
    } else {
      globalIPCInfo->btLength = btLen;
      // Set the last entry null
      globalIPCInfo->backtrace[btLen].ip_norm.lm_id = 0;
      globalIPCInfo->backtrace[btLen].ip_norm.lm_ip = 0;
    }
    __sync_synchronize();
    ipcSharedData->counter = theCounter + 2; // makes the counter even
  } else if ((globalIPCInfo->tid != me)  && (globalIPCInfo->tid != -1)/* dont set WP for my own accessed locations */){
    // If the data is "new" set the WP
SET_FS_WP: ReadIPCSharedDataTransactionally(&localIPCInfo);
           // Get the VA from PA
           void * va = GetVAfromPA (localIPCInfo.address);
           if(va == INVALID_VIRUAL_ADDRESS) {
             goto ErrExit;
           }

           va = va + localIPCInfo.offset;

           long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
           accessedIns += metricThreshold;

           switch (theWPConfig->id) {
             case WP_IPC_TRUE_SHARING:{
                                        // Set WP at the same address
                                        SampleData_t sd= {
                                          .va = va,
                                          .node = localIPCInfo.backtrace,
                                          .accessType=localIPCInfo.accessType,
                                          .type=localIPCInfo.wpType,
                                          .wpLength = GetFloorWPLengthAtAddress(va, accessLen),
                                          .accessLength= accessLen,
                                          .sampledMetricId=sampledMetricId,
                                          .isSamplePointAccurate = isSamplePointAccurate,
                                          .preWPAction=theWPConfig->preWPAction,
                                          .isBackTrace = true
                                        };
                                        SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                                      }
                                      break;
             case WP_IPC_FALSE_SHARING: {
                                          int wpSizes[] = {8, 4, 2, 1};
                                          FalseSharingLocs falseSharingLocs[CACHE_LINE_SZ];
                                          int numFSLocs = 0;
                                          GetAllFalseSharingLocations((size_t) va, accessLen, ALIGN_TO_CACHE_LINE((size_t)va), CACHE_LINE_SZ, wpSizes, 0 /*curWPSizeIdx*/ , 4 /*totalWPSizes*/, falseSharingLocs, &numFSLocs);
                                          // Find 4 slots in the cacheline
                                          for(int i = 0; i < numFSLocs/2; i ++) {
                                            int idx = rdtsc() % numFSLocs;
                                            FalseSharingLocs tmpVal = falseSharingLocs[idx];
                                            falseSharingLocs[idx] = falseSharingLocs[i];
                                            falseSharingLocs[i] = tmpVal;
                                          }
                                          for(int i = 0; i < MIN(numFSLocs, wpConfig.maxWP); i ++) {
                                            SampleData_t sd= {
                                              .va = (void *) falseSharingLocs[i].va,
                                              .node = localIPCInfo.backtrace,
                                              .accessType=localIPCInfo.accessType,
                                              .type=localIPCInfo.wpType,
                                              .wpLength = falseSharingLocs[i].wpLen,
                                              .accessLength= accessLen,
                                              .sampledMetricId=sampledMetricId,
                                              .isSamplePointAccurate = isSamplePointAccurate,
                                              .preWPAction=theWPConfig->preWPAction,
                                              .isBackTrace = true
                                            };
                                            SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                                          }
                                        }
                                        break;
             case WP_IPC_ALL_SHARING: {
                                        assert(0);
                                      }
                                      break;
             default:
                                      break;
           }
  }else{
    /* dont set WP for my own accessed locations */
  }
ErrExit:
  lastTime = rdtsc();
}

#if 0
bool PrintStats(){
  extern get_access_type(void *);
  void *  contextIP = hpcrun_context_pc(context);
  int v1 = get_access_type(mmap_data->ip);
  int v2 = get_access_type(contextIP);

  switch(v1){
    case 0: unk1++; break;
    case 1: ld1++; break;
    case 2: st1++; break;
    case 3: mix1++; break;
    default: break;
  }
  switch(v2){
    case 0: unk2++; break;
    case 1: ld2++; break;
    case 2: st2++; break;
    case 3: mix2++; break;
    default: break;
  }

  float tot = unk1 + ld1 + st1 + mix1;
  fprintf(stderr, "W=%f (%f), L=%f(%f), M=%f(%f), U=%f(%f)\n", st1/tot, st2/tot, ld1/tot, ld2/tot, mix1/tot, mix2/tot, unk1/tot, unk2/tot);
  /*
     if( (mmap_data->ip > contextIP) || (contextIP-mmap_data->ip >15)) {
     incorrect++;
     fprintf(stderr, "BAD IP: contextIP=%p, precieIP=%p, data=%p %f, get_access_type=%d\n", contextIP, mmap_data->ip, mmap_data->addr, 1.0*(incorrect)/(incorrect+correct), get_access_type(mmap_data->ip));
     }  else {
     correct++;
     }
   */
  void *  contextIP = hpcrun_context_pc(context);
  extern int is_same_function(void *ins1, void* ins2);
  int samev1 = is_same_function(contextIP, mmap_data->ip);

  switch(samev1){
    case 0: difffunc++; break;
    case 1: samefunc++; break;
    case 2: unknwfunc++; break;
    default: break;
  }
  double tot = difffunc + samefunc + unknwfunc;
  if (mmap_data->ip==contextIP)
    ipSame ++;
  else
    ipDiff ++;
  fprintf(stderr, "Diff=%f, Same=%f, Unkn=%f, Ctxt=%p, %p, Same =%f\n", difffunc/tot, samefunc/tot, unknwfunc/tot, contextIP, mmap_data->ip, 1.0 * ipSame/ (ipSame+ipDiff));
}
#endif

SharedEntry_t getEntryRandomlyFromBulletinBoard(int tid, uint64_t cur_time, int * do_not_arm_watchpoint) {
  int hashIndex = rdtsc() % HASHTABLESIZE;
  int iter = 0;
  while(1) {
    if(iter == HASHTABLESIZE) {
      *do_not_arm_watchpoint = 1;
      break;
    }
    if((bulletinBoard.hashTable[hashIndex].cacheLineBaseAddress != -1) && (bulletinBoard.hashTable[hashIndex].tid != tid) && ((cur_time - bulletinBoard.hashTable[hashIndex].time) <= bulletinBoard.hashTable[hashIndex].expiration_period))
      break;
    ++hashIndex;
    hashIndex %= HASHTABLESIZE;
    iter++;
  }
  return bulletinBoard.hashTable[hashIndex];
}

SharedEntry_t getEntryFromBulletinBoard(void * cacheLineBaseAddress, int * item_not_found) {
  int hashIndex = hashCode(cacheLineBaseAddress);
  if(cacheLineBaseAddress != bulletinBoard.hashTable[hashIndex].cacheLineBaseAddress)
    *item_not_found = 1;
  return bulletinBoard.hashTable[hashIndex];
}


void hashInsertwithTime(struct SharedEntry item, uint64_t cur_time, uint64_t prev_time) {
  void * cacheLineBaseAddress = item.cacheLineBaseAddress;
  int hashIndex = hashCode(cacheLineBaseAddress);

  if ((bulletinBoard.hashTable[hashIndex].cacheLineBaseAddress == -1) || (item.tid != bulletinBoard.hashTable[hashIndex].tid) || ((item.time - bulletinBoard.hashTable[hashIndex].time) > (cur_time - prev_time))) {
    bulletinBoard.hashTable[hashIndex] = item;
  }
}

/*
   double thread_coefficient(int as_matrix_size) {
   double thread_count = (double) as_matrix_size + 1;
   return 2.31 * pow(thread_count, -0.869);
   }*/

#if 0
double thread_coefficient(int as_matrix_size) {
  double thread_count = (double) as_matrix_size + 1;
  return 50.6 * pow(thread_count, -0.434);
}
#endif

int ibs_get_mem_width(int mem_width) {
        if(mem_width < 3)
                return mem_width;
        else if(mem_width == 3)
                return 4;
        else if(mem_width == 4)
                return 8;
        else if(mem_width == 5)
                return 16;
        return 1;
}

bool OnSample(perf_mmap_data_t * mmap_data, /*void * contextPC*/void * context, cct_node_t *node, int sampledMetricId) {
  if (strncmp (hpcrun_id2metric(sampledMetricId)->name,"L2_RQSTS.MISS", 13) == 0)
    fprintf(stderr, "there is an L2_RQSTS.MISS 1\n"); 
  //fprintf(stderr, "in OnSample\n");
  void * contextPC = hpcrun_context_pc(context); 
  void * data_addr = mmap_data->addr; 
  void * precisePC = (amd_ibs_flag || (mmap_data->header_misc & PERF_RECORD_MISC_EXACT_IP)) ? mmap_data->ip : 0;
  // Filert out address and PC (0 or kernel address will not pass)
  //fprintf(stderr, "OnSample is called %lx\n", data_addr);
  if (strncmp (hpcrun_id2metric(sampledMetricId)->name,"L2_RQSTS.MISS", 13) == 0)
    fprintf(stderr, "there is an L2_RQSTS.MISS\n");
//#if 0
  if (amd_ibs_flag /*&& mmap_data->store*/) {
        //valid_sample_count++;
        valid_sample_count1++;
  }
//#endif
  if (!IsValidAddress(data_addr, precisePC)) { 
    goto ErrExit; // incorrect access type
  }

  if (amd_ibs_flag /*&& mmap_data->store*/) {
        //valid_sample_count++;
        valid_sample_count2++;
  }
//#endif
  //fprintf(stderr, "no problem 1\n");
#if 0
  if (!amd_ibs_flag  && node == NULL) {
    goto ErrExit; // incorrect CCT
  }
#endif

  if (node == NULL) {
    goto ErrExit; // incorrect CCT
  } 

  //fprintf(stderr, "no problem 2\n");

  uint64_t curTime = rdtsc();
  int accessLen = 1;
  AccessType accessType;
  if(amd_ibs_flag) {
	  //fprintf(stderr, "looking for precisePC: %lx in getEntryFromAccessTypeLengthCache\n", precisePC);
	if(mmap_data->store)
        	accessType = STORE;
        else if (mmap_data->load)
                accessType = LOAD;
	accessLen = ibs_get_mem_width(mmap_data->mem_width);
	//fprintf(stderr, "mem_width: %d, accessLen: %d\n", mmap_data->mem_width, accessLen); 		
  }
  else if(false == get_mem_access_length_and_type(precisePC, (uint32_t*)(&accessLen), &accessType)){
    //EMSG("Sampled a non load store at = %p\n", precisePC);
    goto ErrExit; // incorrect access type
  }
  //fprintf(stderr, "in OnSample, sampled address: %lx, disassembled address: %lx, mmap_data->addr_valid: %d\n", data_addr, addr1, mmap_data->addr_valid);
  if(!amd_ibs_flag && (accessType == UNKNOWN || accessLen == 0)){
    //EMSG("Sampled sd.accessType = %d, accessLen=%d at precisePC = %p\n", accessType, accessLen, precisePC);
    goto ErrExit; // incorrect access type
  }
  //fprintf(stderr, "no problem 4\n");

  //fprintf(stderr, "A sample is handled in OnSample\n");
  // if the context PC and precise PC are not in the same function, then the sample point is inaccurate.
  bool isSamplePointAccurate;
  FunctionType ft = is_same_function(contextPC, precisePC);
  if (ft == SAME_FN) {
    isSamplePointAccurate = true;
  } else {
    isSamplePointAccurate = false;
  }

  //fprintf(stderr, "no problem 5\n");
  switch (theWPConfig->id) {
    case WP_DEADSPY:{
                      if(accessType == LOAD){
                        //EMSG("Sampled accessType = %d\n", accessType);
                        goto ErrExit; // incorrect access type
                      }

                      long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                      writtenBytes += accessLen * metricThreshold;
                      SampleData_t sd= {
                        .va = data_addr,
                        .node = node,
                        .type=WP_RW,
                        .wpLength = accessLen,
                        .accessLength= accessLen,
                        .accessType=accessType,
                        .sampledMetricId=sampledMetricId,
                        .isSamplePointAccurate = isSamplePointAccurate,
                        .preWPAction=theWPConfig->preWPAction,
                        .isBackTrace = false
                      };
                      sd.wpLength = GetFloorWPLength(accessLen);
                      SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                    }
                    break;

    case WP_REDSPY:{
                     // If we got an insane address that cannot be read, return silently
                     if(!IsAddressReadable(data_addr)){
                       goto ErrExit;
                     }

                     long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                     writtenBytes += accessLen * metricThreshold;
                     SampleData_t sd= {
                       .va = data_addr,
                       .node = node,
                       .type=WP_WRITE,
                       .wpLength = accessLen,
                       .accessLength= accessLen,
                       .accessType=accessType,
                       .sampledMetricId=sampledMetricId,
                       .isSamplePointAccurate = isSamplePointAccurate,
                       .preWPAction=theWPConfig->preWPAction,
                       .isBackTrace = false
                     };
                     // Must have a store address
                     if(accessType == STORE || sd.accessType == LOAD_AND_STORE){
                       sd.wpLength = GetFloorWPLength(accessLen);
                       SubscribeWatchpoint(&sd, OVERWRITE, true /* capture value */);
                     } else {
                       //EMSG("Sampled accessType = %d\n", accessType);
                       goto ErrExit; // incorrect access type
                     }
                   }
                   break;
    case WP_LOADSPY:{
                      // If we got an insane address that cannot be read, return silently
                      if(!IsAddressReadable(data_addr)){
                        goto ErrExit;
                      }

                      long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                      loadedBytes += accessLen * metricThreshold;
                      // we use WP_RW because we cannot set WP_READ alone
                      SampleData_t sd= {
                        .va = data_addr,
                        .node = node,
                        .type=WP_RW,
                        .wpLength = accessLen,
                        .accessLength= accessLen,
                        .accessType=accessType,
                        .sampledMetricId=sampledMetricId,
                        .isSamplePointAccurate = isSamplePointAccurate,
                        .preWPAction=theWPConfig->preWPAction,
                        .isBackTrace = false
                      };
                      // Must have a store address
                      if(accessType == LOAD || sd.accessType == LOAD_AND_STORE){
                        sd.wpLength = GetFloorWPLength(accessLen);
                        SubscribeWatchpoint(&sd, OVERWRITE, true /* capture value */);
                      } else {
                        //EMSG("Sampled accessType = %d\n", accessType);
                        goto ErrExit; // incorrect access type
                      }
                    }
                    break;
    case WP_REUSE: {
                     //fprintf(stderr, "WP_REUSE in OnSample\n");
#ifdef REUSE_HISTO
#else
                     if ( accessType != reuse_monitor_type && reuse_monitor_type != LOAD_AND_STORE) break;
#endif
                     long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                     accessedIns += metricThreshold;
                     SampleData_t sd= {
                       .node = node,
                       .type=WP_RW,  //jqswang: Setting it to WP_READ causes segment fault
                       .accessType=accessType,
                       //.wpLength = accessLen, // set later
                       .accessLength= accessLen,
                       .sampledMetricId=sampledMetricId,
                       .isSamplePointAccurate = isSamplePointAccurate,
                       .preWPAction=theWPConfig->preWPAction,
                       .isBackTrace = false,
                     };
#ifdef REUSE_HISTO
                     sd.wpLength = 1;
#else
                     sd.wpLength = GetFloorWPLength(accessLen);
                     sd.type = WP_RW;//reuse_trap_type;
                     //fprintf(stderr, "here1\n");
#endif
                     bool isProfileSpatial;
                     if (reuse_profile_type == REUSE_TEMPORAL){
                       isProfileSpatial = false;
                     } else if (reuse_profile_type == REUSE_SPATIAL){
                       isProfileSpatial = true;
                     } else {
                       //fprintf(stderr, "50 50\n");
                       isProfileSpatial = (rdtsc() & 1);
                     }

                     //fprintf(stderr, "here2 data_addr: %lx\n", (uint64_t) data_addr);
                     if (isProfileSpatial) {// detect spatial reuse
                       int wpSizes[] = {8, 4, 2, 1};
                       FalseSharingLocs falseSharingLocs[CACHE_LINE_SZ];
                       int numFSLocs = 0;
                       GetAllFalseSharingLocations((size_t)data_addr, accessLen, ALIGN_TO_CACHE_LINE((size_t)(data_addr)), CACHE_LINE_SZ, wpSizes, 0 /*curWPSizeIdx*/ , 4 /*totalWPSizes*/, falseSharingLocs, &numFSLocs);
                       if (numFSLocs == 0) { // No location is found. It is probably due to the access length already occupies one cache line. So we just monitor the temporal reuse instead.
                         sd.va = data_addr;
                         sd.reuseType = REUSE_TEMPORAL;
                         //fprintf(stderr, "REUSE_TEMPORAL is activated\n");
                       } else {
                         int idx = rdtsc() % numFSLocs; //randomly choose one location to monitor
                         sd.va = (void *)falseSharingLocs[idx].va;
                         sd.reuseType = REUSE_SPATIAL;
                         //fprintf(stderr, "REUSE_SPATIAL is activated\n");
#if 0
                         int offset = ((uint64_t)data_addr - aligned_pc) / accessLen;
                         int bound = CACHE_LINE_SZ / accessLen;
                         int r = rdtsc() % bound;
                         if (r == offset) r = (r+1) % bound;
                         sd.va = aligned_pc + (r * accessLen);
#endif
                       }
                     } else {
                       sd.va = data_addr;
                       sd.reuseType = REUSE_TEMPORAL;
                       //fprintf/(stderr, "REUSE_TEMPORAL is activated\n");
                     }
                     //fprintf(stderr, "here3\n");
                     if (!IsValidAddress(sd.va, precisePC)) {
                       goto ErrExit; // incorrect access type
                     }

                     //fprintf(stderr, "here4\n");
                     // Read the reuse distance event counters
                     // We assume the reading event is load, store or both.
                     for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
                       uint64_t val[3];
                       //fprintf(stderr, "before assert\n");
                       assert(linux_perf_read_event_counter( reuse_distance_events[i], val) >= 0);
                       //fprintf(stderr, "after assert\n");
                       //fprintf(stderr, "USE %lu %lu %lu  -- ", val[0], val[1], val[2]);
                       //fprintf(stderr, "USE %lx -- ", val[0]);
                       memcpy(sd.reuseDistance[i], val, sizeof(uint64_t)*3);;
                     }
                     //fprintf(stderr, "here5\n");
                     //fprintf(stderr, "\n");
                     // register the watchpoint
                     //fprintf(stderr, "watchpoints are about to be armed from OnSample\n");
                     SubscribeWatchpoint(&sd, OVERWRITE, false );
                     //fprintf(stderr, "here6\n");

                   }
                   break;
    case WP_REUSETRACKER: {	
                            sample_count++;
                            int me = TD_GET(core_profile_trace_data.id);

                            int my_core = sched_getcpu();
                            /*if((strncmp (hpcrun_id2metric(sampledMetricId)->name,"MEM_LOAD_RETIRED.L2_MISS",24) == 0))
                              fprintf(stderr, "l2 miss sample happens1\n");*/	
                            //fprintf(stderr, "thread %d is sampled in core %d\n", me, sched_getcpu());
                            //fprintf(stderr, "sample type: %s in thread %d, sample_count: %d\n", hpcrun_id2metric(sampledMetricId)->name, TD_GET(core_profile_trace_data.id), sample_count);	
                            int64_t storeCurTime = 0;
                            if(accessType == STORE || accessType == LOAD_AND_STORE) {
                              storeCurTime = curTime;
                              storeOlderTime = storeLastTime;
                              storeLastTime = storeCurTime;
                            }

#ifdef REUSE_HISTO
#else
                            if ( accessType != reuse_monitor_type && reuse_monitor_type != LOAD_AND_STORE) break;
#endif
                            long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                            accessedIns += metricThreshold;
                            SampleData_t sd= {
                              .node = node,
                              //.type=WP_RW,  //jqswang: Setting it to WP_READ causes segment fault
                              .accessType=accessType,
                              //.wpLength = accessLen, // set later
                              .accessLength= accessLen,
                              .sampledMetricId=sampledMetricId,
                              .isSamplePointAccurate = isSamplePointAccurate,
                              .preWPAction=theWPConfig->preWPAction,
                              .first_accessing_tid =me,
                              .isBackTrace = false,
                              .L3StoreUse = false
                            };
#ifdef REUSE_HISTO
                            //fprintf(stderr, "WP_REUSETRACKER in OnSample\n");
                            sd.wpLength = 1;
#else
                            sd.wpLength = GetFloorWPLength(accessLen);
                            sd.type = WP_RW;//reuse_trap_type;
                            //fprintf(stderr, "here1\n");
#endif
                            if (reuse_profile_type == REUSE_SPATIAL){
                              sd.reuseType = REUSE_SPATIAL;
                            } else {
                              sd.reuseType = REUSE_TEMPORAL;
                            }

                            //fprintf(stderr, "here3\n");
                            if (!IsValidAddress(data_addr, precisePC)) {
                              goto ErrExit; // incorrect access type
                            }

                            /*if((strncmp (hpcrun_id2metric(sampledMetricId)->name,"MEM_LOAD_RETIRED.L2_MISS",24) == 0))
                              fprintf(stderr, "l2 miss sample happens2\n");*/

                            //fprintf(stderr, "sample type: %s\n", hpcrun_id2metric(sampledMetricId)->name);
                            //fprintf(stderr, "here4\n");
                            // Read the reuse distance event counters
                            // We assume the reading event is load, store or both.

                            int sType = -1;

                            if (strncmp (hpcrun_id2metric(sampledMetricId)->name,"MEM_UOPS_RETIRED:ALL_STORES",27) == 0) {
                              //fprintf(stderr, "store sample is detected\n");
                              sType = ALL_STORE;
                            }
                            else if(strncmp (hpcrun_id2metric(sampledMetricId)->name,"MEM_UOPS_RETIRED:ALL_LOADS",26) == 0) {
                              //fprintf(stderr, "load sample is detected\n");
                              sType = ALL_LOAD;
                            }
                            //fprintf(stderr, "threads are selected here\n");
                            //fprintf(stderr, "sampledMetricId: %d\n", sampledMetricId);
                            prev_sample_timestamp = last_sample_timestamp;
                            last_sample_timestamp = curTime;
                            int location = -1;
                            bool steal_wp_slot = false; 
                            if(used_wp_count < MIN(global_thread_count, wpConfig.maxWP)) {
                              uint64_t theCounter = globalReuseWPs.counter;
                              if((theCounter & 1) == 0) {
                                if(__sync_bool_compare_and_swap(&globalReuseWPs.counter, theCounter, theCounter+1)) {
                                  // before
                                  for(int j = 0; j < wpConfig.maxWP; j++) {
                                    if(me == globalWPIsUsers[j]) {
                                      location = j;
                                      break;
                                    }
                                  }
                                  if (location == -1) {
                                    for(int j = 0; j < wpConfig.maxWP; j++) {
                                      if(globalWPIsUsers[j] == -1) {
                                        used_wp_count++;
                                        location = j;
                                        steal_wp_slot = true;
                                        globalWPIsUsers[j] = me;
                                        break; 
                                      }
                                    }
                                  }
                                  // after       	
                                  globalReuseWPs.counter++;
                                }
                              } 
                            }

                            if(steal_wp_slot) {

                              if(max_used_wp_count < used_wp_count)
                                max_used_wp_count = used_wp_count;
                              globalReuseWPs.table[location].tid = me;
                              wait_threshold = sample_count + CHANGE_THRESHOLD;
                              // some code 
                            } 

                            int item_not_found_flag = 0;
                            sd.sampleTime=curTime;

                            int monitored_location = -1;

                            for(int j = 0; j < used_wp_count; j++) {
                              if(me == globalReuseWPs.table[j].monitored_tid) {
                                monitored_location = j;
                                break;
                              }
                            }

                            if(monitored_location != -1 && (profiling_mode == L3 || profiling_mode == MIXED)) {
                              if(!globalReuseWPs.table[monitored_location].sharedActive && globalReuseWPs.table[monitored_location].self_trap && (globalReuseWPs.table[monitored_location].rd > 0)) {
                                WatchPointInfo_t * wpi = getWPI(me, monitored_location);
                                if(wpi && wpi->sample.node) {
                                  double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
                                  uint64_t numDiffSamples = GetWeightedMetricDiff(wpi->sample.node, wpi->sample.sampledMetricId, myProportion); //hpcrun_id2metric(sampledMetricId)->period;
                                  double inc_scale = dynamic_global_thread_count / (double) max_used_wp_count;
                                  uint64_t inc = numDiffSamples * inc_scale;
                                  bool comm_reuse = false;
                                  //uint64_t theCounter = shared_reuse_counter;
                                  //if((theCounter & 1) == 0) {
                                  //if(__sync_bool_compare_and_swap(&shared_reuse_counter, theCounter, theCounter+1)) {
                                  SharedReuseAddDistance(globalReuseWPs.table[monitored_location].rd, inc);
                                  if(globalReuseWPs.table[monitored_location].is_rar == false) {
                                    CommunicationReuseAddDistance(globalReuseWPs.table[monitored_location].rd, inc);
                                    comm_reuse = true;
                                  }
                                  if(profiling_mode == L3)
                                    numWatchpointArmingAttempt[monitored_location] = SAMPLES_POST_FULL_RESET_VAL;
                                  cct_metric_data_increment(l3_reuse_memory_distance_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = (globalReuseWPs.table[monitored_location].rd) });
                                  //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
                                  cct_metric_data_increment(l3_reuse_memory_distance_count_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = 1});
                                  if(comm_reuse) {
                                    cct_metric_data_increment(comm_reuse_memory_distance_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = (globalReuseWPs.table[monitored_location].rd) });
                                    //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
                                    cct_metric_data_increment(comm_reuse_memory_distance_count_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = 1});
                                  }
                                  if (reuse_profile_type == REUSE_TEMPORAL) {
                                    cct_metric_data_increment(l3_temporal_reuse_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = inc});
                                    if(comm_reuse) {
                                      cct_metric_data_increment(comm_temporal_reuse_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = inc});
                                    }
                                  } else if (reuse_profile_type == REUSE_SPATIAL) {
                                    cct_metric_data_increment(l3_spatial_reuse_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = inc});
                                    if(comm_reuse) {
                                      cct_metric_data_increment(comm_spatial_reuse_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = inc});
                                    }	
                                  }
                                  //fprintf(stderr, "histogram is updated by %ld, rd: %ld, me: %d, monitored_tid: %d\n", inc, globalReuseWPs.table[monitored_location].rd, me, globalReuseWPs.table[monitored_location].monitored_tid);
                                  globalReuseWPs.table[monitored_location].rd = 0;
                                  //shared_reuse_counter++;
                                  //}
                                  //}
                                }
                                // update shared reuse histogram
                              }
                            } 

                            location = -1;

                            for(int j = 0; j < used_wp_count; j++) {
                              if(me == globalWPIsUsers[j]) {
                                location = j;
                                break;
                              }
                            }

                            if(location ==  -1) {
                              uint64_t timestamp = MonitoredNode.timestamp;
                              if((last_monitored_wp_timestamp == timestamp) && (last_trapped_timestamp != timestamp) && (MonitoredNode.self_trap == false)) {
                                //reset context scaling count in monitored_node
                                //fprintf(stderr, "inc is reset because of approximation, location: %d, last_monitored_wp_timestamp: %ld, timestamp: %ld, last_trapped_timestamp: %ld, self_trap: %d\n", location, last_monitored_wp_timestamp, timestamp, last_trapped_timestamp, MonitoredNode.self_trap);
                                GetWeightedMetricDiffAndReset(monitored_node, monitored_metric, 1.0);
                                last_trapped_timestamp = timestamp;
                              }
                              else if(last_monitored_wp_timestamp != timestamp) {
                                //fprintf(stderr, "monitored_node is replaced, location: %d, last_monitored_wp_timestamp: %ld, timestamp: %ld\n", location, last_monitored_wp_timestamp, timestamp);
                                monitored_node = node;
                                monitored_metric = MonitoredNode.metricId;
                                last_monitored_wp_timestamp = timestamp;	
                              }
                            }	      
 
                            if ((location != -1) && (sample_count > wait_threshold)) {
                              globalWPIsUsers[location] = -1;
                              globalReuseWPs.table[location].tid = -1; 
                              //globalReuseWPs.table[location].residueSampleCountInPrevThread = GetWeightedMetricDiff(node, sampledMetricId, 1.0);
                              //fprintf(stderr, "residueSampleCountInPrevThread is %ld when thread %d is releasing a global WP\n", globalReuseWPs.table[location].residueSampleCountInPrevThread, me);
                              if(MonitoredNode.tid == me)
                                MonitoredNode.tid = -1;
                              location = -1;
                              used_wp_count--;                                                                     
                            }


                            //fprintf(stderr, "location: %d, thread: %d\n", location, me);
                            if((location != -1) && ArmWatchPointProb(&location, curTime, me)) {

                              uint64_t theCounter = MonitoredNode.counter;
                              if((theCounter & 1) == 0) {
                                if(__sync_bool_compare_and_swap(&MonitoredNode.counter, theCounter, theCounter+1)) {

                                  if((MonitoredNode.tid == -1) || ((MonitoredNode.self_trap == true) && (MonitoredNode.tid == me)) || ((MonitoredNode.self_trap == false) && ((curTime - MonitoredNode.trap_timestamp) >= (2 * (curTime - lastTime))))) {
                                    //fprintf(stderr, "published monitored node is changed, MonitoredNode.tid: %d, self trap: %d, me: %d, time_gap: %ld, expiry period: %ld\n", MonitoredNode.tid, MonitoredNode.self_trap, me, (curTime - MonitoredNode.trap_timestamp), (2 * (curTime - lastTime)));
                                    MonitoredNode.timestamp = curTime;
                                    MonitoredNode.trap_timestamp = 0;
                                    MonitoredNode.metricId = sampledMetricId;
                                    MonitoredNode.self_trap = true;
                                    MonitoredNode.tid = me;
                                  }
                                  MonitoredNode.counter++;
                                }
                              }	

                              // before
                              int affinity_l3;
                              int affinity_l2;
                              if(l2_count > 1) {
                                affinity_l3 = thread_to_l3_mapping[my_core]; 
                                sd.L3Id = affinity_l3;
                                affinity_l2 = thread_to_l2_mapping[my_core];
                                sd.L2Id = affinity_l2;
                              }

                              for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
                                for(int j=0; j < 3; j++) {
                                  sd.sharedReuseDistance[i][j] = 0;
                                }
                              }

                              int cur_global_thread_count = global_thread_count;
                              if(profiling_mode == L3 || profiling_mode == MIXED) {
                                for(int i = 0; i < cur_global_thread_count; i++) {
                                  if((mapping_size == 0) || (l3_count == 1) || (thread_to_l3_mapping[mapping_vector[i % mapping_size]] == affinity_l3)) {
                                    for (int j=0; j < MIN(2, reuse_distance_num_events); j++){
                                      uint64_t val[3];  
                                      linux_perf_read_event_counter_shared( reuse_distance_events[j], val, i/*locality_vector[affinity_l3][i+1]*/);
                                      for(int k=0; k < 3; k++) {
                                        sd.sharedReuseDistance[j][k] += val[k];
                                      }
                                    }
                                  }
                                }
                              }


                              // after
                              int indices[cur_global_thread_count];
                              for (int i = 0; i < cur_global_thread_count; i++) {
                                indices[i] = i;
                              }

                              int wp_index = cur_global_thread_count;
                              while (wp_index) { 
                                int index = rdtsc() % wp_index;
                                wp_index--;
                                int swap = indices[index];
                                indices[index] = indices[wp_index];
                                indices[wp_index] = swap;
                              }


                              //uint64_t pmu_counter = 0;
                              sd.L1Sample = true;
                              for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
                                uint64_t val[3];
                                if(((sType == ALL_LOAD) && (i == 0)) || ((sType == ALL_STORE) && (i == 1)))
                                  assert(linux_perf_read_event_counter_l1( reuse_distance_events[i], val, true) >= 0);
                                else
                                  assert(linux_perf_read_event_counter_l1( reuse_distance_events[i], val, false) >= 0);
                                //fprintf(stderr, "val[0] in sample %d: %ld\n", i, val[0]);
                                //val[1] = 0;
                                memcpy(sd.reuseDistance[i], val, sizeof(uint64_t)*3);
                                //pmu_counter += val[0];
                              }	

                              FalseSharingLocs falseSharingLocs[CACHE_LINE_SZ];
                              int numFSLocs = 0;
                              if (reuse_profile_type == REUSE_SPATIAL){
                                int wpSizes[] = {8, 4, 2, 1};
                                GetAllFalseSharingLocations((size_t)data_addr, accessLen, ALIGN_TO_CACHE_LINE((size_t)(data_addr)), CACHE_LINE_SZ, wpSizes, 0 /*curWPSizeIdx*/ , 4 /*totalWPSizes*/, falseSharingLocs, &numFSLocs);
                              }

                              //void * original_va = sd.va;
                              int original_wpLength = sd.wpLength;
                              for(int i = 0; i < cur_global_thread_count; i++) { 
                                int core_id;
                                if(mapping_size > 0) {
                                  core_id = mapping_vector[indices[i] % mapping_size];	
                                }
                                if((mapping_size == 0) || (thread_to_l3_mapping[core_id] == affinity_l3)) {
                                  //fprintf(stderr, "a wp in thread %d is armed by thread %d to detect reuse\n", indices[i], me);
                                  if(indices[i] == me || profiling_mode == L3 || profiling_mode == MIXED) { 
                                    if (reuse_profile_type == REUSE_SPATIAL){ 
                                      if (numFSLocs == 0) { // No location is found. It is probably due to the access length already occupies one cache line. So we just monitor the temporal reuse instead.
                                        sd.va = data_addr;
                                        sd.reuseType = REUSE_TEMPORAL;
                                        sd.wpLength = 1;
                                        //fprintf(stderr, "REUSE_TEMPORAL is activated\n");
                                      } else {
                                        //fprintf(stderr, "false sharing is searched\n");
                                        int idx = rdtsc() % numFSLocs; //randomly choose one location to monitor
                                        sd.va = (void *)falseSharingLocs[idx].va;
                                        sd.wpLength = falseSharingLocs[idx].wpLen;
                                        sd.reuseType = REUSE_SPATIAL;
                                      }
                                    } else {

                                      sd.reuseType = REUSE_TEMPORAL;
                                      sd.va = data_addr;
                                      sd.wpLength = 1;
                                      //fprintf(stderr, "sample tries to detect temporal reuse in L3\n");
                                    }
                                    if(indices[i] == me || profiling_mode == L3) {
                                      sd.type = WP_RW;
                                    } else if (profiling_mode == MIXED) {
                                      if((rdtsc() % 100) >= 50) {
                                        sd.type = WP_WRITE;
                                      } else {
                                        sd.type = WP_RW;
                                      }
                                    }

                                  } else if (profiling_mode == L1) {
                                    if(wpConfig.cachelineInvalidation) {
                                      int shuffleNums[CACHE_LINE_SZ/MAX_WP_LENGTH] = {0, 1, 2, 3, 4, 5, 6, 7};
                                      int idx = (rdtsc() % 4219) & (CACHE_LINE_SZ/MAX_WP_LENGTH -1); //randomly choose one location to monitor
                                      sd.va = (void *)ALIGN_TO_CACHE_LINE((size_t)(data_addr)) + (shuffleNums[idx] << 3);
                                      sd.wpLength = MAX_WP_LENGTH;
                                      //fprintf(stderr, "This region is executed\n");
                                      //fprintf(stderr, "sample tries to detect L1 cache line invalidation\n");
                                    } else {
                                      //fprintf(stderr, "sample tries to detect true sharing\n");
                                      sd.va = data_addr;
                                      sd.wpLength = MAX_WP_LENGTH;
                                    }
                                    sd.type = WP_WRITE;
                                  } 
                                } else {
                                  //fprintf(stderr, "a wp in thread %d is armed by thread %d to detect invalidation\n", indices[i], me);
                                  if(wpConfig.cachelineInvalidation) {
                                    int shuffleNums[CACHE_LINE_SZ/MAX_WP_LENGTH] = {0, 1, 2, 3, 4, 5, 6, 7};
                                    int idx = (rdtsc() % 4219) & (CACHE_LINE_SZ/MAX_WP_LENGTH -1); //randomly choose one location to monitor
                                    sd.va = (void *)ALIGN_TO_CACHE_LINE((size_t)(data_addr)) + (shuffleNums[idx] << 3);
                                    sd.wpLength = MAX_WP_LENGTH;
                                    //fprintf(stderr, "This region is executed\n");
                                    //fprintf(stderr, "sample tries to detect L3 cache line invalidation\n");
                                  } else {
                                    //fprintf(stderr, "sample tries to detect true sharing\n");
                                    sd.va = data_addr;
                                    sd.wpLength = MAX_WP_LENGTH;
                                  }
                                  sd.type = WP_WRITE;
                                }
                                //fprintf(stderr, "a wp is armed in thread %d\n",me);
                                SubscribeWatchpointShared(&sd, OVERWRITE, false, indices[i], location);
                              }
                            } 
                            lastTime = curTime;
                          }
                          break;
    case WP_AMD_REUSE:{
//#if 0
		     //fprintf(stderr, "WP_REUSE in OnSample on address: %lx\n", data_addr);
#ifdef REUSE_HISTO
#else
                     if ( accessType != reuse_monitor_type && reuse_monitor_type != LOAD_AND_STORE) break;
#endif
		     micro_op_sample = mmap_data->micro_op_sample;
		     mem_access_sample = mmap_data->mem_access_sample;
		     valid_mem_access_sample = mmap_data->valid_mem_access_sample;
		     sample_count++;
                     long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                     accessedIns += metricThreshold;
                     SampleData_t sd= {
                       .node = node,
                       .type=WP_RW,  //jqswang: Setting it to WP_READ causes segment fault
                       .accessType=accessType,
                       //.wpLength = accessLen, // set later
                       .accessLength= accessLen,
                       .sampledMetricId=sampledMetricId,
                       .isSamplePointAccurate = isSamplePointAccurate,
                       .preWPAction=theWPConfig->preWPAction,
                       .isBackTrace = false,
                     };
#ifdef REUSE_HISTO
                     sd.wpLength = 1;
#else
                     sd.wpLength = GetFloorWPLength(accessLen);
                     sd.type = WP_RW;//reuse_trap_type;
                     //fprintf(stderr, "here1\n");
#endif
                     bool isProfileSpatial;
                     if (reuse_profile_type == REUSE_TEMPORAL){
                       isProfileSpatial = false;
                     } else if (reuse_profile_type == REUSE_SPATIAL){
                       isProfileSpatial = true;
                     } else {
                       //fprintf(stderr, "50 50\n");
                       isProfileSpatial = (rdtsc() & 1);
                     }

                     //fprintf(stderr, "here2 data_addr: %lx\n", (uint64_t) data_addr);
                     if (isProfileSpatial) {// detect spatial reuse
                       int wpSizes[] = {8, 4, 2, 1};
                       FalseSharingLocs falseSharingLocs[CACHE_LINE_SZ];
                       int numFSLocs = 0;
                       GetAllFalseSharingLocations((size_t)data_addr, accessLen, ALIGN_TO_CACHE_LINE((size_t)(data_addr)), CACHE_LINE_SZ, wpSizes, 0 /*curWPSizeIdx*/ , 4 /*totalWPSizes*/, falseSharingLocs, &numFSLocs);
                       if (numFSLocs == 0) { // No location is found. It is probably due to the access length already occupies one cache line. So we just monitor the temporal reuse instead.
                         sd.va = data_addr;
                         sd.reuseType = REUSE_TEMPORAL;
                         //fprintf(stderr, "REUSE_TEMPORAL is activated\n");
                       } else {
                         int idx = rdtsc() % numFSLocs; //randomly choose one location to monitor
                         sd.va = (void *)falseSharingLocs[idx].va;
                         sd.reuseType = REUSE_SPATIAL;
                         //fprintf(stderr, "REUSE_SPATIAL is activated\n");
                       }
                     } else {
                       sd.va = data_addr;
                       sd.reuseType = REUSE_TEMPORAL;
                       //fprintf/(stderr, "REUSE_TEMPORAL is activated\n");
                     }
                     //fprintf(stderr, "here3\n");
                     if (!IsValidAddress(sd.va, precisePC)) {
                       goto ErrExit; // incorrect access type
                     }

                     //fprintf(stderr, "here4\n");
                     // Read the reuse distance event counters
                     // We assume the reading event is load, store or both.
                     //for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
                       uint64_t val[3];
                       //fprintf(stderr, "before assert\n");
		       //fprintf(stderr, "reading counter in OnSample data_addr: %lx\n", data_addr);
                       assert(linux_perf_read_event_counter( amd_reuse_distance_event, val) >= 0);
                       //fprintf(stderr, "after assert\n");
                       //fprintf(stderr, "USE %lu %lu %lu  -- ", val[0], val[1], val[2]);
                       //fprintf(stderr, "USE %lx -- ", val[0]);
                       memcpy(sd.reuseDistance[0], val, sizeof(uint64_t)*3);;
                     //}
                     //fprintf(stderr, "here5\n");
                     //fprintf(stderr, "\n");
                     // register the watchpoint
                     //fprintf(stderr, "watchpoints are about to be armed from OnSample on address %lx\n", sd.va);
                     //SubscribeWatchpointAlwaysReplace(&sd, OVERWRITE, false );
		     SubscribeWatchpoint(&sd, OVERWRITE, false );
                     //fprintf(stderr, "here6\n");
//#endif
                          }
                          break;
    case WP_AMD_REUSETRACKER:{
//#if 0
			    sample_count++;
                            int me = TD_GET(core_profile_trace_data.id);

                            int my_core = sched_getcpu();
                            int64_t storeCurTime = 0;
                            if(accessType == STORE || accessType == LOAD_AND_STORE) {
                              storeCurTime = curTime;
                              storeOlderTime = storeLastTime;
                              storeLastTime = storeCurTime;
                            }

#ifdef REUSE_HISTO
#else
                            if ( accessType != reuse_monitor_type && reuse_monitor_type != LOAD_AND_STORE) break;
#endif
                            long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                            accessedIns += metricThreshold;
                            SampleData_t sd= {
                              .node = node,
                              //.type=WP_RW,  //jqswang: Setting it to WP_READ causes segment fault
                              .accessType=accessType,
                              //.wpLength = accessLen, // set later
                              .accessLength= accessLen,
                              .sampledMetricId=sampledMetricId,
                              .isSamplePointAccurate = isSamplePointAccurate,
                              .preWPAction=theWPConfig->preWPAction,
                              .first_accessing_tid =me,
                              .isBackTrace = false,
                              .L3StoreUse = false
                            };
#ifdef REUSE_HISTO
                            //fprintf(stderr, "WP_REUSETRACKER in OnSample\n");
                            sd.wpLength = 1;
#else
                            sd.wpLength = GetFloorWPLength(accessLen);
                            sd.type = WP_RW;//reuse_trap_type;
                            //fprintf(stderr, "here1\n");
#endif
                            if (reuse_profile_type == REUSE_SPATIAL){
                              sd.reuseType = REUSE_SPATIAL;
                            } else {
                              sd.reuseType = REUSE_TEMPORAL;
                            }

                            //fprintf(stderr, "here3\n");
                            if (!IsValidAddress(data_addr, precisePC)) {
                              goto ErrExit; // incorrect access type
                            }

                            /*if((strncmp (hpcrun_id2metric(sampledMetricId)->name,"MEM_LOAD_RETIRED.L2_MISS",24) == 0))
                              fprintf(stderr, "l2 miss sample happens2\n");*/

                            //fprintf(stderr, "sample type: %s\n", hpcrun_id2metric(sampledMetricId)->name);
                            //fprintf(stderr, "here4\n");
                            // Read the reuse distance event counters
                            // We assume the reading event is load, store or both.

                            int sType = -1;

                            if (mmap_data->store) {
                            	sType = ALL_STORE;
                            	store_count++;
                            	//fprintf(stderr, "store sample is detected, store: %d\n", mmap_data->store);
                            }
                            else if(mmap_data->load) {
                            	sType = ALL_LOAD;
                            	load_count++;
                                //fprintf(stderr, "load sample is detected, load: %d\n", mmap_data->load);
                            }                            
			    //fprintf(stderr, "threads are selected here\n");
                            //fprintf(stderr, "sampledMetricId: %d\n", sampledMetricId);
                            prev_sample_timestamp = last_sample_timestamp;
                            last_sample_timestamp = curTime;
                            int location = -1;
                            bool steal_wp_slot = false;

			    //fprintf(stderr, "used_wp_count: %d, MIN(global_thread_count, wpConfig.maxWP): %d\n", used_wp_count, MIN(global_thread_count, wpConfig.maxWP));
                            if(used_wp_count < MIN(global_thread_count, wpConfig.maxWP)) {
                              uint64_t theCounter = globalReuseWPs.counter;
                              if((theCounter & 1) == 0) {
                                if(__sync_bool_compare_and_swap(&globalReuseWPs.counter, theCounter, theCounter+1)) {
                                  // before
                                  for(int j = 0; j < wpConfig.maxWP; j++) {
                                    if(me == globalWPIsUsers[j]) {
				      //fprintf(stderr, "thread %d owns the watchpoint at location %d\n", me, j);
                                      location = j;
                                      break;
                                    }
                                  }
                                  if (location == -1) {
                                    for(int j = 0; j < wpConfig.maxWP; j++) {
                                      if(globalWPIsUsers[j] == -1) {
                                        used_wp_count++;
                                        location = j;
                                        steal_wp_slot = true;
                                        globalWPIsUsers[j] = me;
					//fprintf(stderr, "thread %d takes over the watchpoint at location %d\n", me, j);
                                        break; 
                                      }
                                    }
                                  }
                                  // after       	
                                  globalReuseWPs.counter++;
                                }
                              } 
                            }

                            if(steal_wp_slot) {

                              if(max_used_wp_count < used_wp_count)
                                max_used_wp_count = used_wp_count;
                              globalReuseWPs.table[location].tid = me;
                              wait_threshold = sample_count + CHANGE_THRESHOLD;
                              // some code
                            } 

                            int item_not_found_flag = 0;
                            sd.sampleTime=curTime;

                            int monitored_location = -1;

                            for(int j = 0; j < used_wp_count; j++) {
                              if(me == globalReuseWPs.table[j].monitored_tid) {
                                monitored_location = j;
                                break;
                              }
                            }
//#if 0
                            if(monitored_location != -1 && (profiling_mode == L3 || profiling_mode == MIXED)) {
                              if(!globalReuseWPs.table[monitored_location].sharedActive && globalReuseWPs.table[monitored_location].self_trap && (globalReuseWPs.table[monitored_location].rd > 0)) {
                                WatchPointInfo_t * wpi = getWPI(me, monitored_location);
                                if(wpi && wpi->sample.node) {
                                  double myProportion = ProportionOfWatchpointAmongOthersSharingTheSameContext(wpi);
                                  uint64_t numDiffSamples = GetWeightedMetricDiff(wpi->sample.node, wpi->sample.sampledMetricId, myProportion); //hpcrun_id2metric(sampledMetricId)->period;
                                  double inc_scale = dynamic_global_thread_count / (double) max_used_wp_count;
                                  uint64_t inc = numDiffSamples * inc_scale;
                                  bool comm_reuse = false;
                                  //uint64_t theCounter = shared_reuse_counter;
                                  //if((theCounter & 1) == 0) {
                                  //if(__sync_bool_compare_and_swap(&shared_reuse_counter, theCounter, theCounter+1)) {
                                  SharedReuseAddDistance(globalReuseWPs.table[monitored_location].rd, inc);
                                  if(globalReuseWPs.table[monitored_location].is_rar == false) {
                                    CommunicationReuseAddDistance(globalReuseWPs.table[monitored_location].rd, inc);
                                    comm_reuse = true;
                                  }
                                  if(profiling_mode == L3)
                                    numWatchpointArmingAttempt[monitored_location] = SAMPLES_POST_FULL_RESET_VAL;
                                  cct_metric_data_increment(l3_reuse_memory_distance_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = (globalReuseWPs.table[monitored_location].rd) });
                                  //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
                                  cct_metric_data_increment(l3_reuse_memory_distance_count_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = 1});
                                  if(comm_reuse) {
                                    cct_metric_data_increment(comm_reuse_memory_distance_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = (globalReuseWPs.table[monitored_location].rd) });
                                    //fprintf(stderr, "reuse distance: %ld\n", (val[0][0] + val[1][0]));
                                    cct_metric_data_increment(comm_reuse_memory_distance_count_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = 1});
                                  }
                                  if (reuse_profile_type == REUSE_TEMPORAL) {
                                    cct_metric_data_increment(l3_temporal_reuse_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = inc});
                                    if(comm_reuse) {
                                      cct_metric_data_increment(comm_temporal_reuse_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = inc});
                                    }
                                  } else if (reuse_profile_type == REUSE_SPATIAL) {
                                    cct_metric_data_increment(l3_spatial_reuse_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = inc});
                                    if(comm_reuse) {
                                      cct_metric_data_increment(comm_spatial_reuse_metric_id, globalReuseWPs.table[monitored_location].reusePairNode, (cct_metric_data_t){.i = inc});
                                    }	
                                  }
                                  //fprintf(stderr, "histogram is updated by %ld, rd: %ld, me: %d, monitored_tid: %d\n", inc, globalReuseWPs.table[monitored_location].rd, me, globalReuseWPs.table[monitored_location].monitored_tid);
                                  globalReuseWPs.table[monitored_location].rd = 0;
                                  //shared_reuse_counter++;
                                  //}
                                  //}
                                }
                                // update shared reuse histogram
                              }
                            } 

                            location = -1;

                            for(int j = 0; j < used_wp_count; j++) {
                              if(me == globalWPIsUsers[j]) {
                                location = j;
                                break;
                              }
                            }


			   if(location ==  -1) {
                              uint64_t timestamp = MonitoredNode.timestamp;
                              if((last_monitored_wp_timestamp == timestamp) && (last_trapped_timestamp != timestamp) && (MonitoredNode.self_trap == false)) {
                                //reset context scaling count in monitored_node
                                //fprintf(stderr, "inc is reset because of approximation, location: %d, last_monitored_wp_timestamp: %ld, timestamp: %ld, last_trapped_timestamp: %ld, self_trap: %d\n", location, last_monitored_wp_timestamp, timestamp, last_trapped_timestamp, MonitoredNode.self_trap);
                                GetWeightedMetricDiffAndReset(monitored_node, monitored_metric, 1.0);
                                last_trapped_timestamp = timestamp;
                              }
                              else if(last_monitored_wp_timestamp != timestamp) {
                                //fprintf(stderr, "monitored_node is replaced, location: %d, last_monitored_wp_timestamp: %ld, timestamp: %ld\n", location, last_monitored_wp_timestamp, timestamp);
                                monitored_node = node;
                                monitored_metric = MonitoredNode.metricId;
                                last_monitored_wp_timestamp = timestamp;
                              }
                            } 

                            if ((location != -1) && (sample_count > wait_threshold)) {
                              globalWPIsUsers[location] = -1;
                              globalReuseWPs.table[location].tid = -1;

                              //globalReuseWPs.table[location].residueSampleCountInPrevThread = GetWeightedMetricDiff(node, sampledMetricId, 1.0);
                              //fprintf(stderr, "residueSampleCountInPrevThread is %ld when thread %d is releasing a global WP\n", globalReuseWPs.table[location].residueSampleCountInPrevThread, me);
                              if(MonitoredNode.tid == me)
                                MonitoredNode.tid = -1;
			      //fprintf(stderr, "WP number %d is released by thread %d, sample_count: %d, wait_threshold: %d\n", location, me, sample_count, wait_threshold);
                              location = -1;
			      //fprintf(stderr, "WP number %d is released by thread %d, sample_count: %d, wait_threshold: %d\n", location, me, sample_count, wait_threshold);
                              used_wp_count--;                                                                     
                            }


                            //fprintf(stderr, "location: %d, thread: %d\n", location, me);
                            if((location != -1) && ArmWatchPointProb(&location, curTime, me)) {

                              uint64_t theCounter = MonitoredNode.counter;
                              if((theCounter & 1) == 0) {
                                if(__sync_bool_compare_and_swap(&MonitoredNode.counter, theCounter, theCounter+1)) {

                                  if((MonitoredNode.tid == -1) || ((MonitoredNode.self_trap == true) && (MonitoredNode.tid == me)) || ((MonitoredNode.self_trap == false) && ((curTime - MonitoredNode.trap_timestamp) >= (2 * (curTime - lastTime))))) {
                                    //fprintf(stderr, "published monitored node is changed, MonitoredNode.tid: %d, self trap: %d, me: %d, time_gap: %ld, expiry period: %ld\n", MonitoredNode.tid, MonitoredNode.self_trap, me, (curTime - MonitoredNode.trap_timestamp), (2 * (curTime - lastTime)));
                                    MonitoredNode.timestamp = curTime;
                                    MonitoredNode.trap_timestamp = 0;
                                    MonitoredNode.metricId = sampledMetricId;
                                    MonitoredNode.self_trap = true;
                                    MonitoredNode.tid = me;
                                  }
                                  MonitoredNode.counter++;
                                }
                              }	

                              // before
                              int affinity_l3;
                              int affinity_l2;
                              if(l2_count > 1) {
                                affinity_l3 = thread_to_l3_mapping[my_core]; 
                                sd.L3Id = affinity_l3;
                                affinity_l2 = thread_to_l2_mapping[my_core];
                                sd.L2Id = affinity_l2;
                              }

                              for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
                                for(int j=0; j < 3; j++) {
                                  sd.sharedReuseDistance[0][j] = 0;
                                }
                              }

                              int cur_global_thread_count = global_thread_count;
                              if(profiling_mode == L3 || profiling_mode == MIXED) {
                                for(int i = 0; i < cur_global_thread_count; i++) {
                                  if((mapping_size == 0) || (l3_count == 1) || (thread_to_l3_mapping[mapping_vector[i % mapping_size]] == affinity_l3)) {
				    uint64_t val[3];  
                                    linux_perf_read_event_counter_shared( amd_reuse_distance_event, val, i/*locality_vector[affinity_l3][i+1]*/);
                                    for(int k=0; k < 3; k++) {
                                    	sd.sharedReuseDistance[0][k] += val[k];
                                    }
                                  }
                                }
                              }


                              // after
                              int indices[cur_global_thread_count];
                              for (int i = 0; i < cur_global_thread_count; i++) {
                                indices[i] = i;
                              }

                              int wp_index = cur_global_thread_count;
                              while (wp_index) { 
                                int index = rdtsc() % wp_index;
                                wp_index--;
                                int swap = indices[index];
                                indices[index] = indices[wp_index];
                                indices[wp_index] = swap;
                              }


                              //uint64_t pmu_counter = 0;
                              sd.L1Sample = true;
                              //for (int i=0; i < MIN(2, reuse_distance_num_events); i++){
                                uint64_t val[3];
//#if 0
                                //if(sType == ALL_LOAD || sType == ALL_STORE)
                                assert(linux_perf_read_event_counter_l1_amd( amd_reuse_distance_event, val) >= 0);
                                //else
                                  //assert(linux_perf_read_event_counter_l1( amd_reuse_distance_event, val, false) >= 0);
//#endif
                                // this
				//fprintf(stderr, "val[0]: %ld val[1]: %ld in thread %d\n", val[0], val[1], me);
                                //val[1] = 0;
                                memcpy(sd.reuseDistance[0], val, sizeof(uint64_t)*3);
                                //pmu_counter += val[0];
                              //}	

                              FalseSharingLocs falseSharingLocs[CACHE_LINE_SZ];
                              int numFSLocs = 0;
                              if (reuse_profile_type == REUSE_SPATIAL){
                                int wpSizes[] = {8, 4, 2, 1};
                                GetAllFalseSharingLocations((size_t)data_addr, accessLen, ALIGN_TO_CACHE_LINE((size_t)(data_addr)), CACHE_LINE_SZ, wpSizes, 0 /*curWPSizeIdx*/ , 4 /*totalWPSizes*/, falseSharingLocs, &numFSLocs);
                              }

                              //void * original_va = sd.va;
                              int original_wpLength = sd.wpLength;
//#if 0
                              for(int i = 0; i < cur_global_thread_count; i++) {
                                int core_id;
                                if(mapping_size > 0) {
                                  core_id = mapping_vector[indices[i] % mapping_size];	
                                }
                                if((mapping_size == 0) || (thread_to_l3_mapping[core_id] == affinity_l3)) {
                                  //fprintf(stderr, "a wp in thread %d is armed by thread %d to detect reuse\n", indices[i], me);
                                  if(indices[i] == me || profiling_mode == L3 || profiling_mode == MIXED) { 
                                    if (reuse_profile_type == REUSE_SPATIAL){ 
                                      if (numFSLocs == 0) { // No location is found. It is probably due to the access length already occupies one cache line. So we just monitor the temporal reuse instead.
                                        sd.va = data_addr;
                                        sd.reuseType = REUSE_TEMPORAL;
                                        sd.wpLength = 1;
                                        //fprintf(stderr, "REUSE_TEMPORAL is activated\n");
                                      } else {
                                        //fprintf(stderr, "false sharing is searched\n");
                                        int idx = rdtsc() % numFSLocs; //randomly choose one location to monitor
                                        sd.va = (void *)falseSharingLocs[idx].va;
                                        sd.wpLength = falseSharingLocs[idx].wpLen;
                                        sd.reuseType = REUSE_SPATIAL;
                                      }
                                    } else {

                                      sd.reuseType = REUSE_TEMPORAL;
                                      sd.va = data_addr;
                                      sd.wpLength = 1;
                                      //fprintf(stderr, "sample tries to detect temporal reuse in L3\n");
                                    }
                                    if(indices[i] == me || profiling_mode == L3) {
                                      sd.type = WP_RW;
				      //fprintf(stderr, "a wp in thread %d is armed with WP_RW type by thread %d to detect reuse\n", indices[i], me);
                                    } else if (profiling_mode == MIXED) {
                                      if((rdtsc() % 100) >= 50) {
                                        sd.type = WP_WRITE;
                                      } else {
                                        sd.type = WP_RW;
                                      }
                                    }

                                  } else if (profiling_mode == L1) {
                                    if(wpConfig.cachelineInvalidation) {
                                      int shuffleNums[CACHE_LINE_SZ/MAX_WP_LENGTH] = {0, 1, 2, 3, 4, 5, 6, 7};
                                      int idx = (rdtsc() % 4219) & (CACHE_LINE_SZ/MAX_WP_LENGTH -1); //randomly choose one location to monitor
                                      sd.va = (void *)ALIGN_TO_CACHE_LINE((size_t)(data_addr)) + (shuffleNums[idx] << 3);
                                      sd.wpLength = MAX_WP_LENGTH;
                                      //fprintf(stderr, "This region is executed\n");
                                      //fprintf(stderr, "sample tries to detect L1 cache line invalidation\n");
                                    } else {
                                      //fprintf(stderr, "sample tries to detect true sharing\n");
                                      sd.va = data_addr;
                                      sd.wpLength = MAX_WP_LENGTH;
                                    }
                                    sd.type = WP_WRITE;
                                  } 
                                } else {
                                  //fprintf(stderr, "a wp in thread %d is armed by thread %d to detect invalidation\n", indices[i], me);
                                  if(wpConfig.cachelineInvalidation) {
                                    int shuffleNums[CACHE_LINE_SZ/MAX_WP_LENGTH] = {0, 1, 2, 3, 4, 5, 6, 7};
                                    int idx = (rdtsc() % 4219) & (CACHE_LINE_SZ/MAX_WP_LENGTH -1); //randomly choose one location to monitor
                                    sd.va = (void *)ALIGN_TO_CACHE_LINE((size_t)(data_addr)) + (shuffleNums[idx] << 3);
                                    sd.wpLength = MAX_WP_LENGTH;
                                    //fprintf(stderr, "This region is executed\n");
                                    //fprintf(stderr, "sample tries to detect L3 cache line invalidation\n");
                                  } else {
                                    //fprintf(stderr, "sample tries to detect true sharing\n");
                                    sd.va = data_addr;
                                    sd.wpLength = MAX_WP_LENGTH;
                                  }
				  //fprintf(stderr, "a wp in thread %d is armed with WP_WRITE type by thread %d to detect invalidation\n", indices[i], me);
                                  sd.type = WP_WRITE;
                                }
                                //fprintf(stderr, "wp in thread %d is armed by thread %d\n",indices[i], me);
                                SubscribeWatchpointShared(&sd, OVERWRITE, false, indices[i], location);
                              }
//#endif
                            } 
//#endif
                            lastTime = curTime;
//#endif

			  }
			  break;
    case WP_SPATIAL_REUSE:{
                            long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                            accessedIns += metricThreshold;

                            SampleData_t sd= {
                              .node = node,
                              .type=WP_RW,
                              .accessType=accessType,
                              .wpLength = accessLen,
                              .accessLength= accessLen,
                              .sampledMetricId=sampledMetricId,
                              .isSamplePointAccurate = isSamplePointAccurate,
                              .preWPAction=theWPConfig->preWPAction,
                              .isBackTrace = false
                            };
                            sd.wpLength = GetFloorWPLength(accessLen);
                            // randomly protect another word in the same cache line
                            uint64_t aligned_pc = ALIGN_TO_CACHE_LINE((uint64_t)data_addr);
                            if ((rdtsc() & 1) == 0)
                              sd.va = (void*) (aligned_pc - CACHE_LINE_SZ);
                            else
                              sd.va = (void *) (aligned_pc + CACHE_LINE_SZ);
#if 0
                            int offset = ((uint64_t)data_addr - aligned_pc) / accessLen;
                            int bound = CACHE_LINE_SZ / accessLen;
                            int r = rdtsc() % bound;
                            if (r == offset) r = (r+1) % bound;
                            sd.va = aligned_pc + (r * accessLen);
#endif
                            if (!IsValidAddress(sd.va, precisePC)) {
                              goto ErrExit; // incorrect access type
                            }
                            SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                          }
                          break;
    case WP_TEMPORAL_REUSE:{
                             long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                             accessedIns += metricThreshold;

                             SampleData_t sd= {
                               .va = data_addr,
                               .node = node,
                               .type=WP_RW,
                               .accessType=accessType,
                               .wpLength = accessLen,
                               .accessLength= accessLen,
                               .sampledMetricId=sampledMetricId,
                               .isSamplePointAccurate = isSamplePointAccurate,
                               .preWPAction=theWPConfig->preWPAction,
                               .isBackTrace = false
                             };
                             sd.wpLength = GetFloorWPLength(accessLen);
                             SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                           }
                           break;
    case WP_FALSE_SHARING:
    case WP_TRUE_SHARING:
    case WP_ALL_SHARING:{
                          sample_count++;
                          //fprintf(stderr, "SHARING in OnSample\n");
                          // Is the published address old enough (stayed for > 1 sample time span)
                          int64_t curTime = rdtsc();
                          SharedData_t localSharedData;
                          int me = TD_GET(core_profile_trace_data.id);
                          // Get the time, tid, and counter
                          // This is definately racy but benign.
                          localSharedData.time = gSharedData.time;
                          localSharedData.tid = gSharedData.tid;
                          localSharedData.counter = gSharedData.counter;

                          //ReadSharedDataTransactionally(&localSharedData);
                          if( ((curTime-localSharedData.time) > 2 * (curTime-lastTime)) // Sufficient time passed since the last time somebody published
                              &&
                              ((localSharedData.counter & 1) == 0) // Nobody is in the process of publishing
                            ) {
                            // Attempt to replace WP with my new address
                            uint64_t theCounter = localSharedData.counter;
                            localSharedData.time = rdtsc();
                            localSharedData.tid = me;
                            localSharedData.wpType = accessType == LOAD ? WP_WRITE : WP_RW;
                            localSharedData.accessType = accessType;
                            localSharedData.address = data_addr;
                            localSharedData.accessLen = accessLen;
                            localSharedData.counter ++; // makes the counter odd
                            localSharedData.node = node;

                            if(__sync_bool_compare_and_swap(&gSharedData.counter, theCounter, theCounter+1)){
                              gSharedData = localSharedData;
                              __sync_synchronize();
                              gSharedData.counter++; // makes the counter even
                            } else {
                              // Failed to update ==> someone else succeeded ==> Fetch that address and set a WP for that
                              goto SET_FS_WP;
                            }
                          } else if ((localSharedData.tid != me)  && (localSharedData.tid != -1)/* dont set WP for my own accessed locations */){
                            // If the data is "new" set the WP
SET_FS_WP: ReadSharedDataTransactionally(&localSharedData);
           long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
           accessedIns += metricThreshold;

           switch (theWPConfig->id) {
             case WP_TRUE_SHARING:{
                                    // Set WP at the same address
                                    SampleData_t sd= {
                                      .va = localSharedData.address,
                                      .node = localSharedData.node,
                                      .accessType=localSharedData.accessType,
                                      .type=localSharedData.wpType,
                                      .wpLength = GetFloorWPLengthAtAddress(localSharedData.address, accessLen),
                                      .accessLength= accessLen,
                                      .sampledMetricId=sampledMetricId,
                                      .isSamplePointAccurate = isSamplePointAccurate,
                                      .preWPAction=theWPConfig->preWPAction,
                                      .isBackTrace = false
                                    };
                                    SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                                  }
                                  break;
             case WP_FALSE_SHARING: {
                                      //fprintf(stderr, "in case WP_FALSE_SHARING\n");
                                      int wpSizes[] = {8, 4, 2, 1};
                                      FalseSharingLocs falseSharingLocs[CACHE_LINE_SZ];
                                      int numFSLocs = 0;
                                      GetAllFalseSharingLocations( (size_t) localSharedData.address, accessLen, ALIGN_TO_CACHE_LINE((size_t)localSharedData.address), CACHE_LINE_SZ, wpSizes, 0 /*curWPSizeIdx*/ , 4 /*totalWPSizes*/, falseSharingLocs, &numFSLocs);
                                      // Find 4 slots in the cacheline
                                      for(int i = 0; i < numFSLocs/2; i ++) {
                                        int idx = rdtsc() % numFSLocs;
                                        FalseSharingLocs tmpVal = falseSharingLocs[idx];
                                        falseSharingLocs[idx] = falseSharingLocs[i];
                                        falseSharingLocs[i] = tmpVal;
                                      }
                                      for(int i = 0; i < MIN(numFSLocs, wpConfig.maxWP); i ++) {
                                        SampleData_t sd= {
                                          .va = (void *) falseSharingLocs[i].va,
                                          .node = localSharedData.node,
                                          .accessType=localSharedData.accessType,
                                          .type=localSharedData.wpType,
                                          .wpLength = falseSharingLocs[i].wpLen,
                                          .accessLength= accessLen,
                                          .sampledMetricId=sampledMetricId,
                                          .isSamplePointAccurate = isSamplePointAccurate,
                                          .preWPAction=theWPConfig->preWPAction,
                                          .isBackTrace = false
                                        };
                                        //fprintf(stderr, "in OnSample WP_FALSE_SHARING\n");
                                        SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                                      }
                                    }
                                    break;
             case WP_ALL_SHARING: {
                                    void * cacheLineBaseAddress = (void *) ALIGN_TO_CACHE_LINE((size_t)localSharedData.address);
                                    // Find 4 slots in the cacheline
                                    // FIXME: make dynamic
                                    int shuffleNums[CACHE_LINE_SZ/MAX_WP_LENGTH] = {0, 1, 2, 3, 4, 5, 6, 7}; // hard coded
                                    for(int i = 0; i < CACHE_LINE_SZ/MAX_WP_LENGTH/2; i ++) {
                                      int idx = rdtsc() & (CACHE_LINE_SZ/MAX_WP_LENGTH -1);
                                      int tmpVal = shuffleNums[idx];
                                      shuffleNums[idx] = shuffleNums[i];
                                      shuffleNums[i] = tmpVal;
                                    }
                                    for(int i = 0; i < wpConfig.maxWP; i ++) {
                                      SampleData_t sd= {
                                        .va = cacheLineBaseAddress + (shuffleNums[i] << 3),
                                        .node = localSharedData.node,
                                        .accessType=localSharedData.accessType,
                                        .type=localSharedData.wpType,
                                        .wpLength = MAX_WP_LENGTH,
                                        .accessLength= accessLen,
                                        .sampledMetricId=sampledMetricId,
                                        .isSamplePointAccurate = isSamplePointAccurate,
                                        .preWPAction=theWPConfig->preWPAction,
                                        .isBackTrace = false
                                      };
                                      SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                                    }
                                  }
                                  break;
             default:
                                  break;
           }
                          }else{
                            /* dont set WP for my own accessed locations */
                          }
                          lastTime = curTime;
                        }
                        break;

    case WP_IPC_FALSE_SHARING:
    case WP_IPC_TRUE_SHARING: {
                                UpdateVMMap();
                                HandleIPCFalseSharing(data_addr, precisePC, node, accessLen, accessType, sampledMetricId, isSamplePointAccurate);
                              }
                              break;

    case WP_AMD_COMM:	{
				//fprintf(stderr, "WP_AMD_COMM is handled\n");	
				//fprintf(stderr, "sampling timestamp: %ld, cpu: %d, tid: %d, pid: %d, sampled address: %lx, load: %d, store:%d, handled by thread %ld\n", mmap_data->time, mmap_data->cpu, mmap_data->tid, mmap_data->pid, mmap_data->addr, mmap_data->load, mmap_data->store, syscall(SYS_gettid));
				//fprintf(stderr, "global_thread_count: %d\n", global_thread_count);
				int sType = -1;
                            	sample_count++;
				micro_op_sample = mmap_data->micro_op_sample;
				mem_access_sample = mmap_data->mem_access_sample;
				valid_mem_access_sample = mmap_data->valid_mem_access_sample;

				if(mmap_data->addr_valid) {
					addr_valid_count++;
					//fprintf(stderr, "valid address is detected, addr_valid: %d\n", mmap_data->addr_valid);
				}
				if(mmap_data->phy_addr_valid) {
                                        phy_addr_valid_count++;
                                        //fprintf(stderr, "valid address is detected, addr_valid: %d\n", mmap_data->addr_valid);
                                }
				//fprintf(stderr, "mmap_data->addr_valid: %d, mmap_data->phy_addr_valid: %d, mmap_data->addr: %lx, mmap_data->phy_addr: %lx\n", mmap_data->addr_valid, mmap_data->phy_addr_valid, mmap_data->addr, mmap_data->phy_addr);
                            	if (mmap_data->store) {
                              		sType = ALL_STORE;
					store_count++;
					//fprintf(stderr, "store sample is detected, store: %d\n", mmap_data->store);
				}
                            	else if(mmap_data->load) {
                              		sType = ALL_LOAD;
					global_load_count++;
					load_count++;
					//fprintf(stderr, "load sample is detected, load: %d\n", mmap_data->load);
				}
				//int metricId = -1;
                            	const void* joinNode;  
                            	int joinNodeIdx = isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

                            	uint64_t curtime = rdtsc();

				int64_t storeCurTime = 0;
                            	if(sType == ALL_STORE /*accessType == STORE || accessType == LOAD_AND_STORE*/)
                              		storeCurTime = curtime;


                            	int me = TD_GET(core_profile_trace_data.id);
                            	int current_core = sched_getcpu();
                            // L1 = getCacheline ( M1 )
                            	void * cacheLineBaseAddressVar = (void *) ALIGN_TO_CACHE_LINE((size_t)data_addr);
                            	int item_not_found = 0;
                            	struct SharedEntry item;
                            	do{
                              		int64_t startCounter = bulletinBoard.counter;
                              		if(startCounter & 1) {
                                		continue;
                              		}
                              		//__sync_synchronize();
                              		// entry = BulletinBoard.AtomicGet (key= L1 )
                              		item = getEntryFromBulletinBoard(cacheLineBaseAddressVar, &item_not_found);
                              		//__sync_synchronize();
                              		int64_t endCounter = bulletinBoard.counter;
                              		if(startCounter == endCounter) {
                                		break;
                              		}
                            	}while(1);

				int arm_watchpoint_flag = 0;

                            // if entry == NULL then // nothing was found related to cachelineBaseAddr in bb
                            if((item.cacheLineBaseAddress == -1) || (item_not_found == 1)) {
                              //fprintf(stderr, "not found\n");
                              // TryArmWatchpoint( T 1 )
                              arm_watchpoint_flag = 1;
                              // else
                            } else { // something was found related to cachelineBaseAddr in bb, com detected on sample
                              //fprintf(stderr, "found\n");
                              // < M2 , δ2 , ts2 , T2 > = getEntryAttributes (entry)
                              // if T1 != T2 and ts2 > tprev then
				if(amd_global_sampling_period == 0) {
					amd_global_sampling_period = (double) hpcrun_id2metric(sampledMetricId)->period;
				}
                              if((me != item.tid) && (item.time > prev_timestamp) && ((curtime - item.time) <= (2 * item.expiration_period))) {
                                int flag = 0;
                                double global_sampling_period = 0;
                                if(sType == ALL_LOAD /*accessType == LOAD*/) { // means that the sample is (read) (WAR)
                                  global_sampling_period = (double) hpcrun_id2metric(sampledMetricId)->period;
                                  flag = 1;
                                }
                                if(sType == ALL_STORE) { // means that the sample is a store type (write) (WAW)
                                  global_sampling_period = (double) hpcrun_id2metric(sampledMetricId)->period;
                                  flag = 2;
                                }
			       //fprintf(stderr, "IBS_OP sample period in OnSample: %0.2lf\n", (double) global_sampling_period);	
                                int max_thread_num = item.tid; 
                                if(max_thread_num < me) 
                                {   
                                  max_thread_num = me; 
                                }
                                if(as_matrix_size < max_thread_num) 
                                { 
#if ADAMANT_USED  
                                  matrix_size_set(max_thread_num);
#endif
                                  fs_matrix_size =  max_thread_num;
                                  ts_matrix_size =  max_thread_num;
                                  as_matrix_size =  max_thread_num;  
                                }

                                int max_core_num = item.core_id;
                                if(max_core_num < current_core)
                                {
                                  max_core_num = current_core;
                                }
                                if(as_core_matrix_size < max_core_num)
                                {
#if ADAMANT_USED
                                  core_matrix_size_set(max_core_num);
#endif
                                  fs_core_matrix_size =  max_core_num;
                                  ts_core_matrix_size =  max_core_num;
                                  as_core_matrix_size =  max_core_num;
                                }
				int metricId = -1;
				//fprintf(stderr, "accessLen of sampled access is %d\n", accessLen);
                                if(flag == 1) {  // if sType is all_loads (WAR)
                                  int id = -1;
                                  //int metricId = -1;
                                  double increment = /*valid_sample_count1 / valid_sample_count **/ /*thread_coefficient(as_matrix_size) **/ global_sampling_period; //* thread_coefficient(as_matrix_size);
#if 0
				  if(global_thread_count > 2)
				  	valid_sample_count = 0;
#endif
				  // if [M1 , M1 + δ1 ) overlaps with [M2 , M2 + δ2 ) the
                                  if(GET_OVERLAP_BYTES(item.address, item.accessLen, data_addr, accessLen) > 0) { //then ts

                                    // ends
                                    trueWRIns ++;
                                    metricId = true_wr_metric_id;
                                    joinNode = joinNodes[E_TRUE_WR_SHARE][joinNodeIdx];


				    //fprintf(stderr, "fraction of increment: %0.2lf\n", (double) (curtime - item.time) / item.expiration_period);
                                    ts_matrix[item.tid][me] = ts_matrix[item.tid][me] + increment;
                                    war_ts_matrix[item.tid][me] = war_ts_matrix[item.tid][me] + increment;
                                    if(item.core_id != current_core) {
                                      ts_core_matrix[item.core_id][current_core] = ts_core_matrix[item.core_id][current_core] + increment;
                                      war_ts_core_matrix[item.core_id][current_core] = war_ts_core_matrix[item.core_id][current_core] + increment;        			    }
                                  } else { 
                                

                                    falseWRIns ++;
                                    metricId = false_wr_metric_id;
                                    joinNode = joinNodes[E_FALSE_WR_SHARE][joinNodeIdx];

				    //fprintf(stderr, "fraction of increment: %0.2lf\n", (double) (curtime - item.time) / item.expiration_period);
                                    fs_matrix[item.tid][me] = fs_matrix[item.tid][me] + increment;
                                    war_fs_matrix[item.tid][me] = fs_matrix[item.tid][me] + increment;
                                    if(item.core_id != current_core) {
                                      fs_core_matrix[item.core_id][current_core] = fs_core_matrix[item.core_id][current_core] + increment;
                                      war_fs_core_matrix[item.core_id][current_core] = war_fs_core_matrix[item.core_id][current_core] + increment;
                                    }
                                  }
                                  as_matrix[item.tid][me] = as_matrix[item.tid][me] + increment;
                                  war_as_matrix[item.tid][me] = war_as_matrix[item.tid][me] + increment;
                                  if(item.core_id != current_core) {
                                    as_core_matrix[item.core_id][current_core] = as_core_matrix[item.core_id][current_core] + increment;
                                    war_as_core_matrix[item.core_id][current_core] = war_as_core_matrix[item.core_id][current_core] + increment;
                                  }	
                                  // tprev = ts2
                                  prev_timestamp = item.time;

                                  //sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0, 1, NULL);
                                  // insert a special node
                                  /*cct_node_t *node1 = hpcrun_insert_special_node(node, joinNode);
                                    node1 = hpcrun_cct_insert_path_return_leaf(item.node, node1);
                                  // update the metricId
                                  cct_metric_data_increment(metricId, node1, (cct_metric_data_t){.i = 1});*/

                                }
                                else if(flag == 2) {  // if sType is all_stores (WAW)
                                  int id = -1;
                                  //int metricId = -1;
                                  double increment = /*valid_sample_count1 / valid_sample_count **/ /*thread_coefficient(as_matrix_size) **/ global_sampling_period; //* thread_coefficient(as_matrix_size);
#if 0
				  if(global_thread_count > 2)
				  	valid_sample_count = 0;
#endif
                                  // if [M1 , M1 + δ1 ) overlaps with [M2 , M2 + δ2 ) the
                                  if(GET_OVERLAP_BYTES(item.address, item.accessLen, data_addr, accessLen) > 0) { //then ts
                                    // ends

                                    trueWWIns ++;
                                    metricId = true_ww_metric_id;
                                    joinNode = joinNodes[E_TRUE_WW_SHARE][joinNodeIdx];

                                    ts_matrix[item.tid][me] = ts_matrix[item.tid][me] + increment;
                                    waw_ts_matrix[item.tid][me] = waw_ts_matrix[item.tid][me] + increment;
                                    if(item.core_id != current_core) { 
                                      ts_core_matrix[item.core_id][current_core] = ts_core_matrix[item.core_id][current_core] + increment;
                                      waw_ts_core_matrix[item.core_id][current_core] = waw_ts_core_matrix[item.core_id][current_core] + increment;			    }
                                  } else {
                                    /*falseWWIns ++;
                                      metricId =  false_ww_metric_id;
                                      cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});*/
                                    // Record false sharing
#if ADAMANT_USED
                                   #endif
                                    falseWWIns ++;
                                    metricId = false_ww_metric_id;
                                    joinNode = joinNodes[E_FALSE_WW_SHARE][joinNodeIdx];

                                    fs_matrix[item.tid][me] = fs_matrix[item.tid][me] + increment;
                                    waw_fs_matrix[item.tid][me] = waw_fs_matrix[item.tid][me] + increment;
                                    if(item.core_id != current_core) {
#if ADAMANT_USED
#endif
                                      fs_core_matrix[item.core_id][current_core] = fs_core_matrix[item.core_id][current_core] + increment;
                                      waw_fs_core_matrix[item.core_id][current_core] = waw_fs_core_matrix[item.core_id][current_core] + increment;
                                    }
                                  }
                                  as_matrix[item.tid][me] = as_matrix[item.tid][me] + increment;
                                  waw_as_matrix[item.tid][me] = waw_as_matrix[item.tid][me] + increment;
                                  if(item.core_id != current_core) {
                                    as_core_matrix[item.core_id][current_core] = as_core_matrix[item.core_id][current_core] + increment;
                                    waw_as_core_matrix[item.core_id][current_core] = waw_as_core_matrix[item.core_id][current_core] + increment;
                                  }	
                                  // tprev = ts2
                                  prev_timestamp = item.time;
                                  /*
                                     sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0, 1, NULL);
                                  // insert a special node
                                  cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
                                  node = hpcrun_cct_insert_path_return_leaf(wpi->sample.node, node);
                                  // update the metricId
                                  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
                                   */
                                }

//#if 0	
                                sample_val_t v = hpcrun_sample_callpath(context, metricId /*measured_metric_id*/, SAMPLE_UNIT_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
                                cct_node_t *node1 = hpcrun_insert_special_node(v.sample_node, joinNode);
                                node1 = hpcrun_cct_insert_path_return_leaf(item.node, node1);
                                // update the metricId
                                cct_metric_data_increment(metricId, node1, (cct_metric_data_t){.i = 1});
//#endif
                                // after
                              } else {
                                // TryArmWatchpoint(T1)
                                arm_watchpoint_flag = 1;
                              }
                            }

                            if (arm_watchpoint_flag) {
                              // begin watchpoints
                              int do_not_arm_watchpoint = 0;
                              // getting an unexpired address from BulletinBoard that is not from T
                              struct SharedEntry localSharedData;
                              do{ 
                                int64_t startCounter1 = bulletinBoard.counter;
                                if(startCounter1 & 1) {
                                  continue;
                                }
                                localSharedData = getEntryRandomlyFromBulletinBoard(me, curtime, &do_not_arm_watchpoint);	
                                int64_t endCounter1 = bulletinBoard.counter;
                                if(startCounter1 == endCounter1) {
                                  break;
                                }
                              }while(1);

                              if((localSharedData.cacheLineBaseAddress != -1) && !do_not_arm_watchpoint) {
                                long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                                accessedIns += metricThreshold;
                                void * cacheLineBaseAddress = localSharedData.cacheLineBaseAddress;
                                int shuffleNums[CACHE_LINE_SZ/MAX_WP_LENGTH] = {0, 1, 2, 3, 4, 5, 6, 7}; // hard coded
                                for(int i = 0; i < wpConfig.maxWP; i ++) {
                                  int idx = rdtsc() & (CACHE_LINE_SZ/MAX_WP_LENGTH -1);
                                  int tmpVal = shuffleNums[idx];
                                  shuffleNums[idx] = shuffleNums[i];
                                  shuffleNums[i] = tmpVal;
                                }
                                number_of_arming++;

                                for(int i = 0; i < wpConfig.maxWP; i ++) {
                                  SampleData_t sd= {
                                    .va = cacheLineBaseAddress + (shuffleNums[i] << 3),
                                    .target_va = localSharedData.address,
                                    .node = localSharedData.node,
                                    .samplerAccessType = accessType,
                                    .accessType=localSharedData.accessType,
                                    .sampleType=sType,
                                    .type=localSharedData.wpType,
                                    .wpLength = MAX_WP_LENGTH,
                                    .accessLength= accessLen,
                                    .sampledMetricId=sampledMetricId,
                                    .isSamplePointAccurate = isSamplePointAccurate,
                                    .preWPAction=theWPConfig->preWPAction,
                                    .isBackTrace = false,
                                    .first_accessing_tid = localSharedData.tid,
                                    .first_accessing_core_id = localSharedData.core_id,
                                    .bulletinBoardTimestamp = localSharedData.time,
                                    .expirationPeriod = localSharedData.expiration_period,
				    .valid_sample_count = localSharedData.valid_sample_count
                                  };
                                  // if current WPs in T are old then
                                  // Disarm any previously armed WPs
                                  //fprintf(stderr, "watchpoint is to be armed\n"); 
                                  SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                                }
                              }
                              // end watchpoints
                            }

                            // if ( A1 is not STORE) or (entry != NULL and M2 has not expired) then
                            if(/*(accessType == LOAD)*/ (sType == ALL_LOAD)  || ((item.cacheLineBaseAddress != -1) && (me == item.tid) && ((curtime - item.time) <= (storeCurTime - storeLastTime)))) {
                            } else /*if(mmap_data->addr_valid)*/ {
                              // BulletinBoard.TryAtomicPut(key = L1 , value = < M1 , δ1 , ts1 , T1 >)
                              uint64_t bulletinCounter = bulletinBoard.counter;
                              if((bulletinCounter & 1) == 0) {
                                //bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval, ...)
                                //These builtins perform an atomic compare and swap. That is, if the current value of *ptr
                                //is oldval, then write newval into *ptr.
                                //The “bool” version returns true if the comparison is successful and newval was written.
                                if(__sync_bool_compare_and_swap(&bulletinBoard.counter, bulletinCounter, bulletinCounter+1)){
                                  struct SharedEntry inserted_item;
                                  inserted_item.time = curtime;
                                  inserted_item.tid = me;
                                  inserted_item.core_id = sched_getcpu();
                                  inserted_item.wpType = WP_RW;
                                  inserted_item.accessType = accessType;
                                  inserted_item.sampleType = sType;
                                  inserted_item.address = data_addr;
                                  inserted_item.accessLen = accessLen;
                                  inserted_item.node = node;
                                  inserted_item.cacheLineBaseAddress = cacheLineBaseAddressVar;
                                  inserted_item.prev_transfer_counter = 0;
                                  inserted_item.expiration_period = /*(lastTime == 0 ? 0 : 2 * (curtime - lastTime));*/ (storeLastTime == 0 ? 0 : (storeCurTime - storeLastTime));
				  inserted_item.valid_sample_count = valid_sample_count;
                                  int bb_flag = 0;
                                  //__sync_synchronize();
                                  hashInsertwithTime(inserted_item, storeCurTime, storeLastTime);
				  //valid_sample_count = 0;
                                  //__sync_synchronize();
				  bb_store_count++;
                                  bulletinBoard.counter++;
                                }
                              }
			      all_store_count++;
                            }
                            // ends
			    if(mmap_data->addr_valid && mmap_data->store)
				    valid_sample_count++;

                            lastTime = curtime;
                            if( sType == ALL_STORE  /*accessType == STORE || accessType == LOAD_AND_STORE*/)
                              storeLastTime = storeCurTime; 	
			}
			break;
    case WP_COMDETECTIVE: {
                            int sType = -1;
                            sample_count++;

                            if (strncmp (hpcrun_id2metric(sampledMetricId)->name,"MEM_UOPS_RETIRED:ALL_STORES",27) == 0)
                              sType = ALL_STORE;
                            else if(strncmp (hpcrun_id2metric(sampledMetricId)->name,"MEM_UOPS_RETIRED:ALL_LOADS",26) == 0)
                              sType = ALL_LOAD;
                            else sType = UNKNOWN_SAMPLE_TYPE;
                            if(accessType == LOAD_AND_STORE) {
                              if(sType == ALL_LOAD)
                                load_and_store_all_load++;
                              if(sType == ALL_STORE)
                                load_and_store_all_store++;
                            }
                            if(accessType == STORE) {
                              if(sType == ALL_STORE)
                                store_all_store++;
                            }

                            int metricId = -1;
                            const void* joinNode;
                            int joinNodeIdx = isSamplePointAccurate? E_ACCURATE_JOIN_NODE_IDX : E_INACCURATE_JOIN_NODE_IDX;

                            uint64_t curtime = rdtsc(); 

                            int64_t storeCurTime = 0;
                            if(sType == ALL_STORE /*accessType == STORE || accessType == LOAD_AND_STORE*/)
                              storeCurTime = curtime; 


                            int me = TD_GET(core_profile_trace_data.id); 
                            int current_core = sched_getcpu(); 
                            // L1 = getCacheline ( M1 )
                            void * cacheLineBaseAddressVar = (void *) ALIGN_TO_CACHE_LINE((size_t)data_addr);
                            int item_not_found = 0;
                            struct SharedEntry item;
                            do{
                              int64_t startCounter = bulletinBoard.counter;
                              if(startCounter & 1) {
                                continue;
                              }
                              //__sync_synchronize();
                              // entry = BulletinBoard.AtomicGet (key= L1 )
                              item = getEntryFromBulletinBoard(cacheLineBaseAddressVar, &item_not_found);
                              //__sync_synchronize();
                              int64_t endCounter = bulletinBoard.counter;
                              if(startCounter == endCounter) {
                                break;
                              }
                            }while(1);

                            int arm_watchpoint_flag = 0;

                            // if entry == NULL then // nothing was found related to cachelineBaseAddr in bb
                            if((item.cacheLineBaseAddress == -1) || (item_not_found == 1)) {
                              //fprintf(stderr, "not found\n");
                              // TryArmWatchpoint( T 1 )
                              arm_watchpoint_flag = 1;
                              // else
                            } else { // something was found related to cachelineBaseAddr in bb, com detected on sample
                              //fprintf(stderr, "found\n");
                              // < M2 , δ2 , ts2 , T2 > = getEntryAttributes (entry)
                              // if T1 != T2 and ts2 > tprev then
                              if((me != item.tid) && (item.time > prev_timestamp) && ((curtime - item.time) <= item.expiration_period)) {
                                int flag = 0;
                                double global_sampling_period = 0;
                                if(sType == ALL_LOAD /*accessType == LOAD*/) { // means that the sample is (read) (WAR)
                                  global_sampling_period = (double) global_load_sampling_period;
                                  flag = 1;
                                }
                                if(sType == ALL_STORE) { // means that the sample is a store type (write) (WAW)
                                  global_sampling_period = (double) global_store_sampling_period;
                                  flag = 2;
                                } 
                                int max_thread_num = item.tid; 
                                if(max_thread_num < me) 
                                {   
                                  max_thread_num = me; 
                                }
                                if(as_matrix_size < max_thread_num) 
                                { 
#if ADAMANT_USED  
                                  matrix_size_set(max_thread_num);
#endif
                                  fs_matrix_size =  max_thread_num;
                                  ts_matrix_size =  max_thread_num;
                                  as_matrix_size =  max_thread_num;  
                                }

                                int max_core_num = item.core_id;
                                if(max_core_num < current_core)
                                {
                                  max_core_num = current_core;
                                }
                                if(as_core_matrix_size < max_core_num)
                                {
#if ADAMANT_USED
                                  core_matrix_size_set(max_core_num);
#endif
                                  fs_core_matrix_size =  max_core_num;
                                  ts_core_matrix_size =  max_core_num;
                                  as_core_matrix_size =  max_core_num;
                                }
                                if(flag == 1) {  // if sType is all_loads (WAR)
                                  int id = -1;
                                  int metricId = -1;
                                  double increment = global_sampling_period; //* thread_coefficient(as_matrix_size);
                                  // if [M1 , M1 + δ1 ) overlaps with [M2 , M2 + δ2 ) the
                                  if(GET_OVERLAP_BYTES(item.address, item.accessLen, data_addr, accessLen) > 0) { //then ts
#if ADAMANT_USED
                                    if(getenv(HPCRUN_OBJECT_LEVEL)) {
                                      inc_true_matrix( (uint64_t) data_addr, item.tid, me, increment);
                                      inc_true_count((uint64_t) data_addr, increment);
                                      // before
                                      int obj_id1 = get_object_id_by_address(item.address);
                                      int obj_id2 = get_object_id_by_address(data_addr);
                                      if(obj_id1 == 0 && obj_id2 == 0) {
                                        id = get_id_after_backtrace();
                                        //fprintf(stderr, "true sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", global_sampling_period, id);
                                        inc_true_matrix_by_object_id(id, item.tid, me, increment);
                                        inc_true_count_by_object_id(id, increment);
                                      }
                                      if(obj_id1 == 1 && obj_id2 == 1) {
                                        if(id == -1)
                                          id = get_id_after_backtrace();
                                        //fprintf(stderr, "true sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", global_sampling_period, id);
                                        inc_true_matrix_by_object_id(id, item.tid, me, increment);
                                        inc_true_count_by_object_id(id, increment);
                                      }
                                      // after
                                    }
#endif
                                    // ends
                                    trueWRIns ++;
                                    metricId = true_wr_metric_id;
                                    joinNode = joinNodes[E_TRUE_WR_SHARE][joinNodeIdx];


                                    ts_matrix[item.tid][me] = ts_matrix[item.tid][me] + increment;
                                    war_ts_matrix[item.tid][me] = war_ts_matrix[item.tid][me] + increment;
                                    if(item.core_id != current_core) {
#if ADAMANT_USED
                                      if(getenv(HPCRUN_OBJECT_LEVEL)) {
                                        inc_true_core_matrix( (uint64_t) data_addr, item.core_id, current_core, increment);
                                        inc_true_core_count((uint64_t) data_addr, increment);
                                        int obj_id1 = get_object_id_by_address(item.address);
                                        int obj_id2 = get_object_id_by_address(data_addr);
                                        if(obj_id1 == 0 && obj_id2 == 0) {
                                          if(id == -1)
                                            id = get_id_after_backtrace();
                                          //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
                                          inc_true_core_matrix_by_object_id(id, item.core_id, current_core, increment);
                                          inc_true_core_count_by_object_id(id, increment);
                                        }
                                        if(obj_id1 == 1 && obj_id2 == 1) {
                                          if(id == -1)
                                            id = get_id_after_backtrace();
                                          //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
                                          inc_true_core_matrix_by_object_id(id, item.core_id, current_core, increment);
                                          inc_true_core_count_by_object_id(id, increment);
                                        }
                                      }
#endif
                                      ts_core_matrix[item.core_id][current_core] = ts_core_matrix[item.core_id][current_core] + increment;
                                      war_ts_core_matrix[item.core_id][current_core] = war_ts_core_matrix[item.core_id][current_core] + increment;        			    }
                                  } else {
                                    /*falseWWIns ++;
                                      metricId =  false_ww_metric_id;
                                      cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});*/
                                    // Record false sharing
#if ADAMANT_USED
                                    if(getenv(HPCRUN_OBJECT_LEVEL)) {
                                      inc_false_matrix( (uint64_t) item.address, (uint64_t) data_addr, item.tid, me, increment);
                                      inc_false_count((uint64_t) item.address, (uint64_t) data_addr, increment);
                                      int obj_id1 = get_object_id_by_address(item.address);
                                      int obj_id2 = get_object_id_by_address(data_addr);
                                      // debugging starts
                                      if((obj_id1 == obj_id2) && (obj_id1 == 998)) {
                                        fprintf(stderr, "false sharing is detected between threads %d and %d on address %ld and address %ld\n", item.tid, me, item.address, data_addr);
                                        //sleep(4);
                                      }
                                      // debugging ends
                                      if(obj_id1 == 0 && obj_id2 == 0) {
                                        id = get_id_after_backtrace();
                                        //fprintf(stderr, "false sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", global_sampling_period, id);
                                        inc_false_matrix_by_object_id(id, item.tid, me, increment);
                                        inc_false_count_by_object_id(id, increment);
                                      }
                                      if(obj_id1 == 1 && obj_id2 == 1) {
                                        if(id == -1)
                                          id = get_id_after_backtrace();
                                        //fprintf(stderr, "false sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", global_sampling_period, id);
                                        inc_false_matrix_by_object_id(id, item.tid, me, increment);
                                        inc_false_count_by_object_id(id, increment);
                                      }
                                    }
#endif

                                    falseWRIns ++;
                                    metricId = false_wr_metric_id;
                                    joinNode = joinNodes[E_FALSE_WR_SHARE][joinNodeIdx];

                                    fs_matrix[item.tid][me] = fs_matrix[item.tid][me] + increment;
                                    war_fs_matrix[item.tid][me] = fs_matrix[item.tid][me] + increment;
                                    if(item.core_id != current_core) {
#if ADAMANT_USED
                                      if(getenv(HPCRUN_OBJECT_LEVEL)) {
                                        inc_false_core_matrix( (uint64_t) item.address, (uint64_t) data_addr, item.core_id, current_core, increment);
                                        inc_false_core_count((uint64_t) item.address, (uint64_t) data_addr, increment);
                                        int obj_id1 = get_object_id_by_address(item.address);
                                        int obj_id2 = get_object_id_by_address(data_addr);
                                        if(obj_id1 == 0 && obj_id2 == 0) {
                                          if(id == -1)
                                            id = get_id_after_backtrace();
                                          //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
                                          inc_false_core_matrix_by_object_id(id, item.core_id, current_core, increment);
                                          inc_false_core_count_by_object_id(id, increment);
                                        }
                                        if(obj_id1 == 1 && obj_id2 == 1) {
                                          if(id == -1)
                                            id = get_id_after_backtrace();
                                          //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
                                          inc_false_core_matrix_by_object_id(id, item.core_id, current_core, increment);
                                          inc_false_core_count_by_object_id(id, increment);
                                        }
                                      }
#endif
                                      fs_core_matrix[item.core_id][current_core] = fs_core_matrix[item.core_id][current_core] + increment;
                                      war_fs_core_matrix[item.core_id][current_core] = war_fs_core_matrix[item.core_id][current_core] + increment;
                                    }
                                  }
                                  as_matrix[item.tid][me] = as_matrix[item.tid][me] + increment;
                                  war_as_matrix[item.tid][me] = war_as_matrix[item.tid][me] + increment;
                                  if(item.core_id != current_core) {
                                    as_core_matrix[item.core_id][current_core] = as_core_matrix[item.core_id][current_core] + increment;
                                    war_as_core_matrix[item.core_id][current_core] = war_as_core_matrix[item.core_id][current_core] + increment;
                                  }	
                                  // tprev = ts2
                                  prev_timestamp = item.time;

                                  //sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0, 1, NULL);
                                  // insert a special node
                                  /*cct_node_t *node1 = hpcrun_insert_special_node(node, joinNode);
                                    node1 = hpcrun_cct_insert_path_return_leaf(item.node, node1);
                                  // update the metricId
                                  cct_metric_data_increment(metricId, node1, (cct_metric_data_t){.i = 1});*/

                                }
                                else if(flag == 2) {  // if sType is all_stores (WAW)
                                  int id = -1;
                                  int metricId = -1;
                                  double increment = global_sampling_period; //* thread_coefficient(as_matrix_size);
                                  // if [M1 , M1 + δ1 ) overlaps with [M2 , M2 + δ2 ) the
                                  if(GET_OVERLAP_BYTES(item.address, item.accessLen, data_addr, accessLen) > 0) { //then ts
#if ADAMANT_USED
                                    if(getenv(HPCRUN_OBJECT_LEVEL)) {
                                      inc_true_matrix( (uint64_t) data_addr, item.tid, me, increment);
                                      inc_true_count((uint64_t) data_addr, increment);
                                      // before
                                      int obj_id1 = get_object_id_by_address(item.address);
                                      int obj_id2 = get_object_id_by_address(data_addr);
                                      if(obj_id1 == 0 && obj_id2 == 0) {
                                        id = get_id_after_backtrace();
                                        //fprintf(stderr, "true sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", global_sampling_period, id);
                                        inc_true_matrix_by_object_id(id, item.tid, me, increment);
                                        inc_true_count_by_object_id(id, increment);
                                      }
                                      if(obj_id1 == 1 && obj_id2 == 1) {
                                        if(id == -1)
                                          id = get_id_after_backtrace();
                                        //fprintf(stderr, "true sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", global_sampling_period, id);
                                        inc_true_matrix_by_object_id(id, item.tid, me, increment);
                                        inc_true_count_by_object_id(id, increment);
                                      }
                                      // after
                                    }
#endif
                                    // ends

                                    trueWWIns ++;
                                    metricId = true_ww_metric_id;
                                    joinNode = joinNodes[E_TRUE_WW_SHARE][joinNodeIdx];

                                    ts_matrix[item.tid][me] = ts_matrix[item.tid][me] + increment;
                                    waw_ts_matrix[item.tid][me] = waw_ts_matrix[item.tid][me] + increment;
                                    if(item.core_id != current_core) {
#if ADAMANT_USED
                                      if(getenv(HPCRUN_OBJECT_LEVEL)) {
                                        inc_true_core_matrix( (uint64_t) data_addr, item.core_id, current_core, increment);
                                        inc_true_core_count((uint64_t) data_addr, increment);
                                        int obj_id1 = get_object_id_by_address(item.address);
                                        int obj_id2 = get_object_id_by_address(data_addr);
                                        if(obj_id1 == 0 && obj_id2 == 0) {
                                          if(id == -1)
                                            id = get_id_after_backtrace();
                                          //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
                                          inc_true_core_matrix_by_object_id(id, item.core_id, current_core, increment);
                                          inc_true_core_count_by_object_id(id, increment);
                                        }
                                        if(obj_id1 == 1 && obj_id2 == 1) {
                                          if(id == -1)
                                            id = get_id_after_backtrace();
                                          //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
                                          inc_true_core_matrix_by_object_id(id, item.core_id, current_core, increment);
                                          inc_true_core_count_by_object_id(id, increment);
                                        }
                                      }
#endif
                                      ts_core_matrix[item.core_id][current_core] = ts_core_matrix[item.core_id][current_core] + increment;
                                      waw_ts_core_matrix[item.core_id][current_core] = waw_ts_core_matrix[item.core_id][current_core] + increment;			    }
                                  } else {
                                    /*falseWWIns ++;
                                      metricId =  false_ww_metric_id;
                                      cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});*/
                                    // Record false sharing
#if ADAMANT_USED
                                    if(getenv(HPCRUN_OBJECT_LEVEL)) {
                                      inc_false_matrix( (uint64_t) item.address, (uint64_t) data_addr, item.tid, me, increment);
                                      inc_false_count((uint64_t) item.address, (uint64_t) data_addr, increment);
                                      int obj_id1 = get_object_id_by_address(item.address);
                                      int obj_id2 = get_object_id_by_address(data_addr);
                                      // debugging starts
                                      if((obj_id1 == obj_id2) && (obj_id1 == 998)) {
                                        fprintf(stderr, "false sharing is detected between threads %d and %d on address %ld and address %ld\n", item.tid, me, item.address, data_addr);
                                        //sleep(4);
                                      }
                                      // debugging ends
                                      if(obj_id1 == 0 && obj_id2 == 0) {
                                        id = get_id_after_backtrace();
                                        //fprintf(stderr, "false sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", global_sampling_period, id);
                                        inc_false_matrix_by_object_id(id, item.tid, me, increment);
                                        inc_false_count_by_object_id(id, increment);
                                      }
                                      if(obj_id1 == 1 && obj_id2 == 1) {
                                        if(id == -1)
                                          id = get_id_after_backtrace();
                                        //fprintf(stderr, "false sharing communication is detected on an unknown object with increment %0.2lf on node %d\n", global_sampling_period, id);
                                        inc_false_matrix_by_object_id(id, item.tid, me, increment);
                                        inc_false_count_by_object_id(id, increment);
                                      }
                                    }
#endif

                                    falseWWIns ++;
                                    metricId = false_ww_metric_id;
                                    joinNode = joinNodes[E_FALSE_WW_SHARE][joinNodeIdx];

                                    fs_matrix[item.tid][me] = fs_matrix[item.tid][me] + increment;
                                    waw_fs_matrix[item.tid][me] = waw_fs_matrix[item.tid][me] + increment;
                                    if(item.core_id != current_core) {
#if ADAMANT_USED
                                      if(getenv(HPCRUN_OBJECT_LEVEL)) {
                                        inc_false_core_matrix( (uint64_t) item.address, (uint64_t) data_addr, item.core_id, current_core, increment);
                                        inc_false_core_count((uint64_t) item.address, (uint64_t) data_addr, increment);
                                        int obj_id1 = get_object_id_by_address(item.address);
                                        int obj_id2 = get_object_id_by_address(data_addr);
                                        if(obj_id1 == 0 && obj_id2 == 0) {
                                          if(id == -1)
                                            id = get_id_after_backtrace();
                                          //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
                                          inc_false_core_matrix_by_object_id(id, item.core_id, current_core, increment);
                                          inc_false_core_count_by_object_id(id, increment);
                                        }
                                        if(obj_id1 == 1 && obj_id2 == 1) {
                                          if(id == -1)
                                            id = get_id_after_backtrace();
                                          //fprintf(stderr, "communication is detected on an unknown object with increment %0.2lf on node %d\n", increment, id);
                                          inc_false_core_matrix_by_object_id(id, item.core_id, current_core, increment);
                                          inc_false_core_count_by_object_id(id, increment);
                                        }
                                      }
#endif
                                      fs_core_matrix[item.core_id][current_core] = fs_core_matrix[item.core_id][current_core] + increment;
                                      waw_fs_core_matrix[item.core_id][current_core] = waw_fs_core_matrix[item.core_id][current_core] + increment;
                                    }
                                  }
                                  as_matrix[item.tid][me] = as_matrix[item.tid][me] + increment;
                                  waw_as_matrix[item.tid][me] = waw_as_matrix[item.tid][me] + increment;
                                  if(item.core_id != current_core) {
                                    as_core_matrix[item.core_id][current_core] = as_core_matrix[item.core_id][current_core] + increment;
                                    waw_as_core_matrix[item.core_id][current_core] = waw_as_core_matrix[item.core_id][current_core] + increment;
                                  }	
                                  // tprev = ts2
                                  prev_timestamp = item.time;
                                  /*
                                     sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0, 1, NULL);
                                  // insert a special node
                                  cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
                                  node = hpcrun_cct_insert_path_return_leaf(wpi->sample.node, node);
                                  // update the metricId
                                  cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
                                   */
                                }

                                // before
                                /*
                                   sample_val_t v = hpcrun_sample_callpath(wt->ctxt, measured_metric_id, SAMPLE_UNIT_INC, 0, 1, NULL);
                                // insert a special node
                                cct_node_t *node = hpcrun_insert_special_node(v.sample_node, joinNode);
                                node = hpcrun_cct_insert_path_return_leaf(wpi->sample.node, node);
                                // update the metricId
                                cct_metric_data_increment(metricId, node, (cct_metric_data_t){.i = 1});
                                 */
                                sample_val_t v = hpcrun_sample_callpath(context, measured_metric_id, SAMPLE_UNIT_INC, 0/*skipInner*/, 1/*isSync*/, NULL);
                                cct_node_t *node1 = hpcrun_insert_special_node(v.sample_node, joinNode);
                                node1 = hpcrun_cct_insert_path_return_leaf(item.node, node1);
                                // update the metricId
                                cct_metric_data_increment(metricId, node1, (cct_metric_data_t){.i = 1});
                                // after
                              } else {
                                // TryArmWatchpoint(T1)
                                arm_watchpoint_flag = 1;
                              }
                            }

                            if (arm_watchpoint_flag) {
                              // begin watchpoints
                              int do_not_arm_watchpoint = 0;
                              // getting an unexpired address from BulletinBoard that is not from T
                              struct SharedEntry localSharedData;
                              do{ 
                                int64_t startCounter1 = bulletinBoard.counter;
                                if(startCounter1 & 1) {
                                  continue;
                                }
                                localSharedData = getEntryRandomlyFromBulletinBoard(me, curtime, &do_not_arm_watchpoint);	
                                int64_t endCounter1 = bulletinBoard.counter;
                                if(startCounter1 == endCounter1) {
                                  break;
                                }
                              }while(1);

                              if((localSharedData.cacheLineBaseAddress != -1) && !do_not_arm_watchpoint) {
                                long  metricThreshold = hpcrun_id2metric(sampledMetricId)->period;
                                accessedIns += metricThreshold;
                                void * cacheLineBaseAddress = localSharedData.cacheLineBaseAddress;
                                int shuffleNums[CACHE_LINE_SZ/MAX_WP_LENGTH] = {0, 1, 2, 3, 4, 5, 6, 7}; // hard coded
                                for(int i = 0; i < wpConfig.maxWP; i ++) {
                                  int idx = rdtsc() & (CACHE_LINE_SZ/MAX_WP_LENGTH -1);
                                  int tmpVal = shuffleNums[idx];
                                  shuffleNums[idx] = shuffleNums[i];
                                  shuffleNums[i] = tmpVal;
                                }
                                number_of_arming++;

                                for(int i = 0; i < wpConfig.maxWP; i ++) {
                                  SampleData_t sd= {
                                    .va = cacheLineBaseAddress + (shuffleNums[i] << 3),
                                    .target_va = localSharedData.address,
                                    .node = localSharedData.node,
                                    .samplerAccessType = accessType,
                                    .accessType=localSharedData.accessType,
                                    .sampleType=sType,
                                    .type=localSharedData.wpType,
                                    .wpLength = MAX_WP_LENGTH,
                                    .accessLength= accessLen,
                                    .sampledMetricId=sampledMetricId,
                                    .isSamplePointAccurate = isSamplePointAccurate,
                                    .preWPAction=theWPConfig->preWPAction,
                                    .isBackTrace = false,
                                    .first_accessing_tid = localSharedData.tid,
                                    .first_accessing_core_id = localSharedData.core_id,
                                    .bulletinBoardTimestamp = localSharedData.time,
                                    .expirationPeriod = localSharedData.expiration_period
                                  };
                                  // if current WPs in T are old then
                                  // Disarm any previously armed WPs
                                  // Set WPs on an unexpired address from BulletinBoard that is not from T
                                  //SubscribeWatchpointWithTime(&sd, OVERWRITE, false /* capture value */, curtime, lastTime);
                                  //SubscribeWatchpointWithStoreTime(&sd, OVERWRITE, false /* capture value */, curtime);
                                  SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */);
                                  //SubscribeWatchpoint(&sd, OVERWRITE, false /* capture value */); 
                                }
                              }
                              // end watchpoints
                            }

                            // if ( A1 is not STORE) or (entry != NULL and M2 has not expired) then
                            if(/*(accessType == LOAD)*/ (sType == ALL_LOAD)  || ((item.cacheLineBaseAddress != -1) && (me == item.tid) && ((curtime - item.time) <= (storeCurTime - storeLastTime)))) {
                            } else {
                              // BulletinBoard.TryAtomicPut(key = L1 , value = < M1 , δ1 , ts1 , T1 >)
                              uint64_t bulletinCounter = bulletinBoard.counter;
                              if((bulletinCounter & 1) == 0) {
                                //bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval, ...)
                                //These builtins perform an atomic compare and swap. That is, if the current value of *ptr
                                //is oldval, then write newval into *ptr.
                                //The “bool” version returns true if the comparison is successful and newval was written.
                                if(__sync_bool_compare_and_swap(&bulletinBoard.counter, bulletinCounter, bulletinCounter+1)){
                                  struct SharedEntry inserted_item;
                                  inserted_item.time = curtime;
                                  inserted_item.tid = me;
                                  inserted_item.core_id = sched_getcpu();
                                  inserted_item.wpType = WP_RW;
                                  inserted_item.accessType = accessType;
                                  inserted_item.sampleType = sType;
                                  inserted_item.address = data_addr;
                                  inserted_item.accessLen = accessLen;
                                  inserted_item.node = node;
                                  inserted_item.cacheLineBaseAddress = cacheLineBaseAddressVar;
                                  inserted_item.prev_transfer_counter = 0;
                                  inserted_item.expiration_period = (storeLastTime == 0 ? 0 : (storeCurTime - storeLastTime));
                                  int bb_flag = 0;
                                  //__sync_synchronize();
                                  hashInsertwithTime(inserted_item, storeCurTime, storeLastTime);
                                  //__sync_synchronize();
                                  bulletinBoard.counter++;
                                }
                              }
                            }
                            // ends

                            lastTime = curtime;
                            if( sType == ALL_STORE  /*accessType == STORE || accessType == LOAD_AND_STORE*/)
                              storeLastTime = storeCurTime;
                          }
    default:
                          break;
  }
  //fprintf(stderr, "here7!\n");
  wpStats.numWatchpointsSet ++;
  return true;

ErrExit:
  wpStats.numImpreciseSamples ++;
  return false;

}

void dump_profiling_metrics() {
  //#if 0
  if(theWPConfig->id == WP_AMD_COMM) {
	  //uint64_t val[3] = {0};
          //fprintf(stderr, "before assert\n");
          //fprintf(stderr, "reading counter in OnSample data_addr: %lx\n", data_addr);
          //assert(linux_perf_read_event_counter( amd_reuse_distance_event, val) >= 0);
          //fprintf(stderr, "load and store counter: %ld\n", val[0]);

	  //fprintf(stderr, "bb_store_count: %d, all_store_count: %d\n", bb_store_count, all_store_count);
	  //double scale_ratio = (double) val[0]/amd_global_sampling_period / /*micro_op_sample;*/mem_access_sample; //mem_access_sample / store_count;
	  //fprintf(stderr, "micro_op_sample: %d, mem_access_sample: %d, valid_mem_access_sample: %d, sample_count: %d, original_sample_count: %d, store_count: %d, scale_ratio: %0.2lf\n", micro_op_sample, mem_access_sample, valid_mem_access_sample, sample_count, original_sample_count, store_count, scale_ratio);
	  //fprintf(stderr, "micro_op_sample: %d, mem_access_sample: %d, valid_mem_access_sample: %d, sample_count: %d, original_sample_count: %d, store_count: %d, global_load_count: %d\n", micro_op_sample, mem_access_sample, valid_mem_access_sample, sample_count, original_sample_count, store_count, global_load_count);
	  //adjust_communication_volume(scale_ratio);
  }
//#endif
  if(theWPConfig->id == WP_COMDETECTIVE || theWPConfig->id == WP_AMD_COMM) {
    dump_fs_matrix();
    dump_fs_core_matrix();
    dump_ts_matrix();
    dump_ts_core_matrix();
    dump_as_matrix();
    dump_as_core_matrix();
    dump_war_fs_matrix();
    dump_war_fs_core_matrix();
    dump_war_ts_matrix();
    dump_war_ts_core_matrix();
    dump_war_as_matrix();
    dump_war_as_core_matrix();
    dump_waw_fs_matrix();
    dump_waw_fs_core_matrix();
    dump_waw_ts_matrix();
    dump_waw_ts_core_matrix();
    dump_waw_as_matrix();
    dump_waw_as_core_matrix();
  } 
  if(theWPConfig->id == WP_REUSETRACKER) {
#ifdef REUSE_HISTO
    uint64_t val[3];
    //fprintf(stderr, "FINAL_COUNTING:");
    if (reuse_output_trace == false){ //dump the bin info
      //fprintf(stderr, "the bin info is dumped\n");
      //fprintf(stderr, "inter_thread_invalidation_count: %ld\n", inter_thread_invalidation_count);
      //fprintf(stderr, "inter_core_invalidation_count: %ld\n", inter_core_invalidation_count);
      WriteWitchTraceOutput("BIN_START: %lf\n", reuse_bin_start);
      WriteWitchTraceOutput("BIN_RATIO: %lf\n", reuse_bin_ratio);

      for(int i=0; i < reuse_bin_size; i++){
        WriteWitchTraceOutput("BIN: %d %lu\n", i, reuse_bin_list[i]);
      }
    }

    //fprintf(stderr, "inter_thread_invalidation_count: %ld\n", inter_thread_invalidation_count);
    //fprintf(stderr, "inter_core_invalidation_count: %ld\n", inter_core_invalidation_count);
    WriteWitchTraceOutput("COHERENCE_MISS:");
    WriteWitchTraceOutput(" %ld\n", l3_coherence_miss_count);
    WriteWitchTraceOutput("FINAL_COUNTING:");
    for (int i=0; i < MIN(2,reuse_distance_num_events); i++){
      assert(linux_perf_read_event_counter(reuse_distance_events[i], val) >= 0);
      //fprintf(stderr, " %lu %lu %lu,", val[0], val[1], val[2]);//jqswang
      WriteWitchTraceOutput(" %lu %lu %lu,", val[0], val[1], val[2]);
    }
    //fprintf(stderr, "\n");
    WriteWitchTraceOutput("\n");
    //close the trace output
    CloseWitchTraceOutput();

    char file_name[PATH_MAX];
    int ret = snprintf(file_name, PATH_MAX, "%s-%u.shared.reuse.hpcrun", hpcrun_files_executable_name(), syscall(SYS_gettid));
    FILE * fp;
    fp = fopen (file_name, "w+");
    fprintf(fp, "BIN_START: %lf\n", reuse_bin_start); 
    fprintf(fp, "BIN_RATIO: %lf\n", reuse_bin_ratio);
    for(int i=0; i < shared_reuse_bin_size; i++){
      fprintf(fp, "BIN: %d %lu\n", i, shared_reuse_bin_list[i]);
    }
    fprintf(fp, "COHERENCE_MISS:");
    fprintf(fp, " %ld\n", l3_coherence_miss_count); 
    //fprintf(stderr, "\n");
    fprintf(fp, "\n");
    fclose(fp);
#endif 
    ret = snprintf(file_name, PATH_MAX, "%s-%u.communication.reuse.hpcrun", hpcrun_files_executable_name(), syscall(SYS_gettid));
    FILE * fp1;
    fp1 = fopen (file_name, "w+");
    fprintf(fp1, "BIN_START: %lf\n", reuse_bin_start);
    fprintf(fp1, "BIN_RATIO: %lf\n", reuse_bin_ratio);
    for(int i=0; i < communication_reuse_bin_size; i++){
      fprintf(fp1, "BIN: %d %lu\n", i, communication_reuse_bin_list[i]);
    }
    fprintf(fp1, "COHERENCE_MISS:");
    fprintf(fp1, " %ld\n", l3_coherence_miss_count);
    //fprintf(stderr, "\n");
    fprintf(fp1, "\n");
    fclose(fp1);

  }

  if (theWPConfig->id == WP_AMD_REUSE || theWPConfig->id == WP_AMD_REUSETRACKER) {
	#ifdef REUSE_HISTO
    uint64_t val[3];
    //fprintf(stderr, "FINAL_COUNTING:");
    if (reuse_output_trace == false){ //dump the bin info
      //fprintf(stderr, "the bin info is dumped\n");
      //fprintf(stderr, "inter_thread_invalidation_count: %ld\n", inter_thread_invalidation_count);
      //fprintf(stderr, "inter_core_invalidation_count: %ld\n", inter_core_invalidation_count);
      WriteWitchTraceOutput("BIN_START: %lf\n", reuse_bin_start);
      WriteWitchTraceOutput("BIN_RATIO: %lf\n", reuse_bin_ratio);

      for(int i=0; i < reuse_bin_size; i++){
        WriteWitchTraceOutput("BIN: %d %lu\n", i, reuse_bin_list[i]);
      }
    }

    //fprintf(stderr, "inter_thread_invalidation_count: %ld\n", inter_thread_invalidation_count);
    //fprintf(stderr, "inter_core_invalidation_count: %ld\n", inter_core_invalidation_count);
    WriteWitchTraceOutput("COHERENCE_MISS:");
    WriteWitchTraceOutput(" %ld\n", l3_coherence_miss_count);
    WriteWitchTraceOutput("FINAL_COUNTING:");
    //for (int i=0; i < MIN(2,reuse_distance_num_events); i++){
    assert(linux_perf_read_event_counter(amd_reuse_distance_event, val) >= 0);
      //fprintf(stderr, " %lu %lu %lu,", val[0], val[1], val[2]);//jqswang
    WriteWitchTraceOutput(" %lu %lu %lu,", val[0], val[1], val[2]);
    //}
    //fprintf(stderr, "\n");
    WriteWitchTraceOutput("\n");
    //close the trace output
    CloseWitchTraceOutput();

    char file_name[PATH_MAX];
    int ret = snprintf(file_name, PATH_MAX, "%s-%u.shared.reuse.hpcrun", hpcrun_files_executable_name(), syscall(SYS_gettid));
    FILE * fp;
    fp = fopen (file_name, "w+");
    fprintf(fp, "BIN_START: %lf\n", reuse_bin_start);
    fprintf(fp, "BIN_RATIO: %lf\n", reuse_bin_ratio);
    for(int i=0; i < shared_reuse_bin_size; i++){
      fprintf(fp, "BIN: %d %lu\n", i, shared_reuse_bin_list[i]);
    }
    fprintf(fp, "COHERENCE_MISS:");
    fprintf(fp, " %ld\n", l3_coherence_miss_count);
    //fprintf(stderr, "\n");
    fprintf(fp, "\n");
    fclose(fp);
#endif
    ret = snprintf(file_name, PATH_MAX, "%s-%u.communication.reuse.hpcrun", hpcrun_files_executable_name(), syscall(SYS_gettid));
    FILE * fp1;
    fp1 = fopen (file_name, "w+");
    fprintf(fp1, "BIN_START: %lf\n", reuse_bin_start);
    fprintf(fp1, "BIN_RATIO: %lf\n", reuse_bin_ratio);
    for(int i=0; i < communication_reuse_bin_size; i++){
      fprintf(fp1, "BIN: %d %lu\n", i, communication_reuse_bin_list[i]);
    }
    fprintf(fp1, "COHERENCE_MISS:");
    fprintf(fp1, " %ld\n", l3_coherence_miss_count);
    //fprintf(stderr, "\n");
    fprintf(fp1, "\n");
    fclose(fp1);

  }
}


