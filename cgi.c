#define _POSIX_C_SOURCE 200809L

#include "cgi.h"
#include "parse.h"
#include "svg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

struct cgi_var* get_cgi_var(struct cgi* cgi, const char* var_name) {
    for (int i = 0; i < cgi->n_vars; ++i) {
        struct cgi_var* var = cgi->vars + i;
        if (!strcmp(var->key, var_name)) {
            return var;
        }
    }
    return 0;
}

static char* unescape(const char* str) {
    char* dup = strdup(str);
    const char* read = dup;
    char* write = dup;
    char c;
    while ((c = *read++)) {
        if (c == '%') {
            unsigned int x;
            int n = sscanf(read, "%2x", &x);
            if (n == 1) {
                read += 2;
            } else {
                /*parsing got screwed up, so just cut the string off
                  here */
                return dup;
            }
            *write++ = x;
        } else if (c == '+') {
            *write++ = ' ';
        } else {
            *write++ = c;
        }
    }
    *write = 0;
    return dup;
}

static struct cgi_var new_var(const char* key, const char* value) {
    struct cgi_var var;
    var.key = unescape(key);
    var.values = malloc(sizeof(char*));
    var.values[0] = unescape(value);
    var.n_values = 1;
    var.values_allocated = 1;
    return var;
}

static void add_value(struct cgi_var* var, const char* value) {
    if (var->n_values >= var->values_allocated) {
        var->values_allocated *= 2;
        var->values = realloc(var->values, var->values_allocated * sizeof(char*));
    }
    var->values[var->n_values++] = unescape(value);
}

static void add_var(struct cgi* cgi, struct cgi_var* var) {
    if (cgi->n_vars >= cgi->vars_allocated) {
        cgi->vars_allocated = (cgi->vars_allocated + 1) * 2;
        cgi->vars = realloc(cgi->vars, cgi->vars_allocated * sizeof(struct cgi_var));
    }
    cgi->vars[cgi->n_vars++] = *var;
}

static struct cgi* parse_query_string(const char* query_string) {
    struct cgi* cgi = malloc(sizeof(struct cgi));
    cgi->vars = 0;
    cgi->n_vars = 0;
    cgi->vars_allocated = 0;

    char* query_string_dup = strdup(query_string);
    char* strtok_arg = query_string_dup;
    char* save_ptr;
    char* next_tok;
    while ((next_tok = strtok_r(strtok_arg, "&", &save_ptr))) {
        strtok_arg = 0;
        char* var_name = next_tok;
        char* value = strchr(next_tok, '=');
        *value = 0;
        value ++;
        struct cgi_var* var = get_cgi_var(cgi, var_name);
        if (var) {
            add_value(var, value);
        } else {
            struct cgi_var created = new_var(var_name, value);
            var = &created;
            add_var(cgi, var);
        }
    }
    free(query_string_dup);
    return cgi;
}

struct cgi* init() {
    const char* query_string = "QUERY_STRING";
    int query_string_len = strlen(query_string);
    int i = 0;
    while(environ[i]) {
        if (strncmp(environ[i], query_string, query_string_len) == 0) {
            struct cgi* cgi = parse_query_string(strchr(environ[i], '=') + 1);
            return cgi;
        }
        ++i;
    }

    return parse_query_string("");
}

int main() {
    struct cgi* cgi = init();
    struct cgi_var* expr_var = get_cgi_var(cgi, "expr");
    struct cgi_var* typename_var = get_cgi_var(cgi, "typenames");
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
