// ----- Bstring: growable string declarations ----- //
#include <unistd.h>
#include <stdbool.h>

struct Bstring
{
    char *data;
    unsigned int length;
    unsigned int capacity;
};

#define BSTRING_INIT_CAPACITY 16

struct Bstring *bstring_init(unsigned int capacity, const char *s);
bool bstring_append(struct Bstring *self, const char *s);
void bstring_free(struct Bstring *self);
