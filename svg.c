#define _POSIX_C_SOURCE 200809L

#include "parse.h"
#include "svg.h"
#include "layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define CHAR_WIDTH 7.0
#define CHAR_HEIGHT 11.0
#define BOX_HEIGHT 14.0


static const char* svg_header =
"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
"<svg"
"   xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\""
"   xmlns:svg=\"http://www.w3.org/2000/svg\""
"   xmlns=\"http://www.w3.org/2000/svg\""
"   width=\"1000\""
"   height=\"1000\""
"   id=\"svg2\""
"   version=\"1.1\">";

static const char* svg_rect =
"    <rect"
"       style=\"color:#000000;fill:none;stroke:#000000;stroke-width:1;\""
"       width=\"%f\""
"       height=\"%f\""
"       x=\"%f\""
"       y=\"%f\" />";

static const char* svg_text =
"    <text"
"       xml:space=\"preserve\""
"       style=\"font-size:12px;color:#000000;fill:#000000;stroke:none;font-family:Courier;\""
"       x=\"%f\""
"       y=\"%f\">"
"<tspan"
"         x=\"%f\""
"         y=\"%f\">%s</tspan></text>";


static const char* svg_line =
"    <line style=\"stroke:rgb(255,0,0);stroke-width:2\""
"       x1=\"%f\""
"       y1=\"%f\""
"       x2=\"%f\""
"       y2=\"%f\" />";

static const char* svg_footer = "</svg>";


static void add_child_node(struct label *parent, struct label *child) {
    struct label* last_child = parent->first_child;
    if (!last_child) {
        parent->first_child = child;
        return;
    }
    child->number = 1;
    while (last_child->next_sibling) {
        last_child = last_child->next_sibling;
        child->number ++;
    }
    child->prev_sibling = last_child;
    last_child->next_sibling = child;
}

static struct label* make_label(struct parse_tree_node* node,
                         struct label *parent) {
    struct label* label = malloc(sizeof(struct label));
    label->parent = parent;
    label->first_child = 0;
    label->next_sibling = 0;
    label->prev_sibling = 0;

    label->thread = 0;
    label->ancestor = label;
    label->number = 0;
    label->modifier = label->change = label->shift = 0;

    switch (node->op) {
    case LITERAL_OR_ID:
        label->text = strdup(node->text);
        break;
    case TYPECAST:
        label->text = malloc(strlen(node->text) + 3);
        sprintf(label->text, "(%s)", node->text);
        break;
    case FUNCTION_CALL:
        label->text = strdup("function call");
        break;
    default:
        label->text = strdup(token_names[node->op]);
        break;
    }
    label->width = (strlen(label->text) + 2) * CHAR_WIDTH;
    label->xcoord = 500;
    label->ycoord = 0;
    label->modifier = 0;
    return label;
}

static struct label* get_label_tree(struct parse_tree_node* node,
                             struct label *parent) {
    //to parse a node, we need to create a label for it,

    struct label* label = make_label(node, parent);

    if (node->first_child) {
        add_child_node(label, get_label_tree(node->first_child, label));

        struct parse_tree_node* cur = node->first_child->next_sibling;
        while (cur) {
            add_child_node(label, get_label_tree(cur, label));
            cur = cur->next_sibling;
        }
    }

    return label;
}

static void realloc_if_necessary(char** p, int* allocated, int new_size) {
    if (*allocated > new_size) {
        return;
    }
    while (*allocated < new_size) {
        *allocated *= 2;
    }
    *p = realloc(*p, *allocated);
}

struct queue {
    struct label* label;
    struct queue* next;
};

static struct label* next(struct label* node) {
    if (node->first_child) {
        return node->first_child;
    }
    if (node->next_sibling) {
        return node->next_sibling;
    }
    while (node->parent) {
        if (node->parent->next_sibling)
            return node->parent->next_sibling;
        node = node->parent;
    }
    return 0;
}

static double max(double a, double b) {
    return a > b ? a : b;
}

static char* tree_to_svg(struct label* tree) {
    int allocated = 1024;
    char* svg = malloc(allocated);

    int used = strlen(svg_header);
    realloc_if_necessary(&svg, &allocated, used);
    strcpy(svg, svg_header);

    int rect_len = strlen(svg_rect);
    int text_len = strlen(svg_text);
    int line_len = strlen(svg_line);
    int buf_size = max(max(rect_len, text_len), line_len) + 4 * 15 + 1;
    char svg_buf[buf_size];

    struct label* label = tree;
    do {

        double width = label->width;
        double x = label->xcoord - width / 2;
        double y = label->ycoord;

        if (label->parent) {
            double parent_width = label->parent->width;
            double parent_x = label->parent->xcoord - parent_width / 2;
            double parent_y = label->parent->ycoord;

            int needed = sprintf(svg_buf, svg_line, x + width / 2, 
                                 y, 
                                 parent_x + parent_width / 2, 
                                 parent_y + BOX_HEIGHT);

            realloc_if_necessary(&svg, &allocated, used + needed);
            strcpy(svg + used, svg_buf);
            used += needed;
        }


        int needed = sprintf(svg_buf, svg_rect, width, BOX_HEIGHT, x, y);
        realloc_if_necessary(&svg, &allocated, used + needed);
        strcpy(svg + used, svg_buf);
        used += needed;

        x += CHAR_WIDTH;
        y += CHAR_HEIGHT;
        needed = sprintf(svg_buf, svg_text, x, y, x, y, label->text);
        realloc_if_necessary(&svg, &allocated, used + needed);
        strcpy(svg + used, svg_buf);
        used += needed;

        label = next(label);
    } while (label);

    int needed = strlen(svg_footer);
    realloc_if_necessary(&svg, &allocated, used + needed);
    strcpy(svg + used, svg_footer);
    used += needed;

    return svg;
}

static void free_label_tree(struct label* label) {
    struct label* child = label->first_child;
    while (child) {
        struct label* next_child = child->next_sibling;
        free(child);
        child = next_child;
    }
    free(label);
}

char* parse_tree_to_svg(struct parse_tree_node* node) {
    struct label* label = get_label_tree(node, 0);

    struct walker_layout_rules rules;
    rules.sibling_separation = 10;
    rules.subtree_separation = 20;
    rules.level_separation = 30;
    walker_layout(label, &rules);

    char* svg = tree_to_svg(label);

    free_label_tree(label);

    return svg;
}
