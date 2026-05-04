#include "arraylist.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INIT_CAPACITY 8
#define RESIZE_FACTOR 2

ArrayList* ArrayList_new(void (*free_el)(void *)) {
    ArrayList *new_lst = malloc(sizeof(ArrayList));
    if (new_lst == NULL) {
        fprintf(stderr, "ArrayList_new: fail to malloc\n");
        return NULL;
    }
     
    new_lst->contents = malloc(INIT_CAPACITY * sizeof(void *));
    if (new_lst->contents == NULL) {
        fprintf(stderr, "ArrayList_new: fail to malloc\n");
        return NULL;
    }
    new_lst->size = 0;
    new_lst->capacity = INIT_CAPACITY;
    new_lst->free_el = free_el;

    return new_lst;
}

uint32_t ArrayList_insert(ArrayList *lst, void *data) {
    // Check if params not null
    if (lst == NULL || data == NULL) {
        fprintf(stderr, "ArrayList_insert: lst or data was NULL\n");
        return 0;
    }
    // If array list at capacity, resize
    if (lst->size == lst->capacity) {
        int resized = ArrayList_resize(lst);
        if (!resized) {
            fprintf(stderr, "ArrayList_insert: fail to resize\n");
            return 0;
        }
    }
    
    lst->contents[lst->size] = data;
    lst->size++;

    return 1;
}

void* ArrayList_remove(ArrayList *lst, uint32_t index) {
    if (lst == NULL) {
        fprintf(stderr, "ArrayList_remove: lst was NULL\n");
        return NULL;
    }
    if (index >= lst->size) {
        fprintf(stderr, "ArrayList_remove: index too large\n");
        return NULL;
    }

    void* removed_el = lst->contents[index];

    // Shift contents of list to the right 
    // of index 1 to the left 
    memmove(
        &lst->contents[index],
        &lst->contents[index + 1],
        (lst->size - index - 1) * sizeof(void *)
    );
    lst->size--; // Decrement size
    
    return removed_el;
}

void* ArrayList_get(ArrayList *lst, uint32_t index) {
    if (lst == NULL) {
        fprintf(stderr, "ArrayList_get: lst was NULL");
        return NULL;
    }
    if (index >= lst->size) {
        fprintf(stderr, "ArrayList_get: index was too large\n");
        return NULL;
    }

    return lst->contents[index];
}

uint32_t ArrayList_size(ArrayList *lst) {
    if (lst == NULL) {
        fprintf(stderr, "ArrayList_size: lst was NULL\n");
        return 0;
    }

    return lst->size;
}

uint32_t ArrayList_resize(ArrayList *lst) {
    if (lst == NULL) {
        fprintf(stderr, "ArrayList_resize: lst was NULL\n");
        return 0;
    }

    uint32_t new_capacity = lst->capacity * RESIZE_FACTOR;
    if (new_capacity < lst->capacity) {
        fprintf(stderr, "ArrayList_resize: capacity overflow\n");
        return 0;
    }
    void** temp = realloc(lst->contents, new_capacity * sizeof(void *));

    if (temp == NULL) {
        fprintf(stderr, "ArrayList_resize: realloc fail\n");
        return 0;
    }

    lst->contents = temp;
    lst->capacity = new_capacity;

    return 1;
}

int ArrayList_exists(ArrayList *lst, void *targ, int (*comparator)(void *, void *)) {
    if (lst == NULL || targ == NULL) {
        fprintf(stderr, "ArrayList_exists: lst or targ was NULL\n");
        return 0;
    }

    for (uint32_t i = 0; i < lst->size; i++) {
        if (comparator(targ, lst->contents[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

uint32_t ArrayList_free(ArrayList *lst) {
    if (lst == NULL) {
        fprintf(stderr, "ArrayList_free: lst was NULL");
        return 0;
    }

    // Check if array list has a free function, if so remove using it
    if (lst->free_el != NULL) {
        for (uint32_t i = 0; i < lst->size; i++) {
            lst->free_el(lst->contents[i]);
        }
    }

    free(lst->contents);
    free(lst);

    return 1;
}

void* ArrayList_getel(ArrayList *lst, void *targ, int (*comparator)(void *, void *)) {
    if (lst == NULL || targ == NULL) {
        fprintf(stderr, "ArrayList_exists: lst or targ was NULL\n");
        return 0;
    }

    for (uint32_t i = 0; i < lst->size; i++) {
        if (comparator(targ, lst->contents[i]) == 0) {
            return ArrayList_get(lst, i);
        }
    }

    // If nothing found return NULL
    return NULL;
}