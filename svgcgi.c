#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include "cgi.h"
#include "svg.h"
#include "parse.h"

int main() {
    struct cgi* cgi = cgi_init();
    struct cgi_var* expr_var = cgi_get_var(cgi, "expr");
    struct cgi_var* typename_var = cgi_get_var(cgi, "typenames");
    if (!expr_var) {
        printf("Content-type: text/html\n\n");
        printf("Error: missing arg expr\n");
        return 0;
    }

    const char* expr = expr_var->values[0];

    char** typenames = 0;
    if (typename_var) {
        char* typename_str = strdup(typename_var->values[0]);
        int n_typenames = 1;
        for (char* c = typename_str; *c; ++c) {
            if (*c == ',') {
                ++n_typenames;
            }
        }
        typenames = malloc((n_typenames + 1) * sizeof(char*));
        int i = 0;
        if (strlen(typename_str)) {
            typenames[i++] = typename_str;
        }
        for (char* c = typename_str; *c; ++c) {
            if (*c == ',') {
                *c = 0;
                if (strlen(c)) {
                    typenames[i++] = c;
                }
            }
        }
        typenames[i] = 0;
    }

    struct parse_result* result = parse(expr, typenames);
    if (result->is_error) {
        printf("Content-type: text/html\n\n");
        printf("Error: parsing %s: %s\n", expr, result->error_message);
        return 0;
    }
    
    printf("Content-type: image/svg+xml\n\n");
    char* svg = parse_tree_to_svg(result->node);
    printf("%s", svg);

    return 0;
}
