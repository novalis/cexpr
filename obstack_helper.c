#include "obstack_helper.h"

char* obstack_strdup(struct obstack* obstack, const char* str) {
    while(1) {
        char c = *str++;
        obstack_1grow (obstack, c);
        if (!c) {
            break;
        }
    }
    return obstack_finish(obstack);
}

char* obstack_strndup(struct obstack* obstack, const char* str,
                             size_t n) {
    char c;
    size_t i = 0;
    while((c = *str++) && i++ < n) {
        obstack_1grow (obstack, c);
    }
    obstack_1grow (obstack, 0);
    return obstack_finish(obstack);
}
