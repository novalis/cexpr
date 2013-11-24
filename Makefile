CC=gcc
CFLAGS=-c -Wall -Wextra -pedantic --std=c11
LDFLAGS=
EXPR_PARSE_SOURCES=main.c lex.c parse.c svg.c layout.c
EXPR_PARSE_OBJECTS=$(EXPR_PARSE_SOURCES:.c=.o)
EXPR_PARSE_EXECUTABLE=expr_parse

CGI_SOURCES=cgi.c lex.c parse.c svg.c layout.c
CGI_OBJECTS=$(CGI_SOURCES:.c=.o)
CGI_EXECUTABLE=expr.cgi

all: $(OBJECTS) $(EXPR_PARSE_EXECUTABLE) $(CGI_EXECUTABLE)

$(EXPR_PARSE_EXECUTABLE): $(EXPR_PARSE_OBJECTS)
	$(CC) $(LDFLAGS) $(EXPR_PARSE_OBJECTS) -o $@

$(CGI_EXECUTABLE): $(CGI_OBJECTS)
	$(CC) $(LDFLAGS) $(CGI_OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@
