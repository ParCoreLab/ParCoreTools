#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-interval-arg.h"
#include <sample-sources/watchpoint_support.h>
#include <lib/isa-lean/x86/instruction-set.h>
#include "fnbounds_interface.h"
#include<stdint.h>

#define MAX_BYTES_TO_DECODE (15)

static __thread xed_state_t xedState;
static __thread bool xedInited = false;

static xed_uint64_t watchpointRegisterValueCallback(xed_reg_enum_t reg, void *context, xed_bool_t *error)
{
	ucontext_t *ctxt = (ucontext_t*)context;

	*error = 0;
	switch(reg) {
	case XED_REG_RAX:
	case XED_REG_EAX:
	case XED_REG_AX:
	case XED_REG_AL:
		return ctxt->uc_mcontext.gregs[REG_RAX];
	case XED_REG_AH: assert(0 && "NYI"); *error=1;

	case XED_REG_RCX:
	case XED_REG_ECX:
	case XED_REG_CX:
		return ctxt->uc_mcontext.gregs[REG_RCX];
	case XED_REG_CH: assert(0 && "NYI"); *error=1;

	case XED_REG_RDX:
	case XED_REG_EDX:
	case XED_REG_DX:
		return ctxt->uc_mcontext.gregs[REG_RDX];
	case XED_REG_DH: assert(0 && "NYI"); *error=1;

	case XED_REG_RBX:
	case XED_REG_EBX:
	case XED_REG_BX:
		return ctxt->uc_mcontext.gregs[REG_RBX];
	case XED_REG_BH: assert(0 && "NYI"); *error=1;

	case XED_REG_RSP:
	case XED_REG_ESP:
	case XED_REG_SP:
		return ctxt->uc_mcontext.gregs[REG_RSP];

	case XED_REG_RBP:
	case XED_REG_EBP:
	case XED_REG_BP:
		return ctxt->uc_mcontext.gregs[REG_RBP];

	case XED_REG_RSI:
	case XED_REG_ESI:
	case XED_REG_SI:
		return ctxt->uc_mcontext.gregs[REG_RSI];
	case XED_REG_RDI:
	case XED_REG_EDI:
	case XED_REG_DI:
		return ctxt->uc_mcontext.gregs[REG_RDI];

	case XED_REG_R8:
	case XED_REG_R8D:
	case XED_REG_R8W:
		return ctxt->uc_mcontext.gregs[REG_R8];

	case XED_REG_R9:
	case XED_REG_R9D:
	case XED_REG_R9W:
		return ctxt->uc_mcontext.gregs[REG_R9];

	case XED_REG_R10:
	case XED_REG_R10D:
	case XED_REG_R10W:
		return ctxt->uc_mcontext.gregs[REG_R10];

	case XED_REG_R11:
	case XED_REG_R11D:
	case XED_REG_R11W:
		return ctxt->uc_mcontext.gregs[REG_R11];

	case XED_REG_R12:
	case XED_REG_R12D:
	case XED_REG_R12W:
		return ctxt->uc_mcontext.gregs[REG_R12];

	case XED_REG_R13:
	case XED_REG_R13D:
	case XED_REG_R13W:
		return ctxt->uc_mcontext.gregs[REG_R13];

	case XED_REG_R14:
	case XED_REG_R14D:
	case XED_REG_R14W:
		return ctxt->uc_mcontext.gregs[REG_R14];

	case XED_REG_R15:
	case XED_REG_R15D:
	case XED_REG_R15W:
		return ctxt->uc_mcontext.gregs[REG_R15];

	case XED_REG_EFLAGS:
		return ctxt->uc_mcontext.gregs[REG_EFL];

	case XED_REG_RIP:
	case XED_REG_EIP:
	case XED_REG_IP:
		return ctxt->uc_mcontext.gregs[REG_RIP];

	case XED_REG_DS: *error=1; assert(0 && "NYI"); break;
	case XED_REG_ES: *error=1; assert(0 && "NYI"); break;
	case XED_REG_SS: *error=1; assert(0 && "NYI"); break;

	case XED_REG_CS:
	 	/* Linux stores CS, GS, FS, PAD into one 64b word. */
		return (uint32_t) (ctxt->uc_mcontext.gregs[REG_CSGSFS] & 0xFFFF);
	case XED_REG_FS:
		return (uint32_t) ((ctxt->uc_mcontext.gregs[REG_CSGSFS] >> 32) & 0xFFFF);
	case XED_REG_GS:
		return (uint32_t) ((ctxt->uc_mcontext.gregs[REG_CSGSFS] >> 16) & 0xFFFF);
	default:
		*error=1;
#if 0
				 assert(0 && "NYI");
				 //TODO:This failing YMM gather instruction.
				 NWChem   0xb5ec0c <conv2x_+828>:      vgatherqpd %ymm7,(%r9,%ymm1,8),%ymm3

#endif
            return 0; //FIXME;
	}
}


