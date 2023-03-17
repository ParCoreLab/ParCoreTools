#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <omp.h>

//#define REG_CURRENT_PROCESS 101

#define SIGNEW 44
#define SET_BUFFER_SIZE 0xEU
#define BUFFER_SIZE_B   (1 << 10)
#define SET_MAX_CNT     0x6U
#define OP_MAX_CNT  0x4000
#define IBS_ENABLE      0x0U
#define IBS_DISABLE     0x1U
#define RESET_BUFFER    0x10U
#define GET_LOST        0xEEU

#define REG_CURRENT_PROCESS _IOW('a', 'a', int32_t*)
#define ASSIGN_FD 102

int n_op_samples[1024];
int n_lost_op_samples[1024];

int8_t write_buf[1024];
int8_t read_buf[1024];

int op_cnt_max_to_set = 0;
int buffer_size = 0;
char *global_buffer[1024] = {NULL};

int arr[100000];
int global_var;
int global_arr[2];

int thread_count = 0;
__thread int my_id = -1;
/* The following unions can be used to pull out specific values from inside of
   an IBS sample. */
typedef union {
    uint64_t val;
    struct {
        uint16_t ibs_op_max_cnt     : 16;
        uint16_t reserved_1          : 1;
        uint16_t ibs_op_en           : 1;
        uint16_t ibs_op_val          : 1;
        uint16_t ibs_op_cnt_ctl      : 1;
        uint16_t ibs_op_max_cnt_upper: 7;
        uint16_t reserved_2          : 5;
        uint32_t ibs_op_cur_cnt     : 27;
        uint32_t reserved_3          : 5;
    } reg;
} ibs_op_ctl_t;

typedef union {
    uint64_t val;
    struct {
        uint16_t ibs_comp_to_ret_ctr;
        uint16_t ibs_tag_to_ret_ctr;
        uint8_t ibs_op_brn_resync   : 1; /* Fam. 10h, LN, BD only */
        uint8_t ibs_op_misp_return  : 1; /* Fam. 10h, LN, BD only */
        uint8_t ibs_op_return       : 1;
        uint8_t ibs_op_brn_taken    : 1;
        uint8_t ibs_op_brn_misp     : 1;
        uint8_t ibs_op_brn_ret      : 1;
        uint8_t ibs_rip_invalid     : 1;
        uint8_t ibs_op_brn_fuse     : 1; /* KV+, BT+ */
        uint8_t ibs_op_microcode    : 1; /* KV+, BT+ */
        uint32_t reserved           : 23;
    } reg;
} ibs_op_data1_t;

typedef union {
    uint64_t val;
    struct {
        uint8_t  ibs_nb_req_src          : 3;
        uint8_t  reserved_1              : 1;
        uint8_t  ibs_nb_req_dst_node     : 1; /* Not valid in BT, JG */
        uint8_t  ibs_nb_req_cache_hit_st : 1; /* Not valid in BT, JG */
        uint64_t reserved_2              : 58;
    } reg;
} ibs_op_data2_t;

typedef union {
    uint64_t val;
    struct {
        uint8_t ibs_ld_op                    : 1;
        uint8_t ibs_st_op                    : 1;
        uint8_t ibs_dc_l1_tlb_miss           : 1;
        uint8_t ibs_dc_l2_tlb_miss           : 1;
        uint8_t ibs_dc_l1_tlb_hit_2m         : 1;
        uint8_t ibs_dc_l1_tlb_hit_1g         : 1;
        uint8_t ibs_dc_l2_tlb_hit_2m         : 1;
        uint8_t ibs_dc_miss                  : 1;
        uint8_t ibs_dc_miss_acc              : 1;
        uint8_t ibs_dc_ld_bank_con           : 1; /* Fam. 10h, LN, BD only */
        uint8_t ibs_dc_st_bank_con           : 1; /* Fam. 10h, LN only */
        uint8_t ibs_dc_st_to_ld_fwd          : 1; /* Fam. 10h, LN, BD, BT+ */
        uint8_t ibs_dc_st_to_ld_can          : 1; /* Fam. 10h, LN, BD only */
        uint8_t ibs_dc_wc_mem_acc            : 1;
        uint8_t ibs_dc_uc_mem_acc            : 1;
        uint8_t ibs_dc_locked_op             : 1;
        uint16_t ibs_dc_no_mab_alloc         : 1; /* Fam. 10h-TN:
                                                    IBS DC MAB hit */
        uint16_t ibs_lin_addr_valid          : 1;
        uint16_t ibs_phy_addr_valid          : 1;
        uint16_t ibs_dc_l2_tlb_hit_1g        : 1;
        uint16_t ibs_l2_miss                 : 1; /* KV+, BT+ */
        uint16_t ibs_sw_pf                   : 1; /* KV+, BT+ */
        uint16_t ibs_op_mem_width            : 4; /* KV+, BT+ */
        uint16_t ibs_op_dc_miss_open_mem_reqs: 6; /* KV+, BT+ */
        uint16_t ibs_dc_miss_lat;
        uint16_t ibs_tlb_refill_lat; /* KV+, BT+ */
    } reg;
} ibs_op_data3_t;

