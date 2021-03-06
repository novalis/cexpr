/*
  Extremely minimal CGI support -- only supports GET requests.
  But hey, at least it supports multiple variables with the same
  name.
 */
#ifndef CGI_H
#define CGI_H

struct cgi_var {
    char* var;
    char** values;
    int n_values;
    int values_allocated;
};

struct cgi {
    struct cgi_var* vars;
    int n_vars;
    int vars_allocated;
};

struct cgi_var* cgi_get_var(struct cgi* cgi, const char* var_name);
struct cgi* cgi_init();

#endif
