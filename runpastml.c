#include "asrml.h"
#include "marginal_lik.h"
#include "lik.h"
#include "marginal_approxi.h"
#include "golden.h"
#include "fletcher.h"
#include "fletcherJC.h"
#include "make_tree.h"
#include <time.h>
#include <unistd.h>
#include <errno.h>

Tree *s_tree;
Node *root;
int have_miss = -1;
double opt_param[MAXCHAR + 2];

unsigned int tell_size_of_one_tree(char *filename) {
    /* the only purpose of this is to know about the size of a treefile (NH format) in order to save memspace in allocating the string later on */
    /* wew open and close this file independently of any other fopen */
    unsigned int mysize = 0;
    char u;
    FILE *myfile = fopen(filename, "r");
    if (myfile) {
        while ((u = fgetc(myfile)) != ';') { /* termination character of the tree */
            if (u == EOF) break; /* shouldn't happen anyway */
            if (isspace(u)) continue; else mysize++;
        }
        fclose(myfile);
    } /* end if(myfile) */
    return (mysize + 1);
}

int copy_nh_stream_into_str(FILE *nh_stream, char *big_string) {
    int index_in_string = 0;
    char u;
    /* rewind(nh_stream); DO NOT go to the beginning of the stream if we want to make this flexible enough to read several trees per file */
    while ((u = fgetc(nh_stream)) != ';') { /* termination character of the tree */
        if (u == EOF) {
            big_string[index_in_string] = '\0';
            return EXIT_FAILURE;
        } /* error code telling that no tree has been read properly */
        if (index_in_string == MAX_TREELENGTH - 1) {
            fprintf(stderr, "Fatal error: tree file seems too big, are you sure it is a newick tree file?\n");
            return EXIT_FAILURE;
        }
        if (isspace(u)) continue;
        big_string[index_in_string++] = u;
    }
    big_string[index_in_string++] = ';';
    big_string[index_in_string] = '\0';
    return EXIT_SUCCESS; /* leaves the stream right after the terminal ';' */
} /*end copy_nh_stream_into_str */

void free_edge(Edge *edge) {
    int i;
    if (edge == NULL) return;
    for (i = 0; i < 2; i++) if (edge->subtype_counts[i]) free(edge->subtype_counts[i]);

    free(edge);
}

void free_node(Node *node, int count, int num_anno) {
    int j;

    if (node == NULL) return;
    if (node->name && count != 0) free(node->name);
    if (node->comment) free(node->comment);
    free(node->neigh);
    free(node->br);
    for (j = 0; j < num_anno; j++) free(node->pij[j]);
    free(node->pij);
    if (count == 0) {
        for (j = 0; j < num_anno; j++) free(node->rootpij[j]);
        free(node->rootpij);
    }
    free(node->prob);
    free(node->best_i);
    free(node->condlike);
    free(node->condlike_mar);
    free(node->sortedlike);
    free(node->sortedstates);
    free(node->marginal);
    free(node->mar_state);
    free(node->tmp_best);
    free(node->mar_prob);
    free(node->up_like);
    free(node->sum_down);
    free(node->calc_flag);

    free(node);
}

void free_tree(Tree *tree, int num_anno) {
    int i;
    printf("\nStart freeing tree\n\n");
    if (tree == NULL) return;
    printf("\nThere is a tree to free with %d nodes\n\n", tree->nb_nodes);
    printf("\nand %d annots\n\n", num_anno);
    for (i = 0; i < tree->nb_nodes; i++) free_node(tree->a_nodes[i], i, num_anno);
    for (i = 0; i < tree->nb_edges; i++) free_edge(tree->a_edges[i]);
    for (i = 0; i < tree->nb_taxa; i++) free(tree->taxa_names[i]);

    free(tree->taxa_names);
    free(tree->a_nodes);
    free(tree->a_edges);
    free(tree);

}

