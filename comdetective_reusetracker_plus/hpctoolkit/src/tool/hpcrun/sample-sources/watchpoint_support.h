//
//  WatchPointDriver.cpp
//
//
//  Created by Milind Chabbi on 2/21/17.
//
//
#ifndef __WP_SUPPORT__
#define __WP_SUPPORT__

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <asm/unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <linux/kernel.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "common.h"
#include <hpcrun/main.h>
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/write_data.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/memory/mmap.h>
#include <monitor.h>

#include <hpcrun/cct/cct.h>
#include <hpcrun/metrics.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>
#include <unwind/common/unwind.h>
#include <hpcrun/sample-sources/perf/perf-util.h>


#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX_WP_LENGTH (8L)
#define CACHE_LINE_SZ (64)
#define ALIGN_TO_CACHE_LINE(addr) ((uint64_t)(addr) & (~(CACHE_LINE_SZ-1)))
#define IS_WP_CLIENT_ENABLED(wpi, idx) ((wpi)->isClientRequested[(idx)])
#define WP_BASE_ADDRESS(a) (uintptr_t)((size_t)(a) & (~(MAX_WP_LENGTH - 1)))
#define WP_OFFSET(a) ((size_t)(a) & (MAX_WP_LENGTH - 1))
#define GET_OVERLAP_BYTES(a, a_len, b, b_len) ((a) >= (b)? MIN(a_len, (int64_t)(b_len) - ((int64_t)(a)-(int64_t)(b))) : MIN(b_len, (int64_t)(a_len)-((int64_t)(b)-(int64_t)(a)) ))
#define FIRST_OVERLAPPED_BYTE_OFFSET_IN_FIRST(a, a_len, b, b_len) ((a) >= (b)? (0) : (b-a))
#define FIRST_OVERLAPPED_BYTE_OFFSET_IN_SECOND(a, a_len, b, b_len) ((b) >= (a)? (0) : (a-b))
#define IS_4_BYTE_ALIGNED(addr) (!((size_t)(addr) & (3)))
#define IS_8_BYTE_ALIGNED(addr) (!((size_t)(addr) & (7)))

#define WASTE_THRESHOLD (10)
//#define FINE_GRAINED_WP

typedef enum AccessType {LOAD, STORE, LOAD_AND_STORE, UNKNOWN} AccessType;
typedef enum SampleType {ALL_LOAD, ALL_STORE, UNKNOWN_SAMPLE_TYPE} SampleType;
typedef enum FunctionType {SAME_FN, DIFF_FN, UNKNOWN_FN} FunctionType;
typedef enum FloatType {ELEM_TYPE_FLOAT16, ELEM_TYPE_SINGLE, ELEM_TYPE_DOUBLE, ELEM_TYPE_LONGDOUBLE, ELEM_TYPE_LONGBCD, ELEM_TYPE_UNKNOWN} FloatType;
typedef enum WatchPointType {WP_READ, WP_WRITE, WP_RW, WP_INVALID } WatchPointType;
typedef enum ReplacementPolicy {AUTO, EMPTY_SLOT_ONLY, OLDEST, NEWEST, RDX} ReplacementPolicy;
typedef enum MergePolicy {AUTO_MERGE, NO_MERGE, CLIENT_ACTION} MergePolicy;
typedef enum OverwritePolicy {OVERWRITE, NO_OVERWRITE} OverwritePolicy;
typedef enum VictimType {EMPTY_SLOT, NON_EMPTY_SLOT, NONE_AVAILABLE} VictimType;
typedef enum WPTriggerActionType {DISABLE_WP, ALREADY_DISABLED, DISABLE_ALL_WP, RETAIN_WP} WPTriggerActionType;
typedef enum ReuseType { REUSE_TEMPORAL, REUSE_SPATIAL, REUSE_BOTH, REUSE_CACHELINE, L3_REUSE_TEMPORAL, L3_REUSE_SPATIAL, REUSE_NONE} ReuseType;
typedef enum ProfilingType { L1, L3, MIXED} ProfilingType;

