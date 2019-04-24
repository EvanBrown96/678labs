/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "buddy.h"
#include "list.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12
#define MAX_ORDER 20

#define PAGE_SIZE (1<<MIN_ORDER)
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
	int index;
	int is_free;
	int block_size;
} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];

/* memory area */
char g_memory[1<<MAX_ORDER];

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/

void buddy_split(int order){

	// get first free page of the desired size
	page_t* free_page = list_entry(free_area[order].next, page_t, list);
	// remove it from free list of this size
	list_del(free_area[order].next);

	// find its buddy
	void* buddy_addr = BUDDY_ADDR(PAGE_TO_ADDR(free_page->index), (order-1));
	page_t* buddy_page = &(g_pages[ADDR_TO_PAGE(buddy_addr)]);

	// set block size of both pages
	free_page->block_size = order-1;
	buddy_page->block_size = order-1;

	// add both to free list of one less order
	list_add(&(buddy_page->list), &free_area[order-1]);
	list_add(&(free_page->list), &free_area[order-1]);

}

page_t* buddy_combine(page_t* the_page, page_t* buddy_page){

	// give back original page if its buddy is not free
	if(!buddy_page->is_free) return the_page;

	// remove buddy from free list
	list_del(&(buddy_page->list));

	// swap order of blocks if necessary
	if(buddy_page->index < the_page->index){
		page_t* store = buddy_page;
		buddy_page = the_page;
		the_page = store;
	}

	// increase size of block corresponding with this page
	the_page->block_size++;

	buddy_page = &(g_pages[ADDR_TO_PAGE(BUDDY_ADDR(PAGE_TO_ADDR(the_page->index), the_page->block_size))]);
	return buddy_combine(the_page, buddy_page);

}

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;
	for (i = 0; i < n_pages; i++) {
		//INIT_LIST_HEAD(&(g_pages[i].list));
		g_pages[i].index = i;
		g_pages[i].is_free = 1;
		g_pages[i].block_size = 0;
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) {
		INIT_LIST_HEAD(&free_area[i]);
	}

	g_pages[0].block_size = MAX_ORDER;
	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);
}

/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{

	// get smallest block size that will satisfy request
	int needed_block = MIN_ORDER;
	while((1 << needed_block) < size) needed_block++;
	if(needed_block > MAX_ORDER) return NULL;

	// find smallest open block past this point
	int o = needed_block;
	while(list_empty(&free_area[o])){
		// no free memory, if we reached the end
		if(o >= MAX_ORDER) return NULL;
		o++;
	}

	// split blocks until we have the desired size
	while(o > needed_block){
		buddy_split(o);
		o--;
	}

	// get first page of free area
	page_t* free_page = list_entry(free_area[o].next, page_t, list);
	// remove it from free list
	list_del(free_area[o].next);
	// mark page as not free
	free_page->is_free = 0;

	// get address of page
	void* addr = PAGE_TO_ADDR(free_page->index);
	return addr;
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{
	// get the information of the page corresponding to the given addr
	page_t* page_to_free = &(g_pages[ADDR_TO_PAGE(addr)]);
	page_to_free->is_free = 1;

	page_t* buddy_page = &(g_pages[ADDR_TO_PAGE(BUDDY_ADDR(addr, page_to_free->block_size))]);

	page_t* res_page = buddy_combine(page_to_free, buddy_page);

	struct list_head* free_head = &free_area[res_page->block_size];
	void* insert_loc = free_head;
	page_t* test_page;
	list_for_each_entry(test_page, free_head, list){
		if(res_page->index < test_page->index){
			insert_loc = &(test_page->list);
			break;
		}
	}

	list_add_tail(&(res_page->list), insert_loc);

}

/**
 * Print the buddy system status---order oriented
 *
 * print free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