int runpastml(char *annotation_name, char* tree_name, char *out_annotation_name, char *out_tree_name,
              char *model, double *frequency, char* scaling, char* keep_ID) {
    int i, line = 0, check, sum_freq = 0, count_miss;
    int *states, *count, *factors;
    double sum = 0., mu, lnl = 0., scale, maxlnl = 0., nano, sec;
    double *parameter;
    char **annotations, **character, **tips, *c_tree;
    int num_anno = 0, num_tips = 0;
    FILE *treefile, *annotationfile;
    struct timespec samp_ini, samp_fin;
    char anno_line[MAXLNAME];
    int *iteration, ite;
    double *optlnl, scaleup;

    if ((strcmp(model, "JC") != 0) && (strcmp(model, "F81") != 0)) {
        sprintf(stderr, "Model must be either JC or F81, not %s", model);
        return EINVAL;
    }

    opterr = 0;
    //printf("\n*** Information of primary annotations ***\n\n");
    parameter = calloc(MAXCHAR + 2, sizeof(double));
    states = calloc(MAXNSP, sizeof(int));
    count = calloc(MAXCHAR, sizeof(int));
    factors = calloc(MAXNSP, sizeof(int));
    annotations = calloc(MAXNSP, sizeof(char *));
    tips = calloc(MAXNSP, sizeof(char *));
    for (i = 0; i < MAXNSP; i++) {
        annotations[i] = calloc(MAXLNAME, sizeof(char));
        tips[i] = calloc(MAXLNAME, sizeof(char));
    }
    character = calloc(MAXCHAR, sizeof(char *));
    for (i = 0; i < MAXCHAR; i++) {
        character[i] = calloc(MAXLNAME, sizeof(char));
    }
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &samp_ini);
    srand((unsigned) time(NULL));
    /*Read annotation from file*/
    annotationfile = fopen(annotation_name, "r");
    if (!annotationfile) {
        fprintf(stderr, "Annotation file %s is not found or is impossible to access.", annotation_name);
        fprintf(stderr, "Value of errno: %d\n", errno);
        fprintf(stderr, "Error opening the file: %s\n", strerror(errno));
        return ENOENT;
    }
    num_anno = -1;
    count_miss = 0;
    line = 0;
    states[0] = 0;
    while (fgets(anno_line, MAXLNAME, annotationfile)) {
        sscanf(anno_line, "%[^\n,],%[^\n\r]", tips[line], annotations[line]);
        if (strcmp(annotations[line], "") == 0) sprintf(annotations[line], "?");
        if (strcmp(annotations[line], "?") == 0) {
            count_miss++;
            states[line] = -1;
        } else {
            check = 0;
            for (i = 0; i < line; i++) {
                if (strcmp(annotations[i], "?") == 0) {
                } else {
                    if (strcmp(annotations[line], annotations[i]) == 0) {
                        states[line] = states[i];
                        strcpy(character[states[line]], annotations[line]);
                        check = 1;
                        break;
                    }
                }
            }
            if (check == 0) {
                num_anno++;
                states[line] = num_anno;
                strcpy(character[num_anno], annotations[line]);
            }
        }
        //printf("data %d, %s, %s, %d\n",line+1,tips[line],annotations[line],states[line]);
        line++;
    }
    fclose(annotationfile);
    //printf("number of character is %d\n",num_anno+1);
    num_anno++;

    for (i = 0; i < line; i++) {
        if (states[i] == -1) {
            states[i] = num_anno;
            sprintf(character[num_anno], "?");
        }
        count[states[i]]++;
    }
    have_miss = num_anno;

    for (i = 0; i < num_anno; i++) {
        sum_freq = sum_freq + count[i];
    }
    sum_freq = sum_freq + count_miss;
    printf("\n*** Frequency of each character in the MODEL %s ***\n\n", model);
    for (i = 0; i < num_anno; i++) {
        if (strcmp(model, "JC") == 0) {
            frequency[i] = (double) 1 / num_anno;
        } else if (strcmp(model, "F81") == 0) {
            frequency[i] = (double) count[i] / sum_freq;
        }
        printf("%s = %lf\n", character[i], frequency[i]);
        if (strcmp(character[i], character[i + 1]) == 0) {
            fprintf(stderr,
                    "PASTML cannot recognize the characters in the input annotation file.\n"
                            "Check your file format (must be csv), character encoding (must be UTF-8), "
                            "and newline encoding (must be Unix/Linux)\n");
            return EINVAL;
        }
    }
    printf("Frequency of Missing data %s = %lf\n", character[num_anno], (double) count_miss / (double) sum_freq);
    for (i = 0; i < num_anno; i++) {
        parameter[i] = frequency[i];
    }

    /*Read tree from file*/

    treefile = fopen(tree_name, "r");
    if (treefile == NULL) {
        fprintf(stderr, "Tree file %s is not found or is impossible to access.\n", tree_name);
        fprintf(stderr, "Value of errno: %d\n", errno);
        fprintf(stderr, "Error opening the file: %s\n", strerror(errno));
        return ENOENT;
    }

    unsigned int treefilesize = 3 * tell_size_of_one_tree(tree_name);
    if (treefilesize > MAX_TREELENGTH) {
        fprintf(stderr, "Tree filesize for %s is bigger than %d bytes: are you sure it's a valid newick tree?\n",
                tree_name, MAX_TREELENGTH / 3);
        return EFBIG;
    }

    void *retval;
    if( (retval=calloc(treefilesize + 1, sizeof(char))) == NULL ) {
        printf("Not enough memory\n");
        return ENOMEM;
    }
    c_tree = (char *) retval;

    if (EXIT_SUCCESS != copy_nh_stream_into_str(treefile, c_tree)) {
        fprintf(stderr, "A problem occurred while parsing the reference tree.\n");
        return EINVAL;
    }
    fclose(treefile);

    /*Make Tree structure*/
    s_tree = complete_parse_nh(c_tree, num_anno, keep_ID); /* sets taxname_lookup_table en passant */
    if (NULL == s_tree) {
        fprintf(stderr, "A problem occurred while parsing the reference tree.\n");
        return EINVAL;
    }
    root = s_tree->a_nodes[0];
    num_tips = s_tree->nb_taxa;

    //Compute initial likelihood
    for (i = 0; i < num_anno; i++) {
        sum += frequency[i] * frequency[i];
    }
    mu = 1 / (1 - sum);
    //parameter[num_anno]=s_tree->min_bl;
    parameter[num_anno] = 1.0;
    parameter[num_anno + 1] = MIN_BRLEN;
    calc_lik_bfgs(root, tips, states, num_tips, num_anno, mu, model, parameter, &lnl);
    printf("\n*** Initial likelihood of the tree ***\n\n %lf\n\n", lnl);
    scale = 1.0;
    ite = 0;
    iteration = &ite;
    optlnl = &maxlnl;
    scaleup = 2.0 / s_tree->avgbl;
    if (strcmp(scaling, "T") == 0) {
        golden(root, tips, states, num_tips, num_anno, mu, model, parameter, scaleup);
        printf("Scaling factor is roughly optimized by GSS\n");
        //dfpmin(root, tips, states, num_tips, num_anno, mu, model, parameter, num_anno+2, iteration, optlnl);
        if (strcmp(model, "F81") == 0) {
            frprmn(root, tips, states, num_tips, num_anno, mu, model, parameter, num_anno + 2, 1.0e-3, iteration,
                   optlnl, character);
        } else if (strcmp(model, "JC") == 0) {
            parameter[0] = parameter[num_anno];
            parameter[1] = parameter[num_anno + 1];
            frprmnJC(root, tips, states, num_tips, num_anno, mu, model, parameter, 2, 1.0e-3, iteration, optlnl,
                     character, frequency);
            parameter[num_anno] = parameter[0];
            parameter[num_anno + 1] = parameter[1];
            for (i = 0; i < num_anno; i++) {
                parameter[i] = frequency[i];
            }

        }
    }
    //maxlnl = -1.0 * maxlnl;
    printf("\n*** Optimized frequencies ***\n\n");
    for (i = 0; i < num_anno; i++) {
        //parameter[i]=parameter[i];
        printf("%s = %.5f\n", character[i], parameter[i]);
        //printf("%s = %.12f\n", character[i], opt_param[i]);
    }
    //parameter[num_anno]=parameter[num_anno];
    //parameter[num_anno+1]=parameter[num_anno+1];
    printf("\n*** Tree scaling factor ***\n\n %.5f \n\n*** Epsilon for zero branch length ***\n\n %.5e",
           parameter[num_anno], parameter[num_anno + 1]);
    calc_lik_bfgs(root, tips, states, num_tips, num_anno, mu, model, parameter, optlnl);
    printf("\n\n*** Optimised likelihood ***\n\n %lf\n", (*optlnl));

    //Marginal likelihood calculation
    printf("\n*** Calculating Marginal Likelihoods...\n");
    down_like_marginal(root, num_tips, num_anno, mu, scale, parameter);

    printf("\n*** Computing Marginal Approximation...\n\n");
    int res_code = make_samples(tips, states, num_tips, num_anno, character, parameter, out_annotation_name, out_tree_name);
    if (EXIT_SUCCESS != res_code) {
        return res_code;
    }

    //free all
    free(annotations);
    free(tips);
    free(character);
    //free(frequency);
    free(states);
    //free(count);
    free(factors);
    free_tree(s_tree, num_anno);

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &samp_fin);
    sec = (double) (samp_fin.tv_sec - samp_ini.tv_sec);
    nano = (double) (samp_fin.tv_nsec - samp_ini.tv_nsec) / 1000.0 / 1000.0 / 1000.0;

    printf("\nTotal execution time = %3.10lf seconds\n\n", (sec + nano));
    return EXIT_SUCCESS;
}