#ifndef ARRAYLIST_H
#define ARRAYLIST_H

/*
*   Array list. Based on the code from:
*   https://github.com/marekweb/datastructs-c
*
*   But tweaked for this project
*
*   This is not robust!!!! but good enough for this project
*/

#include <stdint.h> // uint32

typedef struct {
    uint32_t size;
    uint32_t capacity;
    void** contents;
    void (*free_el)(void *); // Function to free element iself
} ArrayList;

/*
*   Create array list. malloc'd
*/
ArrayList* ArrayList_new(void (*free_el)(void *));

/*
*   Insert cannot be NULL   
*
*   Return 0 on fail
*/
uint32_t ArrayList_insert(ArrayList *lst, void *data);

/*
*   Return pointer to removed element, I guess. NULL if fail
*/
void* ArrayList_remove(ArrayList *lst, uint32_t index);

/*
*   Returns a pointer to the element at the specified index. NULL if fail
*/
void* ArrayList_get(ArrayList *lst, uint32_t index);

/*
*   Returns the size of the array list (how many elements in it, not capacity)
*/
uint32_t ArrayList_size(ArrayList *lst);

/*
*   Resize the array list. Mainly used by the ArrayList itself.
*   0 on fail and 1 on success.
*/
uint32_t ArrayList_resize(ArrayList *lst);

/*
*   1 if element in lst, 0 otherwise.
*
*   -1 on invalid input.
*/
int ArrayList_exists(ArrayList *lst, void *element, int (*comparator)(void *, void *));

/*
*   Free the contents in an array list.
*   itself. 1 on success 0 otherwise
*/
uint32_t ArrayList_free(ArrayList *lst);

/*
*   Return a pointer to the element we want. Will returns the
*   first matching element it finds.
*
*   NULL if it does not exist.
*/
void* ArrayList_getel(ArrayList *lst, void *targ, int (*comparator)(void *, void *));


#endif