// Data structure that is given by clients to set a WP
typedef struct SampleData{
	void * va; // access virtual address
	void * target_va; // access virtual address
	int wpLength; // wp length
	int accessLength; // access length
	int numFSLocs;
	int sampledMetricId;
	int first_accessing_tid;
	int first_accessing_core_id;
	int valid_sample_count;
	uint64_t bulletinBoardTimestamp;
	uint64_t prevStoreAccess;
	uint64_t expirationPeriod;
	AccessType accessType; // load or store
	AccessType samplerAccessType;
	SampleType sampleType;
	union {
		void * node;
		cct_addr_t * bt;
	};
	WatchPointType type;
	void * dataObject;
	WPTriggerActionType preWPAction;
	bool isSamplePointAccurate;
	bool isBackTrace;
	bool L1Sample;
	bool L3LoadUse;
	bool L3StoreUse;
	ReuseType reuseType;
	uint64_t reuseDistance[2][3];
	uint64_t sharedReuseDistance[2][3];
	uint64_t sampleTime;
	int L3Id;
	int L2Id;
} SampleData_t;

typedef struct WatchPointInfo{
	SampleData_t sample;

	void * va; // access virtual address
	void * cacheline_va;
	int64_t startTime;
	int fileHandle;
	bool isActive;
	uint8_t value[MAX_WP_LENGTH]; // value
	void * mmapBuffer;
	uint64_t bulletinBoardTimestamp;
} WatchPointInfo_t;

// Data structure that is captured when a WP triggers
typedef struct WatchPointTrigger{
	void * va;
	void * ctxt;
	void * pc;
	FloatType floatType;
	AccessType accessType;
	int accessLength; // access length
	int location;
} WatchPointTrigger_t;

// Data structure that is maintained per WP armed

typedef struct WPConfig {
	bool dontFixIP;
	bool dontDisassembleWPAddress;
	bool isLBREnabled;
	bool isWPModifyEnabled;
	bool getFloatType;
	bool cachelineInvalidation;
	int signalDelivered;
	size_t pgsz;
	ReplacementPolicy replacementPolicy;
	int maxWP;
} WPConfig_t;

extern WPConfig_t wpConfig;

typedef enum WP_CLIENT_ID{
  WP_DEADSPY,
  WP_REDSPY,
  WP_LOADSPY,
  WP_REUSE,
  WP_REUSETRACKER,
  WP_AMD_COMM,
  WP_AMD_REUSE,
  WP_AMD_REUSETRACKER,
  WP_TEMPORAL_REUSE,
  WP_SPATIAL_REUSE,
  WP_FALSE_SHARING,
  WP_COMDETECTIVE,
  WP_ALL_SHARING,
  WP_TRUE_SHARING,
  WP_IPC_FALSE_SHARING,
  WP_IPC_TRUE_SHARING,
  WP_IPC_ALL_SHARING,
  WP_MAX_CLIENTS }WP_CLIENT_ID;

typedef struct MonitoredNodeStruct{
        uint64_t timestamp;
        uint64_t trap_timestamp;
        uint64_t counter;
        int metricId;
	int tid;
        bool self_trap; 
} MonitoredNodeStruct_t;

typedef WPTriggerActionType (*WatchPointUpCall_t)(WatchPointInfo_t *wpi, int startOffset, int safeAccessLen, WatchPointTrigger_t * wt);
typedef void (*ClientConfigOverrideCall_t)(void *);
extern void WatchpointThreadInit();
extern void WatchpointThreadTerminate();
extern WatchPointInfo_t * getWPI  (int me, int location);
extern bool SubscribeWatchpoint(SampleData_t * sampleData, OverwritePolicy overwritePolicy, bool captureValue);
extern bool SubscribeWatchpointAlwaysReplace(SampleData_t * sampleData, OverwritePolicy overwritePolicy, bool captureValue);
extern bool SubscribeWatchpointWithTime(SampleData_t * sampleData, OverwritePolicy overwritePolicy, bool captureValue, uint64_t curTime, uint64_t lastTime);
extern bool SubscribeWatchpointWithStoreTime(SampleData_t * sampleData, OverwritePolicy overwritePolicy, bool captureValue, uint64_t curTime);
extern bool OnSample(perf_mmap_data_t * mmap_data, void * contextPC, cct_node_t *node, int sampledMetricId);
extern bool ArmWatchPointProb(int * location, uint64_t sampleTime, int me);
extern bool IsAltStackAddress(void *addr);
extern bool IsFSorGS(void *addr);
extern double ProportionOfWatchpointAmongOthersSharingTheSameContext(WatchPointInfo_t *wpi);

