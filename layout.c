#include <stdlib.h>
#include <stdbool.h>
#include "layout.h"

/*
Algorithm from:
Improving Walker's Algorithm to run in Linear Time
Christoph Buchheim, Michael Ju"nger, Sebastian Leipert, 2002

(with a slight modification for variable-width nodes)
*/

struct layout_ctx {
    double x_top_adjustment;
    double y_top_adjustment;
    struct label **prev_node_at_level;
    struct walker_layout_rules *rules;
};

struct label* ancestor(struct label* left, struct label* node, struct label* default_ancestor) {

    if (left->ancestor->parent == node->parent) {
        return left->ancestor;
    }
    return default_ancestor;
}

void execute_shifts(struct label* node) {
    double shift = 0;
    double change = 0;
    //find the rightmost child
    struct label* child = node;
    while (child->next_sibling) {
        child = child->next_sibling;
    }
    //walk the children backwards
    while (child) {
        child->xcoord += shift;
        child->modifier += shift;
        change += child->change;
        shift += child->shift + change;
        child = child->prev_sibling;
    }
}

void move_subtree(struct label* left, struct label* right, double shift) {
    int subtrees = right->number - left->number;
    right->change -= shift / subtrees;
    right->shift += shift;
    left->change += shift / subtrees;
    right->xcoord += shift;
    right->modifier += shift;
}

static struct label* next_left(struct label *node) {
    if (node->first_child) {
        return node->first_child;
    }
    return node->thread;
}

static struct label* next_right(struct label *node) {
    if (node->first_child) {
        struct label* child = node->first_child;
        while(child->next_sibling) {
            child = child->next_sibling;
        }
        return child;
    }
    return node->thread;
}

static double spacing(struct layout_ctx* ctx, struct label *node, 
                    struct label *left, bool siblings) {
    double separation;
    if (siblings)
        separation = ctx->rules->sibling_separation;
    else
        separation = ctx->rules->subtree_separation;
    return separation + 0.5 * (node->width + left->width);
}

static struct label* apportion(struct layout_ctx* ctx, struct label* node, 
                               struct label* default_ancestor) {
    if (!node->prev_sibling) {
        return default_ancestor;
    }
    struct label *inner_right_node = node;
    struct label *outer_right_node = node;
    struct label *inner_left_node = node->prev_sibling;
    struct label *outer_left_node = inner_right_node->parent->first_child;
    double shift_inner_right = inner_right_node->modifier;
    double shift_outer_right = outer_right_node->modifier;
    double shift_inner_left = inner_left_node->modifier;
    double shift_outer_left = outer_left_node->modifier;

    while (next_right(inner_left_node) && 
           next_left(inner_right_node)) {

        inner_left_node = next_right(inner_left_node);
        inner_right_node = next_left(inner_right_node);
        outer_left_node = next_left(outer_left_node);
        outer_right_node = next_right(outer_right_node);
        outer_right_node->ancestor = node;
        double shift = spacing(ctx, inner_left_node,inner_right_node,false)
            + inner_left_node->xcoord + shift_inner_left
            - (inner_right_node->xcoord + shift_inner_right);

        if (shift > 0) {
            struct label *a = ancestor(inner_left_node,node, default_ancestor);
            move_subtree(a, node, shift);
            shift_inner_right += shift;
            shift_outer_right += shift;
        }
        shift_inner_left += inner_left_node->modifier;
        shift_inner_right += inner_right_node->modifier;
        shift_outer_left += outer_left_node->modifier;
        shift_outer_right += outer_right_node->modifier;
    }
    if (next_right(inner_left_node) && 
        next_right(outer_right_node) == 0) {

        outer_right_node->thread = next_right(inner_left_node);
        outer_right_node->modifier += shift_inner_left - shift_outer_right;
    } else {
        if (next_left(inner_right_node) && 
            next_left(outer_left_node) == 0) {

            outer_left_node->thread = next_left(inner_right_node);
            outer_left_node->modifier += shift_inner_right - shift_outer_left;
        }
        default_ancestor = node;
    }

    return default_ancestor;
}


static void second_walk(struct layout_ctx* ctx, struct label *node, int level, 
                        double modsum) {

    node->xcoord = ctx->x_top_adjustment + node->xcoord + modsum;
    node->ycoord = ctx->y_top_adjustment + level * ctx->rules->level_separation;

    if (node->first_child) {
        second_walk(ctx, node->first_child, level + 1, 
                    modsum + node->modifier);
    }
    if (node->next_sibling) {
        second_walk(ctx, node->next_sibling, level, modsum);
    }
}

static void first_walk(struct layout_ctx* ctx, struct label *node) {
    if (node->first_child) {
        struct label *default_ancestor = node->first_child;
        struct label *cur = node->first_child;
        struct label *last_child;
        while (cur) {
            first_walk(ctx, cur);
            default_ancestor = apportion(ctx, cur, default_ancestor);
            last_child = cur;
            cur = cur->next_sibling;
        }
        execute_shifts(node);
        double midpoint = (node->first_child->xcoord + last_child->xcoord) / 2;

        if (node->prev_sibling) {
            node->xcoord = node->prev_sibling->xcoord 
                + spacing(ctx, node->prev_sibling, node, true)
                + ctx->rules->sibling_separation;
            node->modifier = node->xcoord - midpoint;
        } else {
            node->xcoord = midpoint;
        }

    } else {
        if (node->prev_sibling) {
            node->xcoord = node->prev_sibling->xcoord 
                + spacing(ctx, node, node->prev_sibling, true);
        } else {
            node->xcoord = 0;
        }
    }

}

int get_max_depth(struct label *node) {
    int depth = 0;
    struct label* child = node->first_child;
    while (child) {
        int child_depth = get_max_depth(child);
        if (child_depth > depth) {
            depth = child_depth;
        }
        child = child->next_sibling;
    }
    return depth + 1;
}

/*
  Lay out a tree using Walker/Buchheim's layout algorithm.  The tree
  is rooted at node->xcoord, node->ycoord
 */
void walker_layout(struct label *node, struct walker_layout_rules* rules) {
    if (!node) {
        return;
    }

    struct layout_ctx ctx;

    ctx.prev_node_at_level = calloc(get_max_depth(node), sizeof(node));
    ctx.rules = rules;

    ctx.x_top_adjustment = node->xcoord;
    ctx.y_top_adjustment = node->ycoord;

    first_walk(&ctx, node);
    second_walk(&ctx, node, 0, -node->xcoord);

}
