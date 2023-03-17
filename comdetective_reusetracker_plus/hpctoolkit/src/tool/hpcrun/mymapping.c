#include <omp.h>
#define _GNU_SOURCE
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#include "env.h"

int mapping_vector[1024];

int mapping_size;

void
hpcrun_set_thread_mapping ()
{
	int sum = 0;

	char *string = getenv(HPCRUN_THREAD_MAPPING);

	mapping_size = 0;

	if(string != NULL) {
		while (1) {
			char *tail;
			int next;

			/* Skip whitespace by hand, to detect the end.  */
			while ((*string) == ',') string++;
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
				mapping_vector[mapping_size++] = next;
				//printf("%d ", next);
			}
			/* Advance past it.  */
			string = tail;
		}
		//printf("\n");
	}
}

int 
stick_this_thread_to_core(int core_id) {
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (core_id < 0 || core_id >= num_cores)
		return EINVAL;

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);

	pthread_t current_thread = pthread_self();    
	return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}
