#include <stdint.h>
#include "sample-sources/watchpoint_support.h"

extern int HASHTABLESIZE;
extern int fs_matrix_size;
extern int ts_matrix_size;
extern int as_matrix_size;

extern int fs_core_matrix_size;
extern int ts_core_matrix_size;
extern int as_core_matrix_size;
extern int invalidation_core_matrix_size;

extern int max_consecutive_count;

extern double fs_matrix[2000][2000];
extern double ts_matrix[2000][2000];
extern double as_matrix[2000][2000];
extern double invalidation_matrix[2000][2000];

extern double fs_core_matrix[2000][2000];
extern double ts_core_matrix[2000][2000];
extern double as_core_matrix[2000][2000];
extern double invalidation_core_matrix[2000][2000];

extern double war_fs_matrix[2000][2000];
extern double war_ts_matrix[2000][2000];
extern double war_as_matrix[2000][2000];

extern double war_fs_core_matrix[2000][2000];
extern double war_ts_core_matrix[2000][2000];
extern double war_as_core_matrix[2000][2000];

extern double waw_fs_matrix[2000][2000];
extern double waw_ts_matrix[2000][2000];
extern double waw_as_matrix[2000][2000];

extern double waw_fs_core_matrix[2000][2000];
extern double waw_ts_core_matrix[2000][2000];
extern double waw_as_core_matrix[2000][2000];

extern int ** matrix;

extern long global_store_sampling_period;
extern long global_load_sampling_period;

extern long number_of_traps;

// before
extern __thread long number_of_sample;
extern __thread long number_of_load_sample;
extern __thread long number_of_store_sample;
extern __thread long number_of_load_store_sample;
extern __thread long number_of_load_store_sample_all_loads;
extern __thread long number_of_load_store_sample_all_stores;
extern __thread long number_of_arming;
extern __thread long number_of_residues;
extern __thread long number_of_caught_traps;
extern __thread long number_of_caught_read_traps;
extern __thread long number_of_caught_write_traps;
extern __thread long number_of_caught_read_write_traps;
extern __thread long number_of_bulletin_board_updates;
extern __thread long number_of_bulletin_board_updates_before;

extern int consecutive_access_count_array[50];
extern int consecutive_wasted_trap_array[50];
// after

void dump_fs_matrix();
void dump_ts_matrix();
void dump_as_matrix();
void dump_invalidation_matrix();

void dump_fs_core_matrix();
void dump_ts_core_matrix();
void dump_as_core_matrix();
void dump_invalidation_core_matrix();

void dump_war_fs_matrix();
void dump_war_ts_matrix();
void dump_war_as_matrix();

void dump_war_fs_core_matrix();
void dump_war_ts_core_matrix();
void dump_war_as_core_matrix();

void dump_waw_fs_matrix();
void dump_waw_ts_matrix();
void dump_waw_as_matrix();

void dump_waw_fs_core_matrix();
void dump_waw_ts_core_matrix();
void dump_waw_as_core_matrix();
void adjust_communication_volume(double scale_ratio);

void dump_matrix();

// comdetective stats
extern double fs_volume;
extern double fs_core_volume;
extern double ts_volume;
extern double ts_core_volume;
extern double as_volume;
extern double as_core_volume;
extern double invalidation_volume;
extern double invalidation_core_volume;
extern double cache_line_transfer;
extern double cache_line_transfer_millions;
extern double cache_line_transfer_gbytes;

extern double war_fs_volume;
extern double war_fs_core_volume;
extern double war_ts_volume;
extern double war_ts_core_volume;
extern double war_as_volume;
extern double war_as_core_volume;
extern double war_cache_line_transfer;
extern double war_cache_line_transfer_millions;
extern double war_cache_line_transfer_gbytes;

extern double waw_fs_volume;
extern double waw_fs_core_volume;
extern double waw_ts_volume;
extern double waw_ts_core_volume;
extern double waw_as_volume;
extern double waw_as_core_volume;
extern double waw_cache_line_transfer;
extern double waw_cache_line_transfer_millions;
extern double waw_cache_line_transfer_gbytes;

typedef struct SharedEntry{
	volatile uint64_t counter __attribute__((aligned(CACHE_LINE_SZ)));
	uint64_t time __attribute__((aligned(CACHE_LINE_SZ)));
	int64_t expiration_period;
	int tid;
	int core_id;
	long prev_transfer_counter;
	WatchPointType wpType;
	AccessType accessType;
	SampleType sampleType;
	void *address;
	void *cacheLineBaseAddress; 
	int accessLen;
	int valid_sample_count;
	cct_node_t * node;
	volatile uint64_t matrix_counter __attribute__((aligned(CACHE_LINE_SZ))); 
	char dummy[CACHE_LINE_SZ];
} SharedEntry_t;

typedef struct hashTableStruct{
	volatile uint64_t counter __attribute__((aligned(64)));
	struct SharedEntry hashTable[HASH_TABLE_SIZE];
	//struct SharedData * hashTable;
} HashTable_t;

extern HashTable_t bulletinBoard;
