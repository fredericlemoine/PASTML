#ifndef PASTML_H
#define PASTML_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>


#define MAXLNAME 255
#define MAXNSP 50000
#define MAXPOLY 10000
#define MAXCHAR 50
#define MAX_TREELENGTH    10000000 /* more or less 10MB for a tree file in NH format */
#define MAX_NAMELENGTH        255    /* max length of a taxon name */
#define TRUE 1
#define FALSE 0
#define POW -500
#define LIM_P pow(2, POW)
#define LOG2 0.69314718055994528623
#define MIN(a, b) ((a)<(b)?(a):(b))

typedef struct __Node {
    char *name;
    int id;            /* unique id attributed to the node */
    int nneigh;    /* number of neighbours */
    struct __Node **neigh;    /* neighbour nodes */

    double **pij;           /* probability of substitution from i to j */
    double *bottom_up_likelihood;       /* conditional likelihoods at the node*/
    double *condlike_mar;
    double *marginal;
    int *best_states;
    double *top_down_likelihood;
    int fixed_state; /* a state given to the node in the annotation file, that is not to be optimised */
    double brlen;
} Node;


typedef struct __Tree {
    Node **a_nodes;            /* array of node pointers */
    Node *node0;            /* the root or pseudo-root node */
    int nb_nodes;
    int nb_edges;
    int nb_taxa;
    int next_avail_node_id;
    double avg_branch_len;
    double min_bl;
  double tip_avg_branch_len;
} Tree;

#endif // PASTML_H
