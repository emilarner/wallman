#include "sarray.h"


sarray *sarray_init(void)
{
    sarray *s = (sarray*) malloc(sizeof(*s));

    s->capacity = 12;
    s->length = 0;
    s->strings = (char**) malloc(s->capacity * sizeof(char*));

    return s;
}

void sarray_add(sarray *s, char *string)
{
    /* If we are about to exceed our capacity, double our allocated memory. */ 
    if (s->length == s->capacity)
    {
        s->capacity *= 2;
        s->strings = (char**) realloc(s->strings, sizeof(char*) * s->capacity);
    }

    /* Create a copy of our string and assign it to the currently available index. */ 
    s->strings[s->length] = strdup(string);
    s->length++;
}

char *sarray_get(sarray *s, size_t index)
{
    /* If the index is greater than the length minus one, it must be invalid--return NULL. */
    if (index > (s->length - 1))
        return NULL;

    return s->strings[index];
}

void sarray_free(sarray *s)
{
    /* Free every malloc()'d string in the array. */ 
    for (size_t i = 0; i < s->length; i++)
        free(s->strings[i]);

    free(s->strings);
    free(s); 
}
