#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <dlfcn.h>
//#include <cstdint>
#include <string.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#if ADAMANT_USED
#include <adm_init_fini.h>
#endif
#include "env.h"
//#define ENABLE_OBJECT_LEVEL 1
#include <sys/mman.h>
//#include <execinfo.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "myposix.h"
#include <pthread.h>
#include <stdlib.h>

static char empty_data[2048];

int empty_pos = 0;

int init_calloc = 0;

int OBJECT_THRESHOLD;

int init_adamant = 0;

void postorder(nary_node * p, int indent)
{
	if(p != NULL) {
		postorder(p->first_child, indent+4);
		nary_node * p1 = p;
		while (p1->next_sibling != NULL) {
			p1 = p1->next_sibling;
			postorder(p1->first_child, indent+4);
			printf("%lx id: %d ", p1->address, p1->node_id);
		}
		for(int i = 0; i < indent; i++) {
			printf(" ");
		}
		printf("%lx id: %d\n", p->address, p->node_id);
	}
}



nary_node * tree_root;

int id_count;

static pthread_mutex_t nary_tree_lock = PTHREAD_MUTEX_INITIALIZER;

static void* (*real_malloc)(size_t)=NULL;

int insert_call_path_to_nary_tree (uint64_t * call_path, int call_path_size) {
	int leaf_id;
	//fprintf(stderr, "begins\n");
	if(tree_root == NULL) {
		id_count = 1001;
		tree_root = (nary_node *) real_malloc (sizeof(nary_node));
		tree_root->node_id = id_count++;
		tree_root->parent = NULL;
		tree_root->first_child = NULL;
		tree_root->next_sibling = NULL;
	}
	nary_node * node;
	nary_node * parent = tree_root;
	pthread_mutex_lock(&nary_tree_lock);
	if(tree_root->first_child != NULL) {
		node = tree_root->first_child;
	} else {
		tree_root->first_child = (nary_node *) real_malloc (sizeof(nary_node));
		tree_root->first_child->parent = tree_root;
		tree_root->first_child->first_child = NULL;
		tree_root->first_child->next_sibling = NULL;
		node = tree_root->first_child;
		node->address = 0;
	}
	for(int i = 0; i < call_path_size; i++) {
		if(node->address == 0) {
			node->address = call_path[i];
			node->node_id = id_count++;
		} else {
			while (node->address != call_path[i] && node->next_sibling != NULL) {
				node = node->next_sibling;
			}
			if(node->next_sibling == NULL && node->address != call_path[i]) {
				node->next_sibling = (nary_node *) real_malloc (sizeof(nary_node));
				node->next_sibling->parent = node->parent;
				node = node->next_sibling;
				node->address = call_path[i];
				node->node_id = id_count++;
				node->next_sibling = NULL;
				node->first_child = NULL;
			}
		}
		if(i+1 < call_path_size && node->first_child == NULL) {
			node->first_child = (nary_node *) real_malloc (sizeof(nary_node));
			node->first_child->parent = node;
			node->first_child->address = 0;
			node->first_child->first_child = NULL;
			node->first_child->next_sibling = NULL;
		}
		parent = node;
		node = node->first_child;	
	}
	//fprintf(stderr, "ends\n");
	pthread_mutex_unlock(&nary_tree_lock);
	return parent->node_id;
}

int get_id_after_backtrace() {
	unw_cursor_t cursor;
	unw_context_t context;

	// Initialize cursor to current frame for local unwinding.
	unw_getcontext(&context);
	unw_init_local(&cursor, &context);

	uint64_t upward_sequence[100];
	int func_count = 0, stack_size;
	// Unwind frames one by one, going up the frame stack.
	pid_t tid = syscall(__NR_gettid);
	//fprintf(stderr, "backtrace starts in thread: %d\n", tid);
	while (unw_step(&cursor) > 0) {
		unw_word_t offset, pc;
		unw_get_reg(&cursor, UNW_REG_IP, &pc);
		if (pc == 0) {
			break;
		}
		//fprintf(stderr, "0x%lx:\n", pc);

		char sym[256];
		if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
			//fprintf(stderr, "function:%lx (%s+0x%lx) in thread %d\n", pc-offset, sym, offset, tid);
			if(strlen(sym) >= 8 && strncmp(sym, "OnSample", 8) == 0)
				//fprintf(stderr, "OnSample is detected\n");
				continue;
			if(strlen(sym) >= 18 && strncmp(sym, "perf_event_handler", 18) == 0)
				//fprintf(stderr, "perf_event_handler is detected\n");
				continue;
			if(strlen(sym) >= 22 && strncmp(sym, "monitor_signal_handler", 22) == 0)
				//fprintf(stderr, "monitor_signal_handler is detected\n");
				continue;
			if(strlen(sym) >= 6 && strncmp(sym, "killpg", 6) == 0)
				//fprintf(stderr, "killpg is detected\n");
				continue;
			if(strlen(sym) >= 22 && strncmp(sym, "ComDetectiveWPCallback", 22) == 0)
				//fprintf(stderr, "ComDetectiveWPCallback is detected\n");
				continue;
			if(strlen(sym) >= 12 && strncmp(sym, "OnWatchPoint", 12) == 0)
				//fprintf(stderr, "OnWatchPoint is detected\n");
				continue;
			//fprintf(stderr, "function:%lx (%s+0x%lx) in thread %d\n", pc-offset, sym, offset, tid);
			upward_sequence[func_count] = pc - offset;
		} else {
			upward_sequence[func_count] = pc;	
			//fprintf(stderr, " -- error: unable to obtain symbol name for this frame\n");
		}
		func_count++;
	}
	stack_size = func_count;
	//fprintf(stderr, "\n");
	uint64_t downward_sequence[100];
	int i = 0;
	while(func_count > 0) {
		downward_sequence[i] = upward_sequence[func_count - 1];
		func_count--;
		i++;
	}
	//fprintf(stderr, "before\n");
	return insert_call_path_to_nary_tree (downward_sequence, stack_size);
	//printf(stderr, "after\n");
}