typedef union {
    uint64_t val;
    struct {
        uint8_t ibs_op_ld_resync: 1;
        uint64_t reserved       : 63;
    } reg;
} ibs_op_data4_t; /* CZ, ST only */

typedef union {
    uint64_t val;
    struct {
        uint64_t ibs_dc_phys_addr   : 48;
        uint64_t reserved           : 16;
    } reg;
} ibs_op_dc_phys_addr_t;


typedef struct ibs_op {
	ibs_op_ctl_t            op_ctl;
	uint64_t                op_rip;
	ibs_op_data1_t          op_data;
	ibs_op_data2_t          op_data2;
	ibs_op_data3_t          op_data3;
	ibs_op_data4_t          op_data4;
	uint64_t                dc_lin_ad;
	ibs_op_dc_phys_addr_t   dc_phys_ad;
	uint64_t                br_target;
	uint64_t                tsc;
	uint64_t                cr3;
	int                     tid;
	int                     pid;
	int                     cpu;
	int                     kern_mode;
} ibs_op_t;

void sig_event_handler(int n, siginfo_t *info, void *unused)
{
    int fd;
    if (n == SIGNEW && my_id >= 0) {
        fd = info->si_int;
        //printf ("Received signal from kernel : Value =  %u\n", check);
        //read(check, read_buf, 1024);
        //printf("signal %d from file with fd %d\n", n, fd);
	ioctl(fd, IBS_DISABLE);
	// before
	int tmp = 0;
	int num_items = 0;

	tmp = read(fd, global_buffer[my_id], buffer_size);
	if (tmp <= 0) {
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
		memcpy ( sample_buffer, global_buffer[my_id] + offset, sizeof(ibs_op_t) );
		offset += i * sizeof(ibs_op_t);
		ibs_op_t *op_data = (ibs_op_t *) sample_buffer;
		//fprintf(stderr, " sampling timestamp: %ld, cpu: %d, tid: %d, pid: %d\n", op_data->tsc, op_data->cpu, op_data->tid, op_data->pid);
#if 0		
		if (op_data->op_data3.reg.ibs_lin_addr_valid)
			fprintf(stderr, " sampling timestamp: %ld, cpu: %d, tid: %d, pid: %d, sampled address: %lx, ld_op: %d, st_op:%d, handled by thread %ld, i: %d out of %d items in buffer\n", op_data->tsc, op_data->cpu, op_data->tid, op_data->pid, op_data->dc_lin_ad, op_data->op_data3.reg.ibs_ld_op, op_data->op_data3.reg.ibs_st_op, syscall(SYS_gettid), i, num_items);
#endif
		if (op_data->op_data3.reg.ibs_phy_addr_valid == 1 && op_data->kern_mode == 0 && (op_data->op_data3.reg.ibs_ld_op == 1 || op_data->op_data3.reg.ibs_st_op == 1))
			fprintf(stderr, "op_data->dc_phys_ad.reg.bs_dc_phys_addr: %ld op_data->dc_lin_ad: %ld in tid: %d op_data->op_data3.reg.ibs_lin_addr_valid: %d\n", op_data->dc_phys_ad.reg.ibs_dc_phys_addr, op_data->dc_lin_ad, op_data->tid, op_data->op_data3.reg.ibs_lin_addr_valid);
	}
	free (sample_buffer);
	// after
	ioctl(fd, IBS_ENABLE);
    }
}

