#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

#ifndef OBSTACK_HELPER_H
#define OBSTACK_HELPER_H
#include <obstack.h>

char* obstack_strdup(struct obstack* obstack, const char* str);

char* obstack_strndup(struct obstack* obstack, const char* str,
                      size_t n);

#endif