static void* (*real_calloc)(size_t, size_t)=NULL;

static void malloc_init(void)
{
	real_malloc = dlsym(RTLD_NEXT, "malloc");
	if (NULL == real_malloc) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void (*real_free)(void*)=NULL;

static void free_init(void)
{
	real_free = dlsym(RTLD_NEXT, "free");
	if (NULL == real_free) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void calloc_init(void)
{
	real_malloc = dlsym(RTLD_NEXT, "malloc");
	if (NULL == real_malloc) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_realloc)(void*, size_t)=NULL;

static void realloc_init(void)
{
	real_realloc = dlsym(RTLD_NEXT, "realloc");
	if (NULL == real_realloc) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static int (*real_posix_memalign)(void**, size_t, size_t)=NULL;

static void posix_memalign_init(void)
{
	real_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");
	if (NULL == real_posix_memalign) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_memalign)(size_t, size_t)=NULL;

static void memalign_init(void)
{
	real_memalign = dlsym(RTLD_NEXT, "memalign");
	if (NULL == real_memalign) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_aligned_alloc)(size_t, size_t)=NULL;

static void aligned_alloc_init(void)
{
	real_aligned_alloc = dlsym(RTLD_NEXT, "aligned_alloc");
	if (NULL == real_aligned_alloc) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_valloc)(size_t)=NULL;

static void valloc_init(void)
{
	real_valloc = dlsym(RTLD_NEXT, "valloc");
	if (NULL == real_valloc) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_pvalloc)(size_t)=NULL;

static void pvalloc_init(void)
{
	real_pvalloc = dlsym(RTLD_NEXT, "pvalloc");
	if (NULL == real_pvalloc) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_mmap)(void *, size_t, int, int, int, off_t)=NULL;

static void mmap_init(void)
{
	real_mmap = dlsym(RTLD_NEXT, "mmap");
	if (NULL == real_mmap) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_mmap64)(void *, size_t, int, int, int, off_t)=NULL;

static void mmap64_init(void)
{
	real_mmap64 = dlsym(RTLD_NEXT, "mmap64");
	if (NULL == real_mmap64) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_numa_alloc_onnode)(size_t size, size_t node)=NULL;

static void numa_alloc_onnode_init(void)
{
	real_numa_alloc_onnode = dlsym(RTLD_NEXT, "numa_alloc_onnode");
	if (NULL == real_numa_alloc_onnode) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

static void* (*real_numa_alloc_interleaved)(size_t size)=NULL;

static void numa_alloc_interleaved_init(void)
{
	real_numa_alloc_interleaved = dlsym(RTLD_NEXT, "numa_alloc_interleaved");
	if (NULL == real_numa_alloc_interleaved) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
	}
}

#if ADAMANT_USED
void *malloc(size_t size)
{
	//fprintf(stderr, "in malloc\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			fprintf(stderr, "adamant is initialized\n");
			adm_initialize();
		}
	}
	//fprintf(stderr, " after in malloc\n");
	if(real_malloc==NULL) {
		malloc_init();
	}
	//fprintf(stderr, " after in malloc 2\n");
	void *p = NULL;
	//fprintf(stderr, "malloc(%ld)\n", size);
	p = real_malloc(size);
	//fprintf(stderr, " after in malloc 3\n");
	//malloc_adm(p, size);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (size > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			//fprintf(stderr, "inserted node id: %d\n", node_id);
			malloc_adm(p, size, node_id);
		}
	}
	return p;
}

