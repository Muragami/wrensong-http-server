
#include "bstring.h"
#include <assert.h>
#include <string.h>

/**
 * initializes a new Bstring. returns `NULL` if memory allocation fails.
 *
 * @param capacity the initial capacity of the vector. if 0, it will be set to a default value.
 * @param s inital value. it must be null terminated. pass `NULL` for no initial value.
 */
struct Bstring *bstring_init(unsigned int capacity, const char *s)
{
    struct Bstring *bstr = malloc(sizeof(struct Bstring));
    if (bstr == NULL)
    {
        return NULL;
    }

    if (capacity == 0)
    {
        capacity = BSTRING_INIT_CAPACITY;
    }

    const unsigned int slen = (s == NULL) ? 0 : (unsigned int)strlen(s);
    if (slen >= capacity)
    {
        capacity = slen + 1; // +1 for null terminator
    }

    bstr->data = malloc(sizeof(char) * capacity);
    if (bstr->data == NULL)
    {
        free(bstr);
        return NULL;
    }

    // copy if not NULL
    if (s != NULL)
    {
        memcpy(bstr->data, s, slen);
    }

    bstr->capacity = capacity;
    bstr->length = slen;
    bstr->data[slen] = '\0';

    return bstr;
}

/**
 * appends the given C string at the end of given bstring
 *
 * @param self the Bstring to append the C string to
 * @param the NULL-terminated C string to be appended
 */
bool bstring_append(struct Bstring *self, const char *s)
{
    assert(self != NULL);
    assert(s != NULL);

    const size_t slen = strlen(s);
    const size_t new_len = self->length + slen;
    if (new_len >= self->capacity)
    {
        // try doubling the current capacity
        size_t new_cap = self->capacity * 2;

        // check if it's enough
        if (new_len >= new_cap)
        {
            new_cap = new_len + 1; // +1 for null terminator
        }

        char *new_data = realloc(self->data, new_cap);
        if (new_data == NULL)
        {
            return false;
        }

        self->data = new_data;
        self->capacity = new_cap;
    }

    memcpy(&self->data[self->length], s, slen); // append
    self->length = new_len;                     // set new length
    self->data[new_len] = '\0';                 // null terminate

    return true;
}

void bstring_free(struct Bstring *self)
{
    assert(self != NULL);

    free(self->data);
    free(self);
}