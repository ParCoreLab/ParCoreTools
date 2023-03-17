// technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2017, Rice University
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
// Linux perf sample source interface
//


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include <sys/syscall.h> 
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

/******************************************************************************
 * linux specific headers
 *****************************************************************************/
#include <linux/perf_event.h>
#include <linux/version.h>


/******************************************************************************
 * libmonitor
 *****************************************************************************/
#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "sample-sources/simple_oo.h"
#include "sample-sources/sample_source_obj.h"
#include "sample-sources/common.h"
#include "sample-sources/watchpoint_support.h"

#include <hpcrun/cct_insert_backtrace.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample-sources/blame-shift/blame-shift.h>
#include <hpcrun/utilities/tokenize.h>
#include <hpcrun/utilities/arch/context-pc.h>
#include <hpcrun/matrix.h>
#include <hpcrun/mymapping.h>

#include <evlist.h>

#include <lib/prof-lean/hpcrun-metric.h> // prefix for metric helper

#include <include/linux_info.h> 

#ifdef ENABLE_PERFMON
#include "perfmon-util.h"
#endif

#include "perf-util.h"        // u64, u32 and perf_mmap_data_t
#include "amd_support.h"
#include "perf_mmap.h"        // api for parsing mmapped buffer
#include "event_custom.h"     // api for pre-defined events

#include "sample-sources/display.h" // api to display available events

#include "kernel_blocking.h"  // api for predefined kernel blocking event

int sched_getcpu(void);

//******************************************************************************
// macros
//******************************************************************************

#define LINUX_PERF_DEBUG 0

// default the number of samples per second
// linux perf tool has default of 4000. It looks very high but
// visually the overhead is still small for them.
// however, for some machines, the overhead is significant, and
//  somehow it causes the kernel to adjust the period threshold to
//  less than 100.
// 300 samples per sec looks has relatively similar percentage
// with perf tool
#define DEFAULT_THRESHOLD  300

#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id  _sigev_un._tid
#endif

// replace SIGIO with SIGRTMIN to support multiple events
// We know that:
// - realtime uses SIGRTMIN+3
// - PAPI uses SIGRTMIN+2
// so SIGRTMIN+4 is a safe bet (temporarily)
#define PERF_SIGNAL (SIGRTMIN+4)
#define SIGNEW 44

#define PERF_EVENT_AVAILABLE_UNKNOWN 0
#define PERF_EVENT_AVAILABLE_NO      1
#define PERF_EVENT_AVAILABLE_YES     2

#define RAW_NONE        0
#define RAW_IBS_FETCH   1
#define RAW_IBS_OP      2

#define PERF_MULTIPLEX_RANGE 1.2

#define PATH_KERNEL_KPTR_RESTICT    "/proc/sys/kernel/kptr_restrict"
#define PATH_KERNEL_PERF_PARANOID   "/proc/sys/kernel/perf_event_paranoid"

// data structures for amd begins

int n_op_samples[1024];
int n_lost_op_samples[1024];
int in_hpctoolkit_ibs[1024];

int op_cnt_max_to_set = 0;
int buffer_size = 0;
__thread char *global_buffer = NULL;
__thread int original_sample_count = 0;
__thread long ibs_count = 0;

// ends

extern int global_thread_count;
extern int dynamic_global_thread_count;
extern long global_l2_miss_sampling_period;
extern int l3_reuse_distance_event_rqsts;
extern int amd_reuse_distance_event;
//extern int amd_micro_op_event;
int ibs_event = -1;
bool amd_ibs_flag = false;
//******************************************************************************
// type declarations
//******************************************************************************

enum threshold_e { PERIOD, FREQUENCY };
struct event_threshold_s {
	long             threshold_num;
	enum threshold_e threshold_type;
};

//******************************************************************************
// forward declarations 
//******************************************************************************

static int
restart_perf_event(int fd);

static int
ibs_restart_perf_event(int fd);

static bool 
perf_thread_init(event_info_t *event, event_thread_t *et);

static void 
perf_thread_fini(int nevents, event_thread_t *event_thread);

static int 
perf_event_handler( int sig, siginfo_t* siginfo, void* context);

static int
sig_event_handler( int sig, siginfo_t* siginfo, void* context);
//******************************************************************************
// constants
//******************************************************************************

#ifndef ENABLE_PERFMON
static const char *event_name = "CPU_CYCLES";
#endif

//******************************************************************************
// local variables
//******************************************************************************

static sigset_t sig_mask;

// a list of main description of events, shared between threads
// once initialize, this list doesn't change (but event description can change)
static event_info_t  *event_desc = NULL;

static struct event_threshold_s default_threshold = {DEFAULT_THRESHOLD, FREQUENCY};


/******************************************************************************
 * external thread-local variables
 *****************************************************************************/
extern __thread bool hpcrun_thread_suppress_sample;

event_thread_t *event_thread_board[HASH_TABLE_SIZE];

//******************************************************************************
// private operations 
//******************************************************************************


/*
 * Enable all the counters
 */ 
	static void
perf_start_all(int nevents, event_thread_t *event_thread)
{
	int i;
	for(i=0; i<nevents; i++) {
		//ioctl(event_thread[i].fd, PERF_EVENT_IOC_ENABLE, 0);
		if(hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP") && event_thread[i].fd >= 0){
                        ioctl(event_thread[i].fd, IBS_ENABLE);
			//ioctl(event_thread[i].fd, IBS_CTL_RELOAD);
                        //fprintf(stderr, "fd: %d is disabled\n", event_thread[i].fd);
                }
                else
                        ioctl(event_thread[i].fd, PERF_EVENT_IOC_ENABLE, 0);
	}
}

/*
 * Disable all the counters
 */ 
	static void
perf_stop_all(int nevents, event_thread_t *event_thread)
{
	int i;
	char filename[64];
	//sprintf(filename, "/dev/cpu/%d/ibs/op", TD_GET(core_profile_trace_data.id));
	//int fd = open(filename, O_RDONLY | O_NONBLOCK);
	for(i=0; i<nevents; i++) {
		//fprintf(stderr, "event %d is closed\n", i);
		//fprintf(stderr, "event %s is to be closed\n", event_thread[i].event->metric_desc->name);
		if(/*amd_ibs_flag*/hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP") && event_thread[i].fd >= 0){
			ioctl(event_thread[i].fd, IBS_DISABLE);
			//fprintf(stderr, "IBS_OP is disabled\n");
			//fprintf(stderr, "fd: %d is disabled\n", event_thread[i].fd);
		}
		else {
			//fprintf(stderr, "event %s has been disabled\n", event_thread[i].event->metric_desc->name);
			ioctl(event_thread[i].fd, PERF_EVENT_IOC_DISABLE, 0);
		}
	}
}

static void
ibs_ctl_backup(int nevents, event_thread_t *event_thread)
{
        int i;
        char filename[64];
        //sprintf(filename, "/dev/cpu/%d/ibs/op", TD_GET(core_profile_trace_data.id));
        //int fd = open(filename, O_RDONLY | O_NONBLOCK);
        for(i=0; i<nevents; i++) {
                //fprintf(stderr, "event %d is closed\n", i);
                //fprintf(stderr, "event %s is to be closed\n", event_thread[i].event->metric_desc->name);
                if(/*amd_ibs_flag*/hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP") && event_thread[i].fd >= 0){
			ibs_count = ioctl(event_thread[i].fd, GET_CUR_CNT);
                        //ioctl(event_thread[i].fd, IBS_CTL_BACKUP);
                        //fprintf(stderr, "IBS_OP is disabled\n");
                        //fprintf(stderr, "fd: %d is disabled\n", event_thread[i].fd);
                } 
        }
}

static void
ibs_ctl_reload(int nevents, event_thread_t *event_thread)
{
        int i;
        char filename[64];
        //sprintf(filename, "/dev/cpu/%d/ibs/op", TD_GET(core_profile_trace_data.id));
        //int fd = open(filename, O_RDONLY | O_NONBLOCK);
        for(i=0; i<nevents; i++) {
                //fprintf(stderr, "event %d is closed\n", i);
                //fprintf(stderr, "event %s is to be closed\n", event_thread[i].event->metric_desc->name);
                if(/*amd_ibs_flag*/hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP") && event_thread[i].fd >= 0){
                        //ioctl(event_thread[i].fd, IBS_CTL_RELOAD);
			//long temp = ioctl(event_thread[i].fd, GET_CUR_CNT);
			//fprintf(stderr, "value of counter in register before reload: %ld\n", temp);
			ioctl(event_thread[i].fd, SET_CUR_CNT, ibs_count);
			//temp = ioctl(event_thread[i].fd, GET_CUR_CNT);
			//fprintf(stderr, "value of counter in register after reload: %ld\n", temp);
                        //fprintf(stderr, "IBS_OP is disabled\n");
                        //fprintf(stderr, "fd: %d is disabled\n", event_thread[i].fd);
                }
	}
}