void *calloc(size_t nmemb, size_t size)
{
	//fprintf(stderr, "in calloc\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}
	void *p = NULL;
	//fprintf(stderr, " after in calloc\n");
	if(real_malloc==NULL) {
		//fprintf(stderr, "calloc is initialized**********\n");
		init_calloc = 1;
		p = &(empty_data[empty_pos]);
		empty_pos += nmemb * size;
		//sleep(1);
	} else {
		p = real_malloc(nmemb * size);
	}
	//fprintf(stderr, " after in calloc 2 %lx pos: %d\n", p, empty_pos);
	//fprintf(stderr, "calloc(%l)\n", size);

	if (p != NULL) {
		memset(p, 0, nmemb * size);
		//sleep(1);
	}
	//fprintf(stderr, " after in calloc 3\n");
	//malloc_adm(p, size);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (nmemb * size > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			//fprintf(stderr, "inserted node id: %d\n", node_id);
			malloc_adm(p, nmemb * size, node_id);
		}
	}
	return p;
}

void free(void* ptr)
{
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_free==NULL) {
		free_init();
	}

	//fprintf(stderr, "address %p is freed\n", ptr);
	real_free(ptr);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		free_adm(ptr);
	}
}

int posix_memalign(void** memptr, size_t alignment, size_t size)
{
	//fprintf(stderr, "in posix_memalign\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_posix_memalign==NULL) {
		posix_memalign_init();
	}

	if(real_malloc==NULL) {
		malloc_init();
	}

	int p;
	p = real_posix_memalign(memptr, alignment, size);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (size > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			posix_memalign_adm(p, memptr, alignment, size, node_id);
		}
	}
	return p;
}

void* memalign(size_t alignment, size_t size)
{
	//fprintf(stderr, "in memalign\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_memalign==NULL) {
		memalign_init();
	}

	if(real_malloc==NULL) {
		malloc_init();
	}

	void* p;
	p = real_memalign(alignment, size);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (size > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			//backtrace();
			//fprintf(stderr, "inserted node id in memalign: %d\n", node_id);
			memalign_adm(p, size, node_id);
		}
	}
	return p;
}

void* aligned_alloc(size_t alignment, size_t size)
{
	//fprintf(stderr, "in aligned_alloc\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_aligned_alloc==NULL) {
		aligned_alloc_init();
	}

	if(real_malloc==NULL) {
		malloc_init();
	}

	void* p;
	p = real_aligned_alloc(alignment, size);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (size > OBJECT_THRESHOLD)) {
			//backtrace();
			int node_id = get_id_after_backtrace();
			aligned_alloc_adm(p, size, node_id);
		}
	}
	return p;
}


void* valloc(size_t size)
{
	//fprintf(stderr, "in valloc\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_valloc==NULL) {
		valloc_init();
	}

	if(real_malloc==NULL) {
		malloc_init();
	}

	void* p;
	p = real_valloc(size);
	//fprintf(stderr, "valloc: %lx\n", (long unsigned int) p);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (size > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			valloc_adm(p, size, node_id);
		}
	}
	return p;
}

void* pvalloc(size_t size)
{
	//fprintf(stderr, "in pvalloc\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_pvalloc==NULL) {
		pvalloc_init();
	}

	if(real_malloc==NULL) {
		malloc_init();
	}

	void* p;
	//fprintf(stderr, "pvalloc(%ld) = ", size);
	p = real_pvalloc(size);
	//fprintf(stderr, "pvalloc: %lx\n", (long unsigned int) p);
	//fprintf(stderr, "%p\n", p);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (size > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			pvalloc_adm(p, size, node_id);
		}
	}
	return p;
}


void *numa_alloc_onnode(size_t size, size_t node) {
	//fprintf(stderr, "in numa_alloc_onnode\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_numa_alloc_onnode == NULL) {
		numa_alloc_onnode_init();
	}

	if(real_malloc==NULL) {
		malloc_init();
	}

	void* p;
	p = real_numa_alloc_onnode(size, node);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (size > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			numa_alloc_onnode_adm(p, size, node_id);
		}
	}
	return p;
}

void *numa_alloc_interleaved(size_t size) {
	//fprintf(stderr, "in numa_alloc_interleaved\n");
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_numa_alloc_interleaved == NULL) {
		numa_alloc_interleaved_init();
	}

	if(real_malloc==NULL) {
		malloc_init();
	}

	void* p;
	p = real_numa_alloc_interleaved(size);
	//fprintf(stderr, "numa_alloc_interleaved: %lx\n", (long unsigned int) p);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (size > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			numa_alloc_interleaved_adm(p, size, node_id);
		}
	}
	return p;
}

void *mmap64(void *start, size_t length, int prot, int flags, int fd, off_t offset) {

	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(!init_adamant) {
			init_adamant = 1;
			adm_initialize();
		}
	}

	if(real_mmap64 == NULL) {
		mmap64_init();
	}

	if(real_malloc==NULL) {
		malloc_init();
	}

	void* p;
	p = real_mmap64(start, length, prot, flags, fd, offset);
	//fprintf(stderr, "mmap64: %lx\n", (long unsigned int) p);
	if (getenv(HPCRUN_OBJECT_LEVEL)) {
		if(real_malloc && (length > OBJECT_THRESHOLD)) {
			int node_id = get_id_after_backtrace();
			mmap64_adm(p, length, node_id);
		}
	}
	return p;
}
#endif