static inline void XedInit()
{
	xed_tables_init();
	xed_state_init (&xedState, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b, XED_ADDRESS_WIDTH_64b);
	xed_agen_register_callback (watchpointRegisterValueCallback, watchpointRegisterValueCallback);
	xedInited = true;
}

static inline xed_error_enum_t decode(xed_decoded_inst_t *xedd, void *ip)
{
	if(!xedInited){
		XedInit();
	}
	xed_decoded_inst_zero_set_mode(xedd, &xedState);
	if(XED_ERROR_NONE != xed_decode(xedd, (const xed_uint8_t*)(ip), MAX_BYTES_TO_DECODE)) {
		printf("get_len_float_operand failed to disassemble instruction\n");
		return -1;
	}
	return XED_ERROR_NONE;
}

unsigned int get_float_operation_length(void *ip, uint8_t op_idx)
{
	xed_decoded_inst_t xedd;
	if(XED_ERROR_NONE != decode(&xedd, ip)) {
		return 0;
	}
	xed_operand_element_type_enum_t op_type = xed_decoded_inst_operand_element_type(&xedd, op_idx);
	switch(op_type) {
	case XED_OPERAND_ELEMENT_TYPE_FLOAT16: return 2;
	case XED_OPERAND_ELEMENT_TYPE_SINGLE: return 4;
	case XED_OPERAND_ELEMENT_TYPE_DOUBLE: return 8;
	case XED_OPERAND_ELEMENT_TYPE_LONGDOUBLE: return 10;
	default: /* does not look like a float */ return 0;
	}
#if 0 
	char buf[1000] = {0};
	xed_decoded_inst_dump_att_format(&xedd, buf, 1000, 0);
	printf("\n %p: %s \n", ip, buf);      
	fflush(stdout);
	assert(0 && "float instruction with unknown operand\n");
#endif
}


AccessType get_access_type(void * ip)
{
    xed_decoded_inst_t xedd;
    if(XED_ERROR_NONE != decode(&xedd, ip)) {
        return UNKNOWN;
    }
    xed_uint_t numMemOps = xed_decoded_inst_number_of_memory_operands(&xedd);
    if (numMemOps == 0) {
        //TODO: Milind: Handle XED_CATEGORY_STRINGOP that have 2 mem ops
        //printf("get_mem_access_length_and_type: numMemOps=%d, pc = %p\n",numMemOps, ip);
        return UNKNOWN;
    }
    
    xed_bool_t isOpRead = xed_decoded_inst_mem_read(&xedd, 0);
    xed_bool_t isOpWr = xed_decoded_inst_mem_written(&xedd, 0);
	if( isOpRead && isOpWr)
		return LOAD_AND_STORE;
	if (isOpRead)
		return LOAD;
	return STORE;
}

