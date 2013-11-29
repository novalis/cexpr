#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgi.h"

struct var_spec {
    char* var;
    int n_values;
    char* values[5];
};

struct test_spec {
    char* input;
    int n_vars;
    struct var_spec vars[5];
};

struct test_spec specs[] = {
    {"a=b", 1, 
     {{"a", 1, 
       {"b"}
         }}
    },
    {"a=b&a=b", 1, 
     {{"a", 2, 
       {"b", "b"}
         }}
    },
    {"a=b&a=c", 1, 
     {{"a", 2, 
       {"b", "c"}
         }}
    },
    {"a=b&c=d&a=e", 1, 
     {{"a", 2, 
       {"b", "e"}
         },
      {"c", 1, 
       {"d"}
      }
     }
    },
    {"a=b&c=%3dc", 1, 
     {{"a", 1, 
       {"b"}
         },
      {"c", 1, 
       {"=c"}
      }
     }
    },
    {0}
};

int main() {
    int bad = 0;

    for (struct test_spec* spec = specs; spec->input; ++spec) {
        setenv("QUERY_STRING", spec->input, 1);
        struct cgi* cgi = cgi_init();
        for (int var = 0; var < spec->n_vars; ++var) {
            struct var_spec expected_var = spec->vars[var];
            struct cgi_var* cgi_var = cgi_get_var(cgi, expected_var.var);
            if (!cgi_var) {
                printf("Query string %s: missing var %s\n", 
                       spec->input, expected_var.var);
                bad ++;
            } else if (strcmp(cgi_var->var, expected_var.var)) {
                printf("Query string %s: misnamed var %s (got %s)\n", 
                       spec->input, expected_var.var, cgi_var->var);
                bad++;
            } else if (cgi_var->n_values != expected_var.n_values) {
                printf("Query string %s: wrong number of values var %s (%d for %d)\n", 
                       spec->input, expected_var.var, 
                       cgi_var->n_values, expected_var.n_values);
                bad++;
            } else {
                for (int val = 0; val < expected_var.n_values; ++val) {
                    if (strcmp(cgi_var->values[val], expected_var.values[val])) {
                        printf("Query string %s: wrong values var %s (%s for %s)\n", 
                               spec->input, expected_var.var,
                               cgi_var->values[val], expected_var.values[val]);
                        bad++;
                    } 
                }
            }
        }
    }

    if (bad) {
        printf ("Found %d errors\n", bad);
    }
    return bad;
}