//----------------------------------------------------------
// initialization
//----------------------------------------------------------

	static void 
perf_init()
{
	perf_mmap_init();

	// initialize mask to block PERF_SIGNAL 
	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, PERF_SIGNAL);

	// Setup the signal handler
	sigset_t block_mask;
	sigfillset(&block_mask);

	struct sigaction sa1 = {
		.sa_sigaction = perf_event_handler,
		.sa_mask = block_mask,
		//.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER | SA_ONSTACK
		.sa_flags =  SA_ONSTACK
	};

	//fprintf(stderr, "perf_event_handler is set up \n");
	if(monitor_sigaction(PERF_SIGNAL, perf_event_handler, 0 /*flags*/, &sa1) == -1) {
		fprintf(stderr, "Failed to set PERF_SIGNAL handler: %s\n", strerror(errno));
		monitor_real_abort();
	}

	monitor_real_pthread_sigmask(SIG_UNBLOCK, &sig_mask, NULL);
}

static void
ibs_perf_init()
{
	perf_mmap_init();
//#if 0
	sigemptyset(&sig_mask);
        sigaddset(&sig_mask, /*PERF_SIGNAL*/ SIGNEW);

        // Setup the signal handler
        sigset_t block_mask;
        sigfillset(&block_mask);

        struct sigaction sa1 = {
                .sa_sigaction = perf_event_handler,
                .sa_mask = block_mask,
                //.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER | SA_ONSTACK
                .sa_flags =  SA_ONSTACK
        };

        fprintf(stderr, "perf_event_handler is set up \n");
        if(monitor_sigaction(/*PERF_SIGNAL*/ SIGNEW, perf_event_handler, 0 /*flags*/, &sa1) == -1) {
                fprintf(stderr, "Failed to set PERF_SIGNAL handler: %s\n", strerror(errno));
                monitor_real_abort();
        }

        monitor_real_pthread_sigmask(SIG_UNBLOCK, &sig_mask, NULL);
	for(int i = 0; i < 1024; i++) {
		in_hpctoolkit_ibs[i] = 0;
	}
//#endif
}


static void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
            asm volatile("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                                        : "0" (*eax), "2" (*ecx));
}

uint32_t get_deep_ibs_info(void)
{   
    uint32_t eax = 0x8000001b;
    uint32_t ebx = 0, ecx = 0, edx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);
    return eax;
}


void set_global_op_sample_rate(int sample_rate)
{
    int max_sample_rate = 0;
    // Check for proper IBS support before we try to read the CPUID information
    // about the maximum sample rate.
    //check_amd_processor();
    //check_basic_ibs_support();
    //check_ibs_op_support();

    if (sample_rate < 0x90)
    {
        fprintf(stderr, "Attempting to set IBS op sample rate too low - %d\n", sample_rate);
        fprintf(stderr, "This generation core should not be set below %d\n", 0x90);
        exit(EXIT_FAILURE);
    }
    uint32_t ibs_id = get_deep_ibs_info();
    uint32_t extra_bits = (ibs_id & (1 << 6)) >> 6;
    if (!extra_bits)
        max_sample_rate = 1<<20;
    else
        max_sample_rate = 1<<27;

    if (sample_rate >= max_sample_rate)
    {
        fprintf(stderr, "Attempting to set IBS op sample rate too high - %d\n", sample_rate);
        fprintf(stderr, "This generation core can only support up to: %d\n", max_sample_rate-1);
        exit(EXIT_FAILURE);
    }
    op_cnt_max_to_set = sample_rate >> 4;
}


//----------------------------------------------------------
// initialize an event
//  event_num: event number
//  name: name of event (has to be recognized by perf event)
//  threshold: sampling threshold 
//----------------------------------------------------------
	static bool
perf_thread_init(event_info_t *event, event_thread_t *et)
{
	//fprintf(stderr, "perf_thread_init is called in thread %d for event %s with period %ld\n", TD_GET(core_profile_trace_data.id), event->metric_desc->name, event->metric_desc->period);
	int my_id = sched_getcpu();
	in_hpctoolkit_ibs[my_id] = 1;
	if(mapping_size > 0) {
		//fprintf(stderr, "thread %d is mapped to core %d\n", TD_GET(core_profile_trace_data.id), mapping_vector[TD_GET(core_profile_trace_data.id) % mapping_size]);
		stick_this_thread_to_core(mapping_vector[TD_GET(core_profile_trace_data.id) % mapping_size]);
		my_id = sched_getcpu();
	}

	if(!hpcrun_ev_is(event->metric_desc->name, "IBS_OP")) {
	et->num_overflows = 0;
	et->prev_num_overflows = 0;
	et->event = event;
	// ask sys to "create" the event
	// it returns -1 if it fails.
	//fprintf(stderr, "monitoring a sample\n");
	event->attr.wakeup_events = 1;
	et->fd = perf_event_open(&event->attr,
			THREAD_SELF, CPU_ANY, GROUP_FD, PERF_FLAGS);
	TMSG(LINUX_PERF, "dbg register event %d, fd: %d, skid: %d, c: %d, t: %d, period: %d, freq: %d",
			event->id, et->fd, event->attr.precise_ip, event->attr.config,
			event->attr.type, event->attr.sample_freq, event->attr.freq);

	// check if perf_event_open is successful
	if (et->fd < 0) {
		fprintf("Linux perf event open %d (%d) failed: %s",
				                                event->id, event->attr.config, strerror(errno));
		EMSG("Linux perf event open %d (%d) failed: %s",
				event->id, event->attr.config, strerror(errno));
		in_hpctoolkit_ibs[my_id] = 0;
		return false;
	}

	// create mmap buffer for this file 
	et->mmap = set_mmap(et->fd);

	// make sure the file I/O is asynchronous
	int flag = fcntl(et->fd, F_GETFL, 0);
	int ret  = fcntl(et->fd, F_SETFL, flag | O_ASYNC );
	if (ret == -1) {
		EMSG("Can't set notification for event %d, fd: %d: %s", 
				event->id, et->fd, strerror(errno));
	}

	// need to set PERF_SIGNAL to this file descriptor
	// to avoid POLL_HUP in the signal handler
	if(hpcrun_ev_is(event->metric_desc->name, "AMD_L1_DATA_ACCESS") || hpcrun_ev_is(event->metric_desc->name, "AMD_MICRO_OP_RETIRED"))
		ret = fcntl(et->fd, F_SETSIG, /*PERF_SIGNAL*/SIGNEW);
	else
		ret = fcntl(et->fd, F_SETSIG, PERF_SIGNAL);
	if (ret == -1) {
		EMSG("Can't set signal for event %d, fd: %d: %s",
				event->id, et->fd, strerror(errno));
	}

	// set file descriptor owner to this specific thread
	struct f_owner_ex owner;
	owner.type = F_OWNER_TID;
	owner.pid  = syscall(SYS_gettid);
	ret = fcntl(et->fd, F_SETOWN_EX, &owner);
	if (ret == -1) {
		EMSG("Can't set thread owner for event %d, fd: %d: %s", 
				event->id, et->fd, strerror(errno));
	}
	//fprintf(stderr, "event %s is initialized using perf_event_open\n", event->metric_desc->name);
	in_hpctoolkit_ibs[my_id] = 0;
	ioctl(et->fd, PERF_EVENT_IOC_RESET, 0);
	return (ret >= 0);
	} else {
		char filename [64];
		et->event = event;
		global_buffer = malloc(BUFFER_SIZE_B);
		//int my_id = sched_getcpu();//TD_GET(core_profile_trace_data.id);
		sprintf(filename, "/dev/cpu/%d/ibs/op", my_id);
                et->fd = open(filename, O_RDONLY | O_NONBLOCK);

                if (et->fd < 0) {
                        //fprintf(stderr, "Could not open %s by thread %d\n", filename, TD_GET(core_profile_trace_data.id));
                        //return false;
                        //continue;
                } else {
			if(!amd_ibs_flag){
			//fprintf(stderr, "Could open %s by thread %d\n", filename, TD_GET(core_profile_trace_data.id));
				amd_ibs_flag = true;
			}
			//fprintf(stderr, "Could open %s by thread %d\n", filename, TD_GET(core_profile_trace_data.id));
                	ioctl(et->fd, SET_BUFFER_SIZE, BUFFER_SIZE_B);
                //ioctl(fd[cpu], SET_POLL_SIZE, poll_size / sizeof(ibs_op_t));
			set_global_op_sample_rate(event->metric_desc->period);
                	ioctl(et->fd, SET_MAX_CNT, op_cnt_max_to_set/*event->metric_desc->period*/);
#if 0
			if (ioctl(et->fd, IBS_ENABLE)) {
                        	fprintf(stderr, "IBS op enable failed on cpu %d\n", my_id);
                        	return false;
                        //continue;
                	}
#endif
			ioctl(et->fd, RESET_BUFFER);
                        ioctl(et->fd, ASSIGN_FD, et->fd);
                        ioctl(et->fd, REG_CURRENT_PROCESS);
			ioctl(et->fd, IBS_ENABLE);
		//for (int i = 0; i < nopfds; i++)
#if 0
               		ioctl(et->fd, RESET_BUFFER);
			ioctl(et->fd, ASSIGN_FD, et->fd);
			ioctl(et->fd, REG_CURRENT_PROCESS);
#endif
		}
		in_hpctoolkit_ibs[my_id] = 0;
		//ioctl(et->fd, REG_CURRENT_PROCESS);
	        return true;	
		//fprintf(stderr, "everything is fine\n");
	}
}


