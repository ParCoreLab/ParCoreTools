#include <sched.h>

extern int mapping_vector[1024];
extern int mapping_size;
int stick_this_thread_to_core(int core_id);
void hpcrun_set_thread_mapping();
int sched_getcpu(void);
