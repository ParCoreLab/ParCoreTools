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

#define SIGNEW 44
#define SET_BUFFER_SIZE 0xEU
// 2048 bytes
#define BUFFER_SIZE_B   (1 << 20)
#define SET_MAX_CNT     0x6U
#define OP_MAX_CNT  0x4000
#define IBS_ENABLE      0x0U
#define IBS_DISABLE     0x1U
#define RESET_BUFFER    0x10U
#define GET_LOST        0xEEU
#define SET_CUR_CNT     0x2U
#define GET_CUR_CNT     0x3U
#define GET_MAX_CNT     0x7U
#define IBS_CTL_BACKUP  0x11U
#define IBS_CTL_RELOAD  0x12U

#define REG_CURRENT_PROCESS _IOW('a', 'a', int32_t*)
#define ASSIGN_FD 102

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
	int                     micro_op_sample;
	int                     mem_access_sample;
        int                     valid_mem_access_sample;
} ibs_op_t;