/*void sig_event_handler(int n, siginfo_t *info, void *unused)
{
    int check;
    if (n == SIGNEW) {
        check = info->si_int;
        printf ("Received signal from kernel : from device with fd =  %u\n", check);
    }
}*/

int main()
{
	char filename [64];

	int num_cpus = get_nprocs_conf();
	int num_online_cpus = get_nprocs();
	int nopfds = 0;

	pid_t cpid;
	int cpu;
	int * fd = calloc(num_cpus, sizeof(int));
	buffer_size = BUFFER_SIZE_B;
	op_cnt_max_to_set = 100000;//OP_MAX_CNT;
	int i;

	struct sigaction act;
        sigemptyset(&act.sa_mask);
        act.sa_flags = (SA_SIGINFO | SA_RESTART);
        act.sa_sigaction = sig_event_handler;
        sigaction(SIGNEW, &act, NULL);

	//global_buffer = malloc(buffer_size);
#if 0
	for (cpu = 0; cpu < num_cpus; cpu++) {
//#pragma omp parallel
	        //{
		//int cpu = omp_get_thread_num();
		sprintf(filename, "/dev/cpu/%d/ibs/op", cpu);
		fd[cpu] = open(filename, O_RDONLY | O_NONBLOCK);

		if (fd[cpu] < 0) {
                	fprintf(stderr, "Could not open %s\n", filename);
                	//goto END;
			continue;
            	}

		ioctl(fd[cpu], SET_BUFFER_SIZE, buffer_size);
		//ioctl(fd[cpu], SET_POLL_SIZE, poll_size / sizeof(ibs_op_t));
		ioctl(fd[cpu], SET_MAX_CNT, op_cnt_max_to_set);
            	if (ioctl(fd[cpu], IBS_ENABLE)) {
                	fprintf(stderr, "IBS op enable failed on cpu %d\n", cpu);
                	//goto END;
			continue;
            	}
		if (ioctl(fd[cpu], REG_CURRENT_PROCESS)) {
                        fprintf(stderr, "REG_CURRENT_PROCESS failed on cpu %d\n", cpu);
                        //goto END;
			continue;
                }
		if (ioctl(fd[cpu], ASSIGN_FD, fd[cpu])) {
                        fprintf(stderr, "ASSIGN_FD failed on cpu %d\n", cpu);
                        //goto END;
			continue;
                }
            	//fds[count].events = POLLIN | POLLRDNORM;
            	nopfds++;
	}
#endif

	
#if 0
	for (int i = 0; i < nopfds; i++)
        	ioctl(fd[i], RESET_BUFFER);
#endif

	#pragma omp parallel
	{
		my_id = omp_get_thread_num();
		n_op_samples[my_id] = 0;
        	n_lost_op_samples[my_id] = 0;
		global_buffer[my_id] = malloc(buffer_size);
		sprintf(filename, "/dev/cpu/%d/ibs/op", my_id);
                fd[my_id] = open(filename, O_RDONLY | O_NONBLOCK);

                if (fd[my_id] < 0) {
                        fprintf(stderr, "Could not open %s\n", filename);
                        goto END;
                        //continue;
                }

                ioctl(fd[my_id], SET_BUFFER_SIZE, buffer_size);
                //ioctl(fd[cpu], SET_POLL_SIZE, poll_size / sizeof(ibs_op_t));
                ioctl(fd[my_id], SET_MAX_CNT, op_cnt_max_to_set);
#if 0
		if (ioctl(fd[my_id], IBS_ENABLE)) {
                        fprintf(stderr, "IBS op enable failed on cpu %d\n", my_id);
                        goto END;
                        //continue;
                }
		//for (int i = 0; i < nopfds; i++)
                ioctl(fd[my_id], RESET_BUFFER);
#endif
		ioctl(fd[my_id], REG_CURRENT_PROCESS); 
		ioctl(fd[my_id], ASSIGN_FD, fd[my_id]);
		if (ioctl(fd[my_id], IBS_ENABLE)) {
                        fprintf(stderr, "IBS op enable failed on cpu %d\n", my_id);
                        goto END;
                        //continue;
                }
                //for (int i = 0; i < nopfds; i++)
                ioctl(fd[my_id], RESET_BUFFER);
		//if(omp_get_thread_num() % 5 == 0)
		//{
			fprintf(stderr, "thread %d has OS id %ld\n", my_id, syscall(__NR_gettid));
			long sum = 0;
			for(int i = 0; i < 100000000; i++) {
				//global_var += 100;
				global_arr[i % 2] += 100;
			}
			fprintf(stderr, "sum's address: %lx\n",(long unsigned int) &sum);
		//}
		ioctl(fd[my_id], IBS_DISABLE);
END:
		close(fd[my_id]);
		fprintf(stderr, "n_op_samples: %d in thread %d\n", n_op_samples[my_id], my_id);
        	fprintf(stderr, "n_lost_op_samples: %d\n", n_lost_op_samples[my_id]);

	}
	//while (!waitpid(cpid, &i, WNOHANG));

#if 0	
	for (int i = 0; i < nopfds; i++) {
        	ioctl(fd[i], IBS_DISABLE);
//END:
		close(fd[i]);
    	}
#endif

	free(fd);
	//fprintf(stderr, "no problem until this point, nopfds: %d, sum: %ld\n", nopfds, sum);
	//fprintf(stderr, "n_op_samples: %d\n", n_op_samples);
	//fprintf(stderr, "n_lost_op_samples: %d\n", n_lost_op_samples);
	return 0;
	// print results here

	/*int fd[4];
	char option;
	int number[4];
	struct sigaction act;

	printf("Welcome to the demo of character device driver...\n");

	sigemptyset(&act.sa_mask);
    	act.sa_flags = (SA_SIGINFO | SA_RESTART);
    	act.sa_sigaction = sig_event_handler;
    	sigaction(SIGNEW, &act, NULL);

	fd[0] = open("/dev/my_device0", O_RDWR);
	if(fd[0] < 0) {
		printf("Cannot open device file...\n");
		return 0;
	}

	if (ioctl(fd[0], REG_CURRENT_PROCESS,(int32_t*) &number[0])) {
        	printf("Failed\n");
        	close(fd[0]);
        	exit(1);
    	}


	fd[1] = open("/dev/my_device1", O_RDWR);
        if(fd[1] < 0) {
                printf("Cannot open device file...\n");
                return 0;
        }

        if (ioctl(fd[1], REG_CURRENT_PROCESS,(int32_t*) &number[1])) {
                printf("Failed\n");
                close(fd[1]);
                exit(1);
        }

	fd[2] = open("/dev/my_device2", O_RDWR);
        if(fd[2] < 0) {
                printf("Cannot open device file...\n");
                return 0;
        }

        if (ioctl(fd[2], REG_CURRENT_PROCESS,(int32_t*) &number[2])) {
                printf("Failed\n");
                close(fd[2]);
                exit(1);
        }

	fd[3] = open("/dev/my_device3", O_RDWR);
        if(fd[3] < 0) {
                printf("Cannot open device file...\n");
                return 0;
        }

        if (ioctl(fd[3], REG_CURRENT_PROCESS,(int32_t*) &number[3])) {
                printf("Failed\n");
                close(fd[3]);
                exit(1);
        }

	while(1) {
		printf("*****please enter your option*****\n");
		printf("	1. Write	\n");
		printf("	2. Read		\n");
		printf("	3. Exit		\n");
		scanf("		%c",&option);
		printf("	your options = %c\n", option);

		switch(option) {
			case '1':
				printf("Enter the string to write into the driver:\n");
				scanf("	%[^\t\n]s", write_buf);
				printf("Data written....");
				write(fd[1], write_buf, strlen(write_buf)+1);
				printf("DONE...\n");
				break;
			case '2':
				printf("Data is Reading...");
				read(fd[1], read_buf, 1024);
				printf("Done...\n\n");
				printf("Data = %s\n\n", read_buf);
				break;
			case '3':
				close(fd[0]);
				close(fd[1]);
				close(fd[2]);
				close(fd[3]);
				exit(1);
				break;
			default:
				printf("Enter valid option = %c\n", option);
				break;
		}
	}
	close(fd[0]);
	close(fd[1]);
	close(fd[2]);
	close(fd[3]);*/
}
