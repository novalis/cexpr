CC=gcc
CFLAGS=-c -Wall -Wextra -pedantic --std=c11 -g
LDFLAGS=

SOURCES=lex.c parse.c layout.c obstack_helper.c

EXPR_PARSE_SOURCES=main.c $(SOURCES)
EXPR_PARSE_OBJECTS=$(EXPR_PARSE_SOURCES:.c=.o)
EXPR_PARSE_EXECUTABLE=expr_parse

CGI_SOURCES=cgi.c svgcgi.c svg.c $(SOURCES)
CGI_OBJECTS=$(CGI_SOURCES:.c=.o)
CGI_EXECUTABLE=expr.cgi

SVG_SOURCES=svgmain.c svg.c $(SOURCES)
SVG_OBJECTS=$(SVG_SOURCES:.c=.o)
SVG_EXECUTABLE=expr_svg

PARSE_TEST_SOURCES=parsetest.c $(SOURCES)
LEX_TEST_SOURCES=lextest.c $(SOURCES)
CGI_TEST_SOURCES=cgitest.c cgi.c $(SOURCES)
PARSE_TEST_OBJECTS=$(PARSE_TEST_SOURCES:.c=.o)
LEX_TEST_OBJECTS=$(LEX_TEST_SOURCES:.c=.o)
CGI_TEST_OBJECTS=$(CGI_TEST_SOURCES:.c=.o)

MAKEDEPEND=makedepend

%.P : %.c
	$(MAKEDEPEND)
	@sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' < $*.d > $@; \
	rm -f $*.d; [ -s $@ ] || rm -f $@

include $(SRCS:.c=.P)

all: $(OBJECTS) $(EXPR_PARSE_EXECUTABLE) $(CGI_EXECUTABLE)

lextest: $(LEX_TEST_OBJECTS)
	$(CC) $(LDFLAGS) $(LEX_TEST_OBJECTS) -o $@

parsetest: $(PARSE_TEST_OBJECTS)
	$(CC) $(LDFLAGS) $(PARSE_TEST_OBJECTS) -o $@

cgitest: $(CGI_TEST_OBJECTS)
	$(CC) $(LDFLAGS) $(CGI_TEST_OBJECTS) -o $@

test: lextest parsetest cgitest
	./lextest
	./parsetest
	./cgitest

$(EXPR_PARSE_EXECUTABLE): $(EXPR_PARSE_OBJECTS)
	$(CC) $(LDFLAGS) $(EXPR_PARSE_OBJECTS) -o $@

$(CGI_EXECUTABLE): $(CGI_OBJECTS)
	$(CC) $(LDFLAGS) $(CGI_OBJECTS) -o $@

$(SVG_EXECUTABLE): $(SVG_OBJECTS)
	$(CC) $(LDFLAGS) $(SVG_OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f *.o *.d lextest parsetest expr_parse expr.cgi