/** @file libpriqueue.c
 */

#include <stdlib.h>
#include <stdio.h>

#include "libpriqueue.h"

#define INITIAL_SIZE 10
#define FALSE 0
#define TRUE 1

/**
  Initializes the priqueue_t data structure.

  Assumtions
    - You may assume this function will only be called once per instance of priqueue_t
    - You may assume this function will be the first function called using an instance of priqueue_t.
  @param q a pointer to an instance of the priqueue_t data structure
  @param comparer a function pointer that compares two elements.
  See also @ref comparer-page
 */
void priqueue_init(priqueue_t *q, int(*comparer)(const void *, const void *))
{
  q->data = malloc(sizeof(void*)*INITIAL_SIZE);
  q->count = 0;
  q->size = INITIAL_SIZE;
  q->start = 0;
  q->comparer = comparer;
}

int priqueue_get_true_index(priqueue_t *q, int index)
{
  return (index+q->start)%q->size;
}

void priqueue_resize(priqueue_t *q)
{
  void** old_data = q->data;
  q->data = malloc(sizeof(void*)*(q->size*2+1));
  for(int i = 0; i < q->count; i++){
    q->data[i] = old_data[priqueue_get_true_index(q, i)];
  }
  free(old_data);
  q->size = q->size*2+1;
  q->start = 0;
}

void priqueue_swap(priqueue_t *q, int index1, int index2)
{
  void* save = q->data[index1];
  q->data[index1] = q->data[index2];
  q->data[index2] = save;
}

/**
  Inserts the specified element into this priority queue.

  @param q a pointer to an instance of the priqueue_t data structure
  @param ptr a pointer to the data to be inserted into the priority queue
  @return The zero-based index where ptr is stored in the priority queue, where 0 indicates that ptr was stored at the front of the priority queue.
 */
int priqueue_offer(priqueue_t *q, void *ptr)
{
  if(q->count == q->size) priqueue_resize(q);

  // find index where data should be inserted
  int index = 0;
  while(index < q->count && q->comparer(ptr, q->data[priqueue_get_true_index(q, index)]) >= 0){
    index++;
  }

  // put new datum at very end of good data
  q->data[priqueue_get_true_index(q, q->count)] = ptr;

  // move new data to desired index
  for(int i = q->count; i>index; i--){
    priqueue_swap(q, priqueue_get_true_index(q, i), priqueue_get_true_index(q, i-1));
  }

  q->count++;
  return index;

}

/**
  adds the specified element at the very end of the priority queue

  @param q a pointer to an instance of the priqueue_t data structure
  @param ptr a pointer to the data to be inserted at the end of the queue
  @return The zero-based index where ptr is stored in the priority queue, where 0 indicates that ptr was stored at the front of the priority queue.
 */
int priqueue_force_end(priqueue_t *q, void *ptr){
  if(q->count == q->size) priqueue_resize(q);

  q->data[priqueue_get_true_index(q, q->count)] = ptr;

  q->count++;
  return (q->count)-1;
}

/**
  Retrieves, but does not remove, the head of this queue, returning NULL if
  this queue is empty.

  @param q a pointer to an instance of the priqueue_t data structure
  @return pointer to element at the head of the queue
  @return NULL if the queue is empty
 */
void *priqueue_peek(priqueue_t *q)
{
  if(priqueue_size(q) == 0) return NULL;

  return q->data[priqueue_get_true_index(q, 0)];
}


/**
  Retrieves and removes the head of this queue, or NULL if this queue
  is empty.

  @param q a pointer to an instance of the priqueue_t data structure
  @return the head of this queue
  @return NULL if this queue is empty
 */
void *priqueue_poll(priqueue_t *q)
{
	if(priqueue_size(q) == 0) return NULL;

  void* ret = q->data[priqueue_get_true_index(q, 0)];
  q->start++;
  q->count--;
  return ret;
}


/**
  Returns the element at the specified position in this list, or NULL if
  the queue does not contain an index'th element.

  @param q a pointer to an instance of the priqueue_t data structure
  @param index position of retrieved element
  @return the index'th element in the queue
  @return NULL if the queue does not contain the index'th element
 */
void *priqueue_at(priqueue_t *q, int index)
{
	if(index >= q->count) return NULL;

  return q->data[priqueue_get_true_index(q, index)];
}


/**
  Removes all instances of ptr from the queue.

  This function should not use the comparer function, but check if the data contained in each element of the queue is equal (==) to ptr.

  @param q a pointer to an instance of the priqueue_t data structure
  @param ptr address of element to be removed
  @return the number of entries removed
 */
int priqueue_remove(priqueue_t *q, void *ptr)
{
  int removed = 0;

  for(int i = 0; i < q->count; i++){
    if(ptr == q->data[priqueue_get_true_index(q, i)]){
      priqueue_remove_at(q, i);
      i--;
      removed++;
    }
  }

  return removed;
}


/**
  Removes the specified index from the queue, moving later elements up
  a spot in the queue to fill the gap.

  @param q a pointer to an instance of the priqueue_t data structure
  @param index position of element to be removed
  @return the element removed from the queue
  @return NULL if the specified index does not exist
 */
void *priqueue_remove_at(priqueue_t *q, int index)
{
  if(index < 0 || index >= q->count) return NULL;

	for(int i = index; i < q->count-1; i++){
    priqueue_swap(q, priqueue_get_true_index(q, i), priqueue_get_true_index(q, i+1));
  }
  q->count--;
  // removed item will now be in the last spot after good data
  return q->data[priqueue_get_true_index(q, q->count)];
}


/**
  Returns the number of elements in the queue.

  @param q a pointer to an instance of the priqueue_t data structure
  @return the number of elements in the queue
 */
int priqueue_size(priqueue_t *q)
{
	return q->count;
}

/**
  Returns 1 if the priority queue is empty, else 0

  @param q a pointer to an instance of the priqueue_t data structure
  @return if the queue is empty
 */
int priqueue_is_empty(priqueue_t *q)
{
  return (q->count == 0);
}

/**
  Destroys and frees all the memory associated with q.

  @param q a pointer to an instance of the priqueue_t data structure
 */
void priqueue_destroy(priqueue_t *q)
{
  free(q->data);
}
