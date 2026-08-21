//
//  array.c
//  PlayGB
//
//  Created by Matteo D'Ignazio on 23/08/21.
//

#include "array.h"

PGB_Array* array_new(void)
{
    PGB_Array *array = pgb_malloc(sizeof(PGB_Array));
    array->length = 0;
    array->capacity = 0;
    array->items = NULL;

    return array;
}

void array_push(PGB_Array *array, void *item)
{
    if (array->length >= array->capacity)
    {
        unsigned int new_cap = array->capacity == 0 ? 8 : (array->capacity * 2);
        void **new_items = pgb_realloc(array->items, new_cap * sizeof(void*));
        if (new_items)
        {
            array->items = new_items;
            array->capacity = new_cap;
        }
    }
    array->items[array->length++] = item;
}

void array_clear(PGB_Array *array)
{
    array->length = 0;
    array->capacity = 0;
    pgb_free(array->items);
    array->items = NULL;
}

void array_free(PGB_Array *array)
{
    pgb_free(array->items);
    pgb_free(array);
}