// if return a non-zero value, this ip is write only to the memory
bool get_mem_access_length_and_type_address(void * ip, uint32_t *accessLen, AccessType *accessType, FloatType * floatType, void * context, void** address)
{
	xed_decoded_inst_t xedd;
	if(XED_ERROR_NONE != decode(&xedd, ip)) {
		return false;
	}
	xed_uint_t numMemOps = xed_decoded_inst_number_of_memory_operands(&xedd);
	if (numMemOps != 1) {
		//TODO: Milind: Handle XED_CATEGORY_STRINGOP that have 2 mem ops
		//printf("get_mem_access_length_and_type: numMemOps=%d, pc = %p\n",numMemOps, ip);
		return false;
	}

	xed_bool_t isOpRead =  xed_decoded_inst_mem_read(&xedd, 0);
	xed_bool_t isOpWritten =  xed_decoded_inst_mem_written(&xedd, 0);
	*accessLen = xed_decoded_inst_get_memory_operand_length(&xedd, 0);
	if (isOpWritten && isOpRead)
		*accessType = LOAD_AND_STORE;
	else if (isOpWritten)
		*accessType = STORE;
	else if (isOpRead)
		*accessType = LOAD;
	else
		*accessType = UNKNOWN;

	if (floatType){
		xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);
		switch (cat) {
		case XED_CATEGORY_AES:
		case XED_CATEGORY_CONVERT:
		case XED_CATEGORY_PCLMULQDQ:
		case XED_CATEGORY_SSE:
		case XED_CATEGORY_AVX2:
		case XED_CATEGORY_AVX:
		case XED_CATEGORY_MMX:
		case XED_CATEGORY_DATAXFER: {
			// Get the mem operand
			const xed_inst_t* xi = xed_decoded_inst_inst(&xedd);
			int  noperands = xed_inst_noperands(xi);
			int memOpIdx = -1;
			for( int i=0; i<noperands ; i++) {
				const xed_operand_t* op = xed_inst_operand(xi,i);
				xed_operand_enum_t op_name = xed_operand_name(op);
				if(XED_OPERAND_MEM0 == op_name) {
					memOpIdx = i;
					break;
				}
			}
			if(memOpIdx == -1) {
				*floatType = ELEM_TYPE_UNKNOWN;
				goto SkipTypeDetection;
			}
			// TODO: case XED_OPERAND_MEM1:
			xed_operand_element_type_enum_t eType = xed_decoded_inst_operand_element_type(&xedd,memOpIdx);
			switch (eType) {
			case XED_OPERAND_ELEMENT_TYPE_FLOAT16:
				*floatType = ELEM_TYPE_FLOAT16;
				break;
			case XED_OPERAND_ELEMENT_TYPE_SINGLE:
				*floatType = ELEM_TYPE_SINGLE;
				break;
			case XED_OPERAND_ELEMENT_TYPE_DOUBLE:
				*floatType = ELEM_TYPE_DOUBLE;
				break;
			case XED_OPERAND_ELEMENT_TYPE_LONGDOUBLE:
				*floatType = ELEM_TYPE_LONGDOUBLE;
				break;
			case XED_OPERAND_ELEMENT_TYPE_LONGBCD:
				*floatType = ELEM_TYPE_LONGBCD;
				break;
			default:
				*floatType = ELEM_TYPE_UNKNOWN;
			}
		}
		break;
		case XED_CATEGORY_X87_ALU:
		case XED_CATEGORY_FCMOV:
		//case XED_CATEGORY_LOGICAL_FP:
		// assumption, the access length must be either 4 or 8 bytes else assert!!!
		//assert(*accessLen == 4 || *accessLen == 8);
			switch(*accessLen) {
			case 4: *floatType = ELEM_TYPE_SINGLE; break;
			case 8:  *floatType = ELEM_TYPE_DOUBLE; break;
			default: *floatType = ELEM_TYPE_UNKNOWN;
			}
			break;
		case XED_CATEGORY_XSAVE:
		case XED_CATEGORY_AVX2GATHER:
		case XED_CATEGORY_STRINGOP:
		default:
			break;
		}
	}
SkipTypeDetection:
	if(!address)
		return true;

	if (XED_ERROR_NONE != xed_agen (&xedd, 0 /* memop idx*/, context, (xed_uint64_t *) (address)))  {
		return false;
	}
	return true;
}

bool get_mem_access_length_and_type(void * ip, uint32_t *accessLen, AccessType *accessType)
{
	return get_mem_access_length_and_type_address(ip, accessLen, accessType, 0, /*nofloat type */ 0 /* context */, 0 /* address */);
}

FunctionType is_same_function(void *ins1, void* ins2)
{
    if (ins1 == ins2)
        return SAME_FN;
    
    void *fn_start=NULL, *fn_end=NULL;
    if (!fnbounds_enclosing_addr(ins1, &fn_start, &fn_end, NULL))
        return UNKNOWN_FN; // failed to find
    if (ins1 <= fn_start || ins1 > fn_end)
        return UNKNOWN_FN; // outside of proposed
    if (ins2 <= fn_start || ins2 > fn_end)
        return DIFF_FN; // different
    return SAME_FN; // same
}
void * get_previous_instruction(void *ins, void **pip, void ** excludeList, int numExcludes)
{
	void *fn_start=NULL, *fn_end=NULL;
	*pip = NULL;
	if (!fnbounds_enclosing_addr(ins, &fn_start, &fn_end, NULL))
        return 0;
	if (ins <= fn_start || ins > fn_end)
        return 0 ;

    // Cannot touch addresses that are being monitored by a watchpoint
    for(int idx = 0; idx < numExcludes; idx++){
        if((fn_start <= excludeList[idx]) && (excludeList[idx] < ins))
            return 0;
    }

	xed_decoded_inst_t xedd;
	xed_decoded_inst_t *xptr = &xedd;
	xed_error_enum_t xed_error;

	if(!xedInited)
		XedInit();

	void *pins = fn_start;
	xed_decoded_inst_zero_set_mode(xptr, &xedState);
	while (pins <= fn_end) {
		xed_decoded_inst_zero_keep_mode(xptr);
		xed_error = xed_decode(xptr, (uint8_t*)pins, MAX_BYTES_TO_DECODE);
		if (xed_error != XED_ERROR_NONE) {
			pins++;
			continue;
		}
		xed_uint_t len = xed_decoded_inst_get_length(xptr);
		if(pins + len == ins) {
			*pip = pins;
			return pins;
		}
		pins += len;
	}
	return 0;
}

