#ifndef SARRAY_H
#define SARRAY_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* The main structure for the simple dynamic string array. */ 
struct sarray
{
    char **strings; 
    size_t capacity;
    size_t length;
};

typedef struct sarray sarray;

/* Initialize the string array, returning a pointer to a malloc()'d structure. */
sarray *sarray_init(void);

/* Add a string to the array. This will create a malloc()'d copy. */
void sarray_add(sarray *s, char *string);

/* Get a string from the array by index. */
char *sarray_get(sarray *s, size_t index);

/* Free the array and all of the malloc()'d strings it holds. Avoid memory leaks! */
void sarray_free(sarray *s);


#endif