extern void ReuseWPConfigOverride(void*);
extern void TemporalReuseWPConfigOverride(void*);
extern void SpatialReuseWPConfigOverride(void*);
extern void FalseSharingWPConfigOverride(void*);
extern void TrueSharingWPConfigOverride(void*);
extern void AllSharingWPConfigOverride(void*);
extern void ComDetectiveWPConfigOverride(void*);
extern void AMDCommWPConfigOverride(void*);
extern void AMDReuseWPConfigOverride(void*);
extern void AMDReuseTrackerWPConfigOverride(void*);
extern void IPCFalseSharingWPConfigOverride(void*);
extern void IPCTrueSharingWPConfigOverride(void*);
extern void IPCAllSharingWPConfigOverride(void*);
extern void RedSpyWPConfigOverride(void *v);
extern void LoadSpyWPConfigOverride(void *v);
extern bool WatchpointClientActive();
//extern inline uint64_t GetWeightedMetricDiffAndReset(cct_node_t * ctxtNode, int pebsMetricId, double proportion);
extern void DisableWatchpointWrapper(WatchPointInfo_t *wpi);
extern bool getEntryFromAccessTypeLengthCache(void * pc, uint32_t *accessLen, AccessType *accessType);
extern void insertEntryToAccessTypeLengthCache(void * pc, uint32_t accessLen, AccessType accessType);

static inline  uint64_t rdtsc(){
	unsigned int lo,hi;
	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32) | lo;
}

#define MAX_WP_SLOTS (5)
#define CACHE_LINE_SIZE (64)

typedef struct globalReuseEntry{
  volatile uint64_t counter __attribute__((aligned(CACHE_LINE_SZ)));
  uint64_t time;
  int tid;
  int monitored_tid;
  int sampledMetricId;
  bool active;
  bool sharedActive;
  //bool first_coherence_miss;
  //bool trap_just_happened;
  bool self_trap;
  //uint64_t rd;
  int node_id;
  uint64_t inc;
  uint64_t rd;
  bool is_rar;
  cct_node_t *reusePairNode;
  cct_node_t *commReusePairNode;
  //uint64_t sampleCountInNode;
  //uint64_t residueSampleCountInPrevThread;
  //int residueSampleCountInPrevOwner[2];
  char dummy[CACHE_LINE_SZ];
} globalReuseEntry_t;

typedef struct globalReuseTable{
  volatile uint64_t counter __attribute__((aligned(CACHE_LINE_SZ)));
  struct globalReuseEntry table[MAX_WP_SLOTS];
  //struct SharedData * hashTable;
} globalReuseTable_t;

#define HASH_TABLE_SIZE 503

typedef struct accessTypeLengthEntry{
  volatile uint64_t counter __attribute__((aligned(CACHE_LINE_SZ)));
  void * pc;
  AccessType accessType;  
  int accessLength; 
  char dummy[CACHE_LINE_SZ];
} accessTypeLengthEntry_t;

typedef struct accessTypeLengthTable{
  struct accessTypeLengthEntry table[HASH_TABLE_SIZE];
} accessTypeLengthTable_t;

#define CHANGE_THRESHOLD 50
#define L2_MISS_RATIO_PERIOD 50

#endif //__WP_SUPPORT__
