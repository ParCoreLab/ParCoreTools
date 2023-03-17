typedef struct reuseBBEntry{
  uint64_t time __attribute__((aligned(CACHE_LINE_SZ)));
  int tid;
  int core_id;
  AccessType accessType;
  void *address;
  void *cacheLineBaseAddress;
  int accessLen;
  cct_node_t * node;
  uint64_t pmu_counter;
  uint64_t eventCountBetweenSamples;
  uint64_t timeBetweenSamples;
  char dummy[CACHE_LINE_SZ];
} ReuseBBEntry_t;

typedef struct reuseHashTableStruct{
  volatile uint64_t counter __attribute__((aligned(64)));
  struct reuseBBEntry hashTable[503];
  //struct SharedData * hashTable;
} ReuseHashTable_t;

extern ReuseHashTable_t reuseBulletinBoard;