//----------------------------------------------------------
// actions when the program terminates: 
//  - unmap the memory
//  - close file descriptors used by each event
//----------------------------------------------------------

#if 0
	static void
perf_thread_fini(int nevents, event_thread_t *event_thread)
{
	fprintf(stderr, "fini\n");
	for(int i=0; i<nevents; i++) {
		if (event_thread[i].fd >= 0) 
			close(event_thread[i].fd);

		if(hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP") && event_thread[i].fd >= 0)
		{
			free(global_buffer);
		}
		else
		{
			if (event_thread[i].mmap) 
				perf_unmmap(event_thread[i].mmap);
		}
	}
}
#endif


        static void
perf_thread_fini(int nevents, event_thread_t *event_thread)
{
        //fprintf(stderr, "fini\n");
        for(int i=0; i<nevents; i++) {
		if(!hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP")) {
                	if (event_thread[i].fd)
                        	close(event_thread[i].fd);

                        if (event_thread[i].mmap)
                                perf_unmmap(event_thread[i].mmap);
                }
        }
}


// ---------------------------------------------
// get the index of the file descriptor
// ---------------------------------------------

	static event_thread_t*
get_fd_index(int nevents, int fd, event_thread_t *event_thread)
{
	for(int i=0; i<nevents; i++) {
		if (event_thread[i].fd == fd)
			return &(event_thread[i]);
	}
	return NULL; 
}

	static sample_val_t*
record_sample(event_thread_t *current, perf_mmap_data_t *mmap_data,
		void* context, sample_val_t* sv)
{
	//fprintf(stderr, "record_sample is called\n");
	if (current == NULL || current->event == NULL || current->event->metric < 0)
		return NULL;

	//fprintf(stderr, "record_sample is called 1\n");
	// ----------------------------------------------------------------------------
	// for event with frequency, we need to increase the counter by its period
	// sampling taken by perf event kernel
	// ----------------------------------------------------------------------------
	bool amd_ibs_event = false;
	if(hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP") && current->fd >= 0)
			amd_ibs_event = true;
	uint64_t metric_inc = 1;
	if ((!amd_ibs_event && current->event->attr.freq==1) && mmap_data->period > 0)
		metric_inc = mmap_data->period;
	//fprintf(stderr, "metric_inc: %ld\n", metric_inc);
	// ----------------------------------------------------------------------------
	// record time enabled and time running
	// if the time enabled is not the same as running time, then it's multiplexed
	// ----------------------------------------------------------------------------
	u64 time_enabled = !amd_ibs_event ? current->mmap->time_enabled : 1;
	u64 time_running = !amd_ibs_event ? current->mmap->time_running : 1;

	// ----------------------------------------------------------------------------
	// the estimate count = raw_count * scale_factor
	//              = metric_inc * time_enabled / time running
	// ----------------------------------------------------------------------------
	double scale_f = (double) time_enabled / time_running;

	// for period-based sampling with no multiplexing, there is no need to adjust
	// the scale. Also for software event. For them, the value of time_enabled
	//  and time_running are incorrect (the ratio is less than 1 which doesn't make sense)

	if (scale_f < 1.0)
		scale_f = 1.0;

	double counter = scale_f * metric_inc;
	//fprintf(stderr, "counter from metric_inc: %0.2lf\n", counter);

	// ----------------------------------------------------------------------------
	// set additional information for the metric description
	// ----------------------------------------------------------------------------
	thread_data_t *td = hpcrun_get_thread_data();
	metric_aux_info_t *info_aux = &(td->core_profile_trace_data.perf_event_info[current->event->metric]);

	// check if this event is multiplexed. we need to notify the user that a multiplexed
	//  event is not accurate at all.
	// Note: perf event can report the scale to be close to 1 (like 1.02 or 0.99).
	//       we need to use a range of value to see if it's multiplexed or not
	info_aux->is_multiplexed    |= (scale_f>PERF_MULTIPLEX_RANGE);

	// case of multiplexed or frequency-based sampling, we need to store the mean and
	// the standard deviation of the sampling period
	info_aux->num_samples++;
	const double delta    = counter - info_aux->threshold_mean;
	info_aux->threshold_mean += delta / info_aux->num_samples;

	// ----------------------------------------------------------------------------
	// update the cct and add callchain if necessary
	// ----------------------------------------------------------------------------
	sampling_info_t info = {.sample_clock = 0, .sample_data = mmap_data};

	// hpcrun_sample_callpath will use the precise pc, if available
	// Note: if precise_pc and context_pc are in different function
	// the leaft of generated call paths will look odd.
	// I am making this choice to have atleast the leaf correct.
	if(WatchpointClientActive() && (mmap_data->header_misc & PERF_RECORD_MISC_EXACT_IP)){
		td->precise_pc = (void  *) mmap_data->ip;
	} else {
		td->precise_pc = 0;
	}
	//fprintf(stderr, "counter: %0.2lf is incremented\n", counter); 
//#if 0
	//fprintf(stderr, "context: %lx before\n", context);
	*sv = hpcrun_sample_callpath(context, current->event->metric,
			(hpcrun_metricVal_t) {.r=counter},
			0/*skipInner*/, 0/*isSync*/, &info);
	//fprintf(stderr, "context: %lx after\n", context);
//#endif
	// no need to reset the precise_pc; hpcrun_sample_callpath does so
	// td->precise_pc = 0;

#if 0
	//TODO: Delete me
	hpcrun_set_handling_sample(td);
	sigjmp_buf_t* it = &(td->bad_unwind);
	int ljmp = sigsetjmp(it->jb, 1);
	if (ljmp == 0){
		volatile int i;
		void * addr = mmap_data->addr;
		if(mmap_data->ip && addr &&
				((addr && !(((unsigned long)addr) & 0xF0000000000000)))
		  )
			i += *(char*)(mmap_data->addr);
	} else {
		// ok
	}
	hpcrun_clear_handling_sample(td);
#endif

	// check whether we can get the ra_loc in each frame
	if (ENABLED(RALOC)) {
		for (frame_t* f = td->btbuf_beg; f < td->btbuf_cur; f++) {
			if (f->ra_loc)
				TMSG(RALOC, "frame ra_loc = %p, ra@loc = %p", f->ra_loc, *((void**) f->ra_loc));
			else
				TMSG(RALOC, "frame ra_loc = %p", f->ra_loc);
		}
		TMSG(RALOC, "--------------------------");
	}

	if(WatchpointClientActive()){
		//fprintf(stderr, "OnSample is called\n");
//#if 0
		OnSample(mmap_data,
				/*hpcrun_context_pc(context)*/ context,
				sv->sample_node,
				current->event->metric);
//#endif
	}

	return sv;
}


/***
 * get the default event count threshold by looking from the environment variable
 * (HPCRUN_PERF_COUNT) set by hpcrun when user specifies -c option
 */
	static struct event_threshold_s
init_default_count()
{
	const char *str_val= getenv("HPCRUN_PERF_COUNT");
	if (str_val == NULL) {
		return default_threshold;
	}
	int res = hpcrun_extract_threshold(str_val, &default_threshold.threshold_num, DEFAULT_THRESHOLD);
	if (res == 1)
		default_threshold.threshold_type = PERIOD;

	return default_threshold;
}

/******************************************************************************
 * method functions
 *****************************************************************************/

// --------------------------------------------------------------------------
// event occurs when the sample source is initialized
// this method is called first before others
// --------------------------------------------------------------------------
	static void
METHOD_FN(init)
{
	TMSG(LINUX_PERF, "%d: init", self->sel_idx);

	// checking the option of multiplexing:
	// the env variable is set by hpcrun or by user (case for static exec)

	self->state = INIT;

	// init events
	kernel_blocking_init();

	TMSG(LINUX_PERF, "%d: init OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// when a new thread is created and has been started
// this method is called after "start"
// --------------------------------------------------------------------------
	static void
METHOD_FN(thread_init)
{
	TMSG(LINUX_PERF, "%d: thread init", self->sel_idx);

	TMSG(LINUX_PERF, "%d: thread init OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// start of the thread
// --------------------------------------------------------------------------
	static void
METHOD_FN(thread_init_action)
{
	TMSG(LINUX_PERF, "%d: thread init action", self->sel_idx);

	TMSG(LINUX_PERF, "%d: thread init action OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// start of application thread
// --------------------------------------------------------------------------
	static void
METHOD_FN(start)
{
	TMSG(LINUX_PERF, "%d: start", self->sel_idx);

	source_state_t my_state = TD_GET(ss_state)[self->sel_idx];

	// make LINUX_PERF start idempotent.  the application can turn on sampling
	// anywhere via the start-stop interface, so we can't control what
	// state LINUX_PERF is in.

	if (my_state == START) {
		TMSG(LINUX_PERF,"%d: *NOTE* LINUX_PERF start called when already in state START",
				self->sel_idx);
		return;
	}

	int nevents = (self->evl).nevents;

	event_thread_t *event_thread = (event_thread_t *)TD_GET(ss_info)[self->sel_idx].ptr;

	for (int i=0; i<nevents; i++)
	{
		int ret;
		if(hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP") && event_thread[i].fd >= 0){
			ret = ioctl(event_thread[i].fd, IBS_ENABLE);
		} else
			ret = ioctl(event_thread[i].fd, PERF_EVENT_IOC_RESET, 0);
		if (ret == -1) {
			TMSG(LINUX_PERF, "error fd %d in IOC_RESET: %s", event_thread[i].fd, strerror(errno));
		}

		if(hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP"))
			ibs_restart_perf_event( event_thread[i].fd );
		else
			restart_perf_event( event_thread[i].fd );
	}

	thread_data_t* td = hpcrun_get_thread_data();
	td->ss_state[self->sel_idx] = START;

	TMSG(LINUX_PERF, "%d: start OK", self->sel_idx);
}

// --------------------------------------------------------------------------
// end of thread
// --------------------------------------------------------------------------
	static void
METHOD_FN(thread_fini_action)
{
	TMSG(LINUX_PERF, "%d: unregister thread", self->sel_idx);

	TMSG(LINUX_PERF, "%d: unregister thread OK", self->sel_idx);
}


// --------------------------------------------------------------------------
// end of the application
// --------------------------------------------------------------------------
	static void
METHOD_FN(stop)
{
	//fprintf(stderr, "stop\n");
	TMSG(LINUX_PERF, "%d: stop", self->sel_idx);

	source_state_t my_state = TD_GET(ss_state)[self->sel_idx];
	if (my_state == STOP) {
		TMSG(LINUX_PERF,"%d: *NOTE* PERF stop called when already in state STOP",
				self->sel_idx);
		return;
	}

	if (my_state != START) {
		TMSG(LINUX_PERF,"%d: *WARNING* PERF stop called when not in state START",
				self->sel_idx);
		return;
	}

	event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;
	int nevents  = (self->evl).nevents;

	perf_stop_all(nevents, event_thread);

	thread_data_t* td = hpcrun_get_thread_data();
	td->ss_state[self->sel_idx] = STOP;

	TMSG(LINUX_PERF, "%d: stop OK", self->sel_idx);

	for(int i=0; i<nevents; i++) {
		if(hpcrun_ev_is(event_thread[i].event->metric_desc->name, "IBS_OP")) {
			if (event_thread[i].fd >= 0) {
                        	close(event_thread[i].fd);
				free(global_buffer);
			}
		}
	}
}

// --------------------------------------------------------------------------
// really end
// --------------------------------------------------------------------------
	static void
METHOD_FN(shutdown)
{
	//fprintf(stderr, "shutdown\n");
	TMSG(LINUX_PERF, "shutdown");

	METHOD_CALL(self, stop); // make sure stop has been called
	// FIXME: add component shutdown code here

	event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;
	int nevents = (self->evl).nevents; 

	perf_thread_fini(nevents, event_thread);

#ifdef ENABLE_PERFMON
	// terminate perfmon
	pfmu_fini();
#endif

	self->state = UNINIT;
	TMSG(LINUX_PERF, "shutdown OK");
}


// --------------------------------------------------------------------------
// Return true if Linux perf recognizes the name, whether supported or not.
// We'll handle unsupported events later.
// --------------------------------------------------------------------------
	static bool
METHOD_FN(supports_event, const char *ev_str)
{
	TMSG(LINUX_PERF, "supports event %s", ev_str);

#ifdef ENABLE_PERFMON
	// perfmon is smart enough to detect if pfmu has been initialized or not
	pfmu_init();
#endif

	if (self->state == UNINIT){
		METHOD_CALL(self, init);
	}

	// extract the event name and the threshold (unneeded in this phase)
	long thresh;
	char ev_tmp[1024];
	hpcrun_extract_ev_thresh(ev_str, sizeof(ev_tmp), ev_tmp, &thresh, DEFAULT_THRESHOLD) ;

	// check if the event is a predefined event
	//fprintf(stderr, "support of event is checked here\n");
	if (event_custom_find(ev_tmp) != NULL) {
		//fprintf(stderr, "event is supported here\n");
		return true;
	}

	if (hpcrun_ev_is(ev_tmp, "IBS_OP") || hpcrun_ev_is(ev_tmp, "AMD_L1_DATA_ACCESS") || hpcrun_ev_is(ev_tmp, "AMD_MICRO_OP_RETIRED")) {
		//fprintf(stderr, "event %s is supported with period: %ld\n", ev_tmp, thresh);
		return true;
	}

	// this is not a predefined event, we need to consult to perfmon (if enabled)
#ifdef ENABLE_PERFMON
	return pfmu_isSupported(ev_tmp) >= 0;
#else
	return (strncmp(event_name, ev_str, strlen(event_name)) == 0);
#endif
}



// --------------------------------------------------------------------------
// handle a list of events
// --------------------------------------------------------------------------
	static void
METHOD_FN(process_event_list, int lush_metrics)
{
	TMSG(LINUX_PERF, "process event list");

	//fprintf(stderr, "this process_event_list is called\n");
	metric_desc_properties_t prop = metric_property_none;
	char *event;

	char *evlist = METHOD_CALL(self, get_event_str);
	int num_events = 0;

	// TODO: stupid way to count the number of events
	// manually, setup the number of events. In theory, this is to be done
	//  automatically. But in practice, it didn't. Not sure why.

	for (event = start_tok(evlist); more_tok(); event = next_tok(), num_events++);

	self->evl.nevents = num_events;

	// setup all requested events
	// if an event cannot be initialized, we still keep it in our list
	//  but there will be no samples

	size_t size = sizeof(event_info_t) * num_events;
	event_desc = (event_info_t*) hpcrun_malloc(size);
	if (event_desc == NULL) {
		EMSG("Unable to allocate %d bytes", size);
		return;
	}
	memset(event_desc, 0, size);

	extern int *reuse_distance_events;
	extern int reuse_distance_num_events;
	extern int l3_reuse_distance_event;
	extern int amd_reuse_distance_event;
	//extern int amd_micro_op_event;
	//extern int l3_reuse_distance_event_rqsts;
	reuse_distance_events = (int *) hpcrun_malloc(sizeof(int) * num_events);
	reuse_distance_num_events = 0;
	if (reuse_distance_events == NULL){
		EMSG("Unable to allocate %d bytes", sizeof(int)*num_events);
		return;
	}
	l3_reuse_distance_event = 0;
	amd_reuse_distance_event = 0;
	//amd_micro_op_event = 0;
	l3_reuse_distance_event_rqsts = 0;

	int i=0;

	default_threshold = init_default_count();

	for(int i = 0; i < HASH_TABLE_SIZE; i++) {
                event_thread_board[i] = NULL;
        }

	// do things here

	// ----------------------------------------------------------------------
	// for each perf's event, create the metric descriptor which will be used later
	// during thread initialization for perf event creation
	// ----------------------------------------------------------------------
	bool ibs_flag = false;
	for (event = start_tok(evlist); more_tok(); event = next_tok(), i++) {
		char name[1024];
		long threshold = 1;

		TMSG(LINUX_PERF,"checking event spec = %s",event);

		int period_type = hpcrun_extract_ev_thresh(event, sizeof(name), name, &threshold,
				default_threshold.threshold_num);
		//global_sampling_period = threshold;
		//global_sampling_period = threshold;
		if ((strncmp (name,"MEM_UOPS_RETIRED:ALL_STORES",27) == 0) || (strncmp (name,"MEM_INST_RETIRED.ALL_STORES",27) == 0))
			global_store_sampling_period = threshold;

		if (strncmp (name,"MEM_UOPS_RETIRED:ALL_LOADS",26) == 0)
			global_load_sampling_period = threshold;

		if ((strncmp (name,"MEM_LOAD_UOPS_RETIRED.L2_MISS",29) == 0) || (strncmp (name,"MEM_LOAD_RETIRED.L2_MISS",24) == 0)) {
			global_l2_miss_sampling_period = threshold;
		}

		// ------------------------------------------------------------
		// need a special case if we have our own customized  predefined  event
		// This "customized" event will use one or more perf events
		// ------------------------------------------------------------
		event_desc[i].metric_custom = event_custom_find(name);

		if (event_desc[i].metric_custom != NULL) {
			if (event_desc[i].metric_custom->register_fn != NULL) {
				// special registration for customized event
				event_desc[i].metric_custom->register_fn( &event_desc[i] );
				continue;
			}
		}

		struct perf_event_attr *event_attr = &(event_desc[i].attr);

		int isPMU = pfmu_getEventAttribute(name, event_attr);
		if (isPMU < 0 && !hpcrun_ev_is(name, "IBS_OP") && !hpcrun_ev_is(name, "AMD_L1_DATA_ACCESS") && !hpcrun_ev_is(name, "AMD_MICRO_OP_RETIRED")) {
			fprintf(stderr, "%s is an unknown event\n", name);
			// case for unknown event
			// it is impossible to be here, unless the code is buggy
			continue;
		}

		bool is_period = (period_type == 1);

		// ------------------------------------------------------------
		// initialize the generic perf event attributes for this event
		// all threads and file descriptor will reuse the same attributes.
		// ------------------------------------------------------------
		if(!hpcrun_ev_is(name, "IBS_OP")) {
			if(hpcrun_ev_is(name, "AMD_L1_DATA_ACCESS")) {
                        	event_attr->config = /*0x0c0;0x4300c1;*/0x0329;/*0x0229;0x430729;*/
                        	event_attr->type = PERF_TYPE_RAW;
                	} else if(hpcrun_ev_is(name, "AMD_MICRO_OP_RETIRED")) {
                                event_attr->config = 0x4300c1;
                                event_attr->type = PERF_TYPE_RAW;
                        }
			perf_attr_init(event_attr, is_period, threshold, 0);
			if(hpcrun_ev_is(name, "AMD_L1_DATA_ACCESS") || hpcrun_ev_is(name, "AMD_MICRO_OP_RETIRED")) {
				event_attr->sample_type =PERF_SAMPLE_IP;
			}
		} else
			ibs_flag = true;

		// ------------------------------------------------------------
		// initialize the property of the metric
		// if the metric's name has "CYCLES" it mostly a cycle metric 
		//  this assumption is not true, but it's quite closed
		// ------------------------------------------------------------

		prop = (strstr(name, "CYCLES") != NULL) ? metric_property_cycles : metric_property_none;

		char *name_dup = strdup(name); // we need to duplicate the name of the metric until the end
		// since the OS will free it, we don't have to do it in hpcrun
		// set the metric for this perf event
		event_desc[i].metric = hpcrun_new_metric();

		/******** For witch client WP_REUSE ***************/
#ifdef REUSE_HISTO
		if ((strstr(name, "MEM_UOPS_RETIRED") != NULL) || (strstr(name, "MEM_INST_RETIRED") != NULL))
#else
			if ((strstr(name, "MEM_UOPS_RETIRED") != NULL) || (strstr(name, "MEM_INST_RETIRED") != NULL)) //jqswang: TODO // && threshold == 0)
#endif
			{
				//fprintf(stderr, "assignment to l3_reuse_distance_event MEM_INST_RETIRED happens here\n");
				reuse_distance_events[reuse_distance_num_events++] = i;
			}

#ifdef REUSE_HISTO
                if ((strstr(name, "MEM_LOAD_UOPS_RETIRED") != NULL) || (strstr(name, "MEM_LOAD_RETIRED") != NULL))
#else
                        if ((strstr(name, "MEM_LOAD_UOPS_RETIRED") != NULL) || (strstr(name, "MEM_LOAD_RETIRED") != NULL)) //jqswang: TODO // && threshold == 0)
#endif
                        {
                                l3_reuse_distance_event = i;
                                fprintf(stderr, "assignment to l3_reuse_distance_event MEM_LOAD_RETIRED happens here\n");
                        }

		#ifdef REUSE_HISTO
                if ((strstr(name, "L2_RQSTS") != NULL) || (strstr(name, "L2_RQSTS") != NULL))
#else
                        if ((strstr(name, "L2_RQSTS") != NULL) || (strstr(name, "L2_RQSTS") != NULL)) //jqswang: TODO // && threshold == 0)
#endif
                        {
                                l3_reuse_distance_event_rqsts = i;
                                fprintf(stderr, "assignment to L2_RQSTS.MISS happens here\n");
                        }
		#ifdef REUSE_HISTO
                if ((strstr(name, "AMD_L1_DATA_ACCESS") != NULL) || (strstr(name, "AMD_L1_DATA_ACCESS") != NULL))
#else
                        if ((strstr(name, "AMD_L1_DATA_ACCESS") != NULL) || (strstr(name, "AMD_L1_DATA_ACCESS") != NULL)) //jqswang: TODO // && threshold == 0)
#endif
                        {
                                amd_reuse_distance_event = i;
                                fprintf(stderr, "assignment to AMD_L1_DATA_ACCESS happens here\n");
                        }
#ifdef REUSE_HISTO
		if ((strstr(name, "AMD_MICRO_OP_RETIRED") != NULL) || (strstr(name, "AMD_MICRO_OP_RETIRED") != NULL))
#else
                        if ((strstr(name, "AMD_MICRO_OP_RETIRED") != NULL) || (strstr(name, "AMD_MICRO_OP_RETIRED") != NULL)) //jqswang: TODO // && threshold == 0)
#endif
                        {
                                amd_reuse_distance_event = i;
                                fprintf(stderr, "assignment to AMD_MICRO_OP_RETIRED happens here\n");
                        }
		/**************************************************/


		// ------------------------------------------------------------
		// if we use frequency (event_type=1) then the period is not deterministic,
		// it can change dynamically. In this case, the period is 1
		// ------------------------------------------------------------
		if (!is_period) {
			// using frequency : the threshold is always 1, 
			//                   since the period is determine dynamically
			threshold = 1;
		}
		metric_desc_t *m = hpcrun_set_metric_info_and_period(event_desc[i].metric, name_dup,
				MetricFlags_ValFmt_Real, threshold, prop);

		if (m == NULL) {
			EMSG("Error: unable to create metric #%d: %s", index, name);
		} else {
			m->is_frequency_metric = (event_desc[i].attr.freq == 1);
		}
		event_desc[i].metric_desc = m;
		extern void SetupWatermarkMetric(int);
		// Watchpoint
		SetupWatermarkMetric(event_desc[i].metric);
	}

	if (num_events > 0)
		if(!ibs_flag)
			perf_init();
		else
			ibs_perf_init();
}


// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
	static void
METHOD_FN(gen_event_set, int lush_metrics)
{
	TMSG(LINUX_PERF, "gen_event_set");

	//fprintf(stderr, "this gen_event_set is called\n");
	int nevents 	  = (self->evl).nevents;
	int num_metrics = hpcrun_get_num_metrics();

	//fprintf(stderr, "this gen_event_set is called with %d events\n", nevents);
	// a list of event information, private for each thread
	event_thread_t  *event_thread = (event_thread_t*) hpcrun_malloc(sizeof(event_thread_t) * nevents);

	// allocate and initialize perf_event additional metric info

	size_t mem_metrics_size = num_metrics * sizeof(metric_aux_info_t);
	metric_aux_info_t* aux_info = (metric_aux_info_t*) hpcrun_malloc(mem_metrics_size);
	memset(aux_info, 0, mem_metrics_size);

	thread_data_t* td = hpcrun_get_thread_data();

	td->core_profile_trace_data.perf_event_info = aux_info;
	td->ss_info[self->sel_idx].ptr = event_thread;


	// setup all requested events
	// if an event cannot be initialized, we still keep it in our list
	//  but there will be no samples

	for (int i=0; i<nevents; i++)
	{
		// initialize this event. If it's valid, we set the metric for the event
		if (!perf_thread_init( &(event_desc[i]), &(event_thread[i])) ) {
			TMSG(LINUX_PERF, "FAIL to initialize %s", event_desc[i].metric_desc->name);
		}
	}

	event_thread_board[TD_GET(core_profile_trace_data.id)] =  event_thread;
	//fprintf(stderr, "event_thread array is initialized in thread %d\n", TD_GET(core_profile_trace_data.id));

	global_thread_count++;
	dynamic_global_thread_count++;

	TMSG(LINUX_PERF, "gen_event_set OK");
}


// --------------------------------------------------------------------------
// list events
// --------------------------------------------------------------------------
	static void
METHOD_FN(display_events)
{
	event_custom_display(stdout);

	display_header(stdout, "Available Linux perf events");

#ifdef ENABLE_PERFMON
	// perfmon is smart enough to detect if pfmu has been initialized or not
	pfmu_init();
	pfmu_showEventList();
	pfmu_fini();
#else
	printf("Name\t\tDescription\n");
	display_line_single(stdout);

	printf("%s\tTotal cycles.\n",
			"PERF_COUNT_HW_CPU_CYCLES");
	printf("\n");
#endif
	printf("\n");
}


// --------------------------------------------------------------------------
// read a counter from the file descriptor,
//  and returns the value of the counter
// Note: this function is used for debugging purpose in gdb
// --------------------------------------------------------------------------
	long
read_fd(int fd)
{
	char buffer[1024];
	if (fd <= 0)
		return 0;

	size_t t = read(fd, buffer, 1024);
	if (t>0) {
		return atoi(buffer);
	}
	return -1;
}


// ------------------------------------------------------------
// Refresh a disabled perf event
// returns -1 if error, non-negative is success (any returns from ioctl)
// ------------------------------------------------------------

	static int
restart_perf_event(int fd)
{
	if (fd < 0) {
		TMSG(LINUX_PERF, "Unable to start event: fd is not valid");
		return -1;
	}

	int ret = ioctl(fd, PERF_EVENT_IOC_RESET, 0);

	if (ret == -1) {
		TMSG(LINUX_PERF, "error fd %d in PERF_EVENT_IOC_RESET: %s", fd, strerror(errno));
	}

	ret = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1) {
		TMSG(LINUX_PERF, "error fd %d in IOC_REFRESH: %s", fd, strerror(errno));
	}
	return ret;
}

static int
ibs_restart_perf_event(int fd)
{
        if (fd < 0) {
                TMSG(LINUX_PERF, "Unable to start event: fd is not valid");
                return -1;
        }

        int ret = ioctl(fd, RESET_BUFFER);
        if (ret < 0) {
        	TMSG(LINUX_PERF, "error fd %d in ioctl RESET_BUFFER: %s", fd, strerror(errno));
        }
        return ret;
}
/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name linux_perf
#define ss_cls SS_HARDWARE
#define ss_sort_order  70

#include "sample-sources/ss_obj.h"

void linux_perf_events_pause(){
	sample_source_t *self = &obj_name();
	event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;
	int nevents = self->evl.nevents;
	ibs_ctl_backup(nevents, event_thread);
	perf_stop_all(nevents, event_thread);	

}

void linux_perf_events_resume(){
	sample_source_t *self = &obj_name();
	event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;
	int nevents = self->evl.nevents;
	ibs_ctl_reload(nevents, event_thread);
	perf_start_all(nevents, event_thread);
}


// OUTPUT: val, it is a uint64_t array and has at least 3 elements.
// For a counting event, val[0] is the actual value read from counter; val[1] is the time enabling; val[2] is time running
// For a overflow event, val[0] is the actual scaled value; val[1] and val[2] are set to 0
// RETURN: 0, sucess; -1, error
int linux_perf_read_event_counter(int event_index, uint64_t *val){
	//fprintf(stderr, "this function is executed\n");
	sample_source_t *self = &obj_name();
	event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;

	event_thread_t *current = &(event_thread[event_index]);

	int ret = perf_read_event_counter(current, val);

	if (ret < 0) {
		//fprintf(stderr, "problem here 1\n");
		return -1; // something wrong here
	}

	uint64_t sample_period = current->event->attr.sample_period;
	if (sample_period == 0){ // counting event
		return 0;
	} else {
		// overflow event
		//assert(val[1] == val[2]); //jqswang: TODO: I have no idea how to calculate the value under multiplexing for overflow event.
		int64_t scaled_val = (int64_t) val[0] ;//% sample_period;
		//fprintf(stderr, "original counter value %ld in thread %d\n", scaled_val, TD_GET(core_profile_trace_data.id));
		if (event_index != amd_reuse_distance_event && (scaled_val >= sample_period * 10 // The counter value can become larger than the sampling period but they are usually less than 2 * sample_period
				|| scaled_val < 0)){
			//jqswang: TODO: it does not filter out all the invalid values
			//fprintf(stderr, "WEIRD_COUNTER: %ld %s\n", scaled_val, current->event->metric_desc->name);
			hpcrun_stats_num_corrected_reuse_distance_inc(1);
			scaled_val = 0;
		}
		//fprintf(stderr, "in linux_perf_read_event_counter %s: num_overflows: %lu, val[0]: %ld, val[1]: %lu, val[2]: %lu\n", current->event->metric_desc->name, current->num_overflows, val[0],val[0],val[1],val[2]);
		//fprintf(stderr, "current->num_overflows: %ld, current->prev_num_overflows: %ld, sample_period: %ld, scaled_val: %ld\n", current->num_overflows, current->prev_num_overflows, sample_period, scaled_val);
		//val[0] = (current->num_overflows > current->prev_num_overflows) ? (current->num_overflows * sample_period) : ((current->num_overflows > 0) ? (current->num_overflows * sample_period + scaled_val) : scaled_val);
		val[0] = current->num_overflows * sample_period + scaled_val;
		//fprintf(stderr, "val[0]: %ld\n", val[0]);
		current->prev_num_overflows = current->num_overflows;
		val[1] = 0;
		val[2] = 0;
		return 0;
	}
}

int linux_perf_read_event_counter_l1(int event_index, uint64_t *val, bool use){
        //fprintf(stderr, "this function is executed\n");
        sample_source_t *self = &obj_name();
        event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;

        event_thread_t *current = &(event_thread[event_index]);

        int ret = perf_read_event_counter(current, val);

        if (ret < 0) {
                //fprintf(stderr, "problem here 1\n");
                return -1; // something wrong here
        }

        uint64_t sample_period = current->event->attr.sample_period;
        if (sample_period == 0){ // counting event
                return 0;
        } else {
                // overflow event
                //assert(val[1] == val[2]); //jqswang: TODO: I have no idea how to calculate the value under multiplexing for overflow event.
                int64_t scaled_val = (int64_t) val[0] ;//% sample_period;
                //fprintf(stderr, "original counter value %ld\n", scaled_val);
                if (scaled_val >= sample_period * 10 // The counter value can become larger than the sampling period but they are usually less than 2 * sample_period
                                || scaled_val < 0){
                        //jqswang: TODO: it does not filter out all the invalid values
                        //fprintf(stderr, "WEIRD_COUNTER: %ld %s\n", scaled_val, current->event->metric_desc->name);
                        hpcrun_stats_num_corrected_reuse_distance_inc(1);
                        scaled_val = 0;
                }
		val[0] = use ? current->num_overflows * sample_period : current->num_overflows * sample_period + scaled_val;
		//val[0] = current->num_overflows * sample_period + scaled_val;
                //fprintf(stderr, "val[0]: %ld\n", val[0]);
                current->prev_num_overflows = current->num_overflows;
                val[1] = scaled_val;
                val[2] = 0;
                return 0;  
        }
}

int linux_perf_read_event_counter_l1_amd(int event_index, uint64_t *val){
        //fprintf(stderr, "this function is executed\n");
        sample_source_t *self = &obj_name();
        event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;

        event_thread_t *current = &(event_thread[event_index]);

        int ret = perf_read_event_counter(current, val);

        if (ret < 0) {
                //fprintf(stderr, "problem here 1\n");
                return -1; // something wrong here
        }

        uint64_t sample_period = current->event->attr.sample_period;
        if (sample_period == 0){ // counting event
                return 0;
        } else {
                // overflow event
                //assert(val[1] == val[2]); //jqswang: TODO: I have no idea how to calculate the value under multiplexing for overflow event.
                int64_t scaled_val = (int64_t) val[0] ;//% sample_period;
                //fprintf(stderr, "original counter value %ld\n", scaled_val);
                if (scaled_val >= sample_period * 10 // The counter value can become larger than the sampling period but they are usually less than 2 * sample_period
                                || scaled_val < 0){
                        //jqswang: TODO: it does not filter out all the invalid values
                        //fprintf(stderr, "WEIRD_COUNTER: %ld %s\n", scaled_val, current->event->metric_desc->name);
                        hpcrun_stats_num_corrected_reuse_distance_inc(1);
			scaled_val = 0;
                }
                val[0] = current->num_overflows * sample_period + scaled_val;
                //val[0] = current->num_overflows * sample_period + scaled_val;
                //fprintf(stderr, "val[0]: %ld\n", val[0]);
                //current->prev_num_overflows = current->num_overflows;
                val[1] = scaled_val;
                val[2] = 0;
                return 0;
        }
}

int linux_perf_read_event_counter_shared(int event_index, uint64_t *val, int tid){
	//fprintf(stderr, "this function is executed\n");
	//sample_source_t *self = &obj_name();
	/*if (event_index == 0) {
                fprintf(stderr, "problem here\n");
                return -1; // something wrong here
        }*/

	event_thread_t *event_thread = event_thread_board[tid];


	if (event_thread == NULL) {
                //fprintf(stderr, "problem here 2 in thread %d\n", tid);
                return -1; // something wrong here
        }

	event_thread_t *current = &(event_thread[event_index]);


	//fprintf(stderr, "this function is executed before perf_read_event_counter\n");
	int ret = perf_read_event_counter(current, val);
	//fprintf(stderr, "this function is executed after perf_read_event_counter\n");
	
	if (ret < 0) {
		//fprintf(stderr, "problem here 3\n");
		return -1; // something wrong here
	}

	uint64_t sample_period = current->event->attr.sample_period;
	if (sample_period == 0){ // counting event
		return 0;
	} else {
		// overflow event
		//assert(val[1] == val[2]); //jqswang: TODO: I have no idea how to calculate the value under multiplexing for overflow event.
		int64_t scaled_val = (int64_t) val[0] ;//% sample_period;
		//fprintf(stderr, "original counter value %ld\n", scaled_val);
		if (((event_index != l3_reuse_distance_event_rqsts) &&  (scaled_val >= sample_period * 10)) // The counter value can become larger than the sampling period but they are usually less than 2 * sample_period
				|| scaled_val < 0){
			//jqswang: TODO: it does not filter out all the invalid values
			//fprintf(stderr, "WEIRD_COUNTER: %ld %s\n", scaled_val, current->event->metric_desc->name);
			hpcrun_stats_num_corrected_reuse_distance_inc(1);
			scaled_val = 0;
		}
		//fprintf(stderr, "in linux_perf_read_event_counter %s: num_overflows: %lu, val[0]: %ld, val[1]: %lu, val[2]: %lu\n", current->event->metric_desc->name, current->num_overflows, val[0],val[0],val[1],val[2]);
		//fprintf(stderr, "current->num_overflows: %ld, current->prev_num_overflows: %ld, sample_period: %ld, scaled_val: %ld\n", current->num_overflows, current->prev_num_overflows, sample_period, scaled_val);
		//val[0] = (current->num_overflows > current->prev_num_overflows) ? (current->num_overflows * sample_period) : ((current->num_overflows > 0) ? (current->num_overflows * sample_period + scaled_val) : scaled_val);
		 val[0] = current->num_overflows * sample_period + scaled_val;
		//fprintf(stderr, "val[0]: %ld\n", val[0]);
		//current->prev_num_overflows = current->num_overflows;
		val[1] = 0;
		val[2] = 0;
		return 0;
	}
}

// ---------------------------------------------
// signal handler
// ---------------------------------------------

        static int
sig_event_handler(int n, siginfo_t *info, void *unused)
{
	int fd;
    //fprintf(stderr, "sig_event_handler is called\n");
    int my_id = TD_GET(core_profile_trace_data.id);
    if (n == /*PERF_SIGNAL*/SIGNEW && my_id >= 0) {
        fd = info->si_fd;//info->si_int;
        //printf ("Received signal from kernel : Value =  %u\n", check);
        //read(check, read_buf, 1024);
        printf("signal %d from file with fd %d in thread %d\n", n, fd, my_id);
	ioctl(fd, IBS_DISABLE);
	// before
	int tmp = 0;
	int num_items = 0;

	tmp = read(fd, global_buffer, BUFFER_SIZE_B);
	if (tmp <= 0) {
		fprintf(stderr, "returns here tmp: %d\n", tmp);
		ioctl(fd, IBS_ENABLE);
		return;
	}
	num_items = tmp / sizeof(ibs_op_t);
	n_op_samples[my_id] += num_items;
	n_lost_op_samples[my_id] += ioctl(fd, GET_LOST);
	char * sample_buffer = malloc (sizeof(ibs_op_t));
	int offset = 0;
	for (int i = 0; i < num_items; i++) {
		//fread((char *)&op, sizeof(op), 1, op_in_fp)
		memcpy ( sample_buffer, global_buffer + offset, sizeof(ibs_op_t) );
		offset += i * sizeof(ibs_op_t);
		ibs_op_t *op_data = (ibs_op_t *) sample_buffer;
		//fprintf(stderr, " sampling timestamp: %ld, cpu: %d, tid: %d, pid: %d\n", op_data->tsc, op_data->cpu, op_data->tid, op_data->pid);	
	}
	free (sample_buffer);
	// after
	ioctl(fd, IBS_ENABLE);
    }
}

int
read_ibs_buffer(event_thread_t *current, perf_mmap_data_t *mmap_info, ibs_op_t * op_data)
{
	mmap_info->period = current->event->metric_desc->period;
	mmap_info->time = op_data->tsc;
	mmap_info->cpu = op_data->cpu;
	mmap_info->tid = op_data->tid;
	mmap_info->pid = op_data->pid;
	mmap_info->load = op_data->op_data3.reg.ibs_ld_op;
	mmap_info->store = op_data->op_data3.reg.ibs_st_op;
	mmap_info->mem_width = op_data->op_data3.reg.ibs_op_mem_width;
	mmap_info->addr_valid = op_data->op_data3.reg.ibs_lin_addr_valid;
	//if(mmap_info->addr_valid)
	mmap_info->addr = op_data->dc_lin_ad;

	mmap_info->phy_addr_valid = op_data->op_data3.reg.ibs_phy_addr_valid;
	//if(mmap_info->phy_addr_valid)
	mmap_info->phy_addr = op_data->dc_phys_ad.reg.ibs_dc_phys_addr;
	mmap_info->ip = op_data->op_rip;
	mmap_info->micro_op_sample = op_data->micro_op_sample;
	mmap_info->mem_access_sample = op_data->mem_access_sample;
	mmap_info->valid_mem_access_sample = op_data->valid_mem_access_sample;

	//fprintf(stderr, "in read_ibs_buffer sampling timestamp: %ld, cpu: %d, tid: %d, pid: %d, sampled address: %lx, ld_op: %d, st_op:%d, handled by thread %ld, kern_mode: %d\n", op_data->tsc, op_data->cpu, op_data->tid, op_data->pid, op_data->dc_lin_ad, op_data->op_data3.reg.ibs_ld_op, op_data->op_data3.reg.ibs_st_op, syscall(SYS_gettid), op_data->kern_mode);
	//fprintf(stderr, "in read_ibs_buffer ibs_rip_invalid: %d, ibs_ld_op: %d, ibs_st_op: %d, ibs_lin_addr_valid: %d, ibs_op_mem_width: %d\n", op_data->op_data.reg.ibs_rip_invalid, op_data->op_data3.reg.ibs_ld_op, op_data->op_data3.reg.ibs_st_op, op_data->op_data3.reg.ibs_lin_addr_valid, op_data->op_data3.reg.ibs_op_mem_width);
	return 0;
}

	static int
perf_event_handler(
		int sig, 
		siginfo_t* siginfo, 
		void* context
		)
{
	// ----------------------------------------------------------------------------
	// disable all counters
	// ----------------------------------------------------------------------------
	//fprintf(stderr, "in perf_event_handler 0\n");
	sample_source_t *self = &obj_name();
	event_thread_t *event_thread = TD_GET(ss_info)[self->sel_idx].ptr;

	int nevents = self->evl.nevents;
	//ibs_ctl_backup(nevents, event_thread);
	perf_stop_all(nevents, event_thread);

// check counter here 1
// check counter here 2

	int my_id = sched_getcpu();


	int fd;
	fd = siginfo->si_fd;
	event_thread_t *current = get_fd_index(nevents, fd, event_thread);
	if (current == NULL) {
                // signal not from perf event
                TMSG(LINUX_PERF, "signal si_code %d with fd %d: unknown perf event",
                                siginfo->si_fd, fd);
                //fprintf(stderr, "signal si_code %d with fd %d: unknown perf event\n", siginfo->si_code, fd);
                //hpcrun_safe_exit();

		if(amd_ibs_flag)
        		ibs_restart_perf_event(fd);
		else
                	restart_perf_event(fd);
		//ibs_ctl_reload(nevents, event_thread);
                perf_start_all(nevents, event_thread);

                return 0; // tell monitor the signal has not been handled.
        }
	//fprintf(stderr, "sample of event with name %s is detected with fd: %d in thread: %d\n", current->event->metric_desc->name, fd, TD_GET(core_profile_trace_data.id));
	//fprintf(stderr, "in perf_event_handler 1\n");

	// ----------------------------------------------------------------------------
	// check #0:
	// if the interrupt came while inside our code, then drop the sample
	// and return and avoid the potential for deadlock.
	// ----------------------------------------------------------------------------

	void *pc = hpcrun_context_pc(context);

	//fprintf(stderr, "sample in addr %lx in perf_event_handler\n", pc);
//#if 0
	if (! hpcrun_safe_enter_async(pc) /*&& !hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP")*/) {
//#endif
		hpcrun_stats_num_samples_blocked_async_inc();
		if(hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP"))
			ibs_restart_perf_event(/*siginfo->si_int*/ siginfo->si_fd);
		else
			restart_perf_event(/*siginfo->si_int*/ siginfo->si_fd);
		//fprintf(stderr, "quit perf_event_handler pc: %lx\n", pc);
		//ibs_ctl_reload(nevents, event_thread);
		perf_start_all(nevents, event_thread);
		return 0; // tell monitor the signal has been handled.
	}

	if (in_hpctoolkit_ibs[my_id] == 1) {
                //ibs_ctl_reload(nevents, event_thread);

		if(hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP"))
                        ibs_restart_perf_event(fd);
                else
                        restart_perf_event(fd);

                perf_start_all(nevents, event_thread);

                return 0;
        }
//#if 0
	//fprintf(stderr, "in perf_event_handler 2\n");

	// ----------------------------------------------------------------------------
	// check #1: check if signal generated by kernel for profiling
	// ----------------------------------------------------------------------------
	if (siginfo->si_code < 0 && !hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP")) {
		TMSG(LINUX_PERF, "signal si_code %d < 0 indicates not from kernel", 
				siginfo->si_code);
		//fprintf(stderr, "quit 1\n");
		//ibs_ctl_reload(nevents, event_thread);
		perf_start_all(nevents, event_thread);

		return 1; // tell monitor the signal has not been handled.
	}

	// ----------------------------------------------------------------------------
	// check #2:
	// if sampling disabled explicitly for this thread, skip all processing
	// ----------------------------------------------------------------------------
	if (hpcrun_thread_suppress_sample) {
		//fprintf(stderr, "quit 2\n");
		return 0;
	}

	//fprintf(stderr, "in perf_event_handler\n");
	//int fd;
	//if(hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP") && current->fd >= 0)
	//fd = siginfo->si_fd;

	//fprintf(stderr, "in perf_event_handler fd: %d\n", fd);
	// ----------------------------------------------------------------------------
	// check #3: we expect only POLL_HUP, not POLL_IN
	// Sometimes we have signal code other than POll_HUP
	// and still has a valid information (x86 on les).
	// ----------------------------------------------------------------------------
#if 0
	if (siginfo->si_code != POLL_HUP) {
		TMSG(LINUX_PERF, "signal si_code %d (fd: %d) not generated by signal %d",
				siginfo->si_code, siginfo->si_fd, PERF_SIGNAL);

		restart_perf_event(fd);
		perf_start_all(nevents, event_thread);
		return 0; // tell monitor the signal has not been handled.
	}
#endif

	// ----------------------------------------------------------------------------
	// check #4:
	// check the index of the file descriptor (if we have multiple events)
	// if the file descriptor is not on the list, we shouldn't store the 
	// metrics. Perhaps we should throw away?
	// ----------------------------------------------------------------------------

	int tmp = 0;
	bool amd_ibs_event = false;
	if(hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP") && current->fd >= 0)
	{
		amd_ibs_event = true;
		tmp = read(fd, global_buffer, BUFFER_SIZE_B);
	}

	// Increment the number of overflows for the current event
	current->num_overflows++;
	// ----------------------------------------------------------------------------
	// parse the buffer until it finishes reading all buffers
	// ----------------------------------------------------------------------------
//#if 0
	int more_data = 0;
	char * sample_buffer;
	if(amd_ibs_event)
	{
       		more_data = tmp / sizeof(ibs_op_t);
		sample_buffer = malloc (sizeof(ibs_op_t));
		//fprintf(stderr, "%d samples are detected in a counter overflow\n", more_data);
	} 

	int offset = 0;
	//fprintf(stderr, "event with name %s is about to be read\n", current->event->metric_desc->name);
	int i = 0;
//#if 0
	do {
		perf_mmap_data_t mmap_data;
		memset(&mmap_data, 0, sizeof(perf_mmap_data_t));

		 sample_val_t sv;
		 memset(&sv, 0, sizeof(sample_val_t));
		// reading info from mmapped buffer
		if(amd_ibs_event)
		{
			memcpy ( sample_buffer, global_buffer + offset, sizeof(ibs_op_t) );
			i++;
                	offset = i * sizeof(ibs_op_t);
                	ibs_op_t *op_data = (ibs_op_t *) sample_buffer;
			original_sample_count++;
			//fprintf(stderr, "more_data: %d\n", more_data);
			read_ibs_buffer(current, &mmap_data, op_data);

			//if (hpcrun_safe_enter_async(mmap_data.ip) /*&& !hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP")*/) {	
			//fprintf(stderr, "before record_sample\n");
        		record_sample(current, &mmap_data, context, &sv);
			//}

			//if(!op_data->kern_mode)
			//fprintf(stderr, "event with name %s is handled here\n", current->event->metric_desc->name);
			//record_sample(current, &mmap_data, context, &sv);		
			more_data--;
		} else
		{
			more_data = read_perf_buffer(current, &mmap_data);

			//fprintf(stderr, "event with name %s is handled there\n", current->event->metric_desc->name);
                	if (mmap_data.header_type == PERF_RECORD_SAMPLE && !hpcrun_ev_is(current->event->metric_desc->name, "AMD_L1_DATA_ACCESS") && !hpcrun_ev_is(current->event->metric_desc->name, "AMD_MICRO_OP_RETIRED"))
                        	record_sample(current, &mmap_data, context, &sv);

			kernel_block_handler(current, sv, &mmap_data);
		}
		//fprintf(stderr, "event with name %s has been read\n", current->event->metric_desc->name);
		TMSG(LINUX_PERF, "record buffer: sid: %d, ip: %p, addr: %p, id: %d", mmap_data.sample_id, mmap_data.ip, mmap_data.addr, mmap_data.id);

#if 0
		sample_val_t sv;
		memset(&sv, 0, sizeof(sample_val_t));

		if (mmap_data.header_type == PERF_RECORD_SAMPLE)
			record_sample(current, &mmap_data, context, &sv);

		kernel_block_handler(current, sv, &mmap_data);
#endif

	} while (more_data > 0);
//#endif
	if(amd_ibs_event)
        {
                free(sample_buffer);
        }
//#endif
	hpcrun_safe_exit();

	if(hpcrun_ev_is(current->event->metric_desc->name, "IBS_OP"))
		ibs_restart_perf_event(fd);
	else
		restart_perf_event(fd);

	//ibs_ctl_reload(nevents, event_thread);
	perf_start_all(nevents, event_thread);

	return 0; // tell monitor the signal has been handled.
}

