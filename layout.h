#ifndef LAYOUT_H
#define LAYOUT_H

struct walker_layout_rules {
    double sibling_separation;
    double subtree_separation;
    double level_separation;
    double char_width;
};

/*
  a horizontal list of labels; each item knows about its children,
  parent, siblings, and some supplemental data from the Buchheim
  algorithm.
 */
struct label {
    struct label* prev_sibling;
    struct label* next_sibling;
    struct label* first_child;

    /* the next node along the contour of the subtree */
    struct label* thread;
    struct label* ancestor;

    /* which number child of the parent it is */
    int number;
    double change;
    double shift;

    struct label* parent;
    char* text;
    int len;

    double xcoord;
    double ycoord;
    double modifier;

};

void walker_layout(struct label *node, struct walker_layout_rules* rules);

#endif
