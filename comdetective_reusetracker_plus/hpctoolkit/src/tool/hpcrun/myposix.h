#include <stdint.h>
void pointers_init(void);

extern int OBJECT_THRESHOLD;

int get_id_after_backtrace();

struct nary_node {
	uint64_t address;
	int node_id;
	struct nary_node* parent;
	struct nary_node* first_child; //Points to the first node on the next level down
	struct nary_node* next_sibling; //Points to the next node on the same level
};

typedef struct nary_node nary_node;
