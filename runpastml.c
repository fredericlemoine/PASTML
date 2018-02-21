#include "pastml.h"
#include "marginal_lik.h"
#include "lik.h"
#include "marginal_approxi.h"
#include "make_tree.h"
#include "param_minimization.h"
#include <time.h>
#include <errno.h>

Tree *s_tree;
Node *root;

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

void free_node(Node *node, int count, int num_anno) {
    int j;

    if (node == NULL) return;
    if (node->name && count != 0) free(node->name);
    free(node->neigh);
    free(node->br);
    free(node->condlike);
    free(node->condlike_mar);
    free(node->marginal);
    free(node->tmp_best);
    free(node->mar_prob);
    free(node->up_like);
    free(node->sum_down);
    free(node->local_flag);
    for (j = 0; j < num_anno; j++) free(node->pij[j]);
    free(node->pij);
    for (j = 0; j < num_anno; j++) free(node->rootpij[j]);
    free(node->rootpij);
    free(node);
}

void free_tree(Tree *tree, int num_anno) {
    int i;
    if (tree == NULL) return;
    for (i = 0; i < tree->nb_nodes; i++) free_node(tree->a_nodes[i], i, num_anno);
    for (i = 0; i < tree->nb_edges; i++) free(tree->a_edges[i]);
    for (i = 0; i < tree->nb_taxa; i++) free(tree->taxa_names[i]);
    free(tree->taxa_names);
    free(tree->a_nodes);
    free(tree->a_edges);
    free(tree);

}

int runpastml(char *annotation_name, char *tree_name, char *out_annotation_name, char *out_tree_name, char *model,
              double collapse_BRLEN) {
    int i, line = 0, found_new_annotation, sum_freq = 0;
    int *states;
    int *count_array;
    double sum = 0., mu, lnl, scale, nano, sec;
    double *parameter;
    char **annotations, **character, **tips, *c_tree;
    int num_anno = -1, num_tips = 0;
    FILE *treefile, *annotationfile;
    struct timespec samp_ini, samp_fin;
    char anno_line[MAXLNAME];
    int max_characters = MAXCHAR;
    double *frequency = NULL;

    if ((strcmp(model, "JC") != 0) && (strcmp(model, "F81") != 0)) {
        sprintf(stderr, "Model must be either JC or F81, not %s", model);
        return EINVAL;
    }

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &samp_ini);
    srand((unsigned) time(NULL));

    /* Allocate memory */
    states = calloc(MAXNSP, sizeof(int));
    annotations = calloc(MAXNSP, sizeof(char *));
    tips = calloc(MAXNSP, sizeof(char *));
    for (i = 0; i < MAXNSP; i++) {
        annotations[i] = calloc(MAXLNAME, sizeof(char));
        tips[i] = calloc(MAXLNAME, sizeof(char));
    }
    states[0] = 0;
    character = calloc(max_characters, sizeof(char *));
    for (i = 0; i < max_characters; i++) {
        character[i] = calloc(MAXLNAME, sizeof(char));
    }

    /*Read annotation from file*/
    annotationfile = fopen(annotation_name, "r");
    if (!annotationfile) {
        fprintf(stderr, "Annotation file %s is not found or is impossible to access.", annotation_name);
        fprintf(stderr, "Value of errno: %d\n", errno);
        fprintf(stderr, "Error opening the file: %s\n", strerror(errno));
        return ENOENT;
    }

    while (fgets(anno_line, MAXLNAME, annotationfile)) {
        sscanf(anno_line, "%[^\n,],%[^\n\r]", tips[line], annotations[line]);
        char *annotation_value = annotations[line];
        if (strcmp(annotation_value, "") == 0) sprintf(annotation_value, "?");
        if (strcmp(annotation_value, "?") == 0) {
            states[line] = -1;
        } else {
            found_new_annotation = TRUE;
            for (i = 0; i < line; i++) {
                if (strcmp(annotations[i], "?") != 0 && strcmp(annotation_value, annotations[i]) == 0) {
                    states[line] = states[i];
                    strcpy(character[states[line]], annotation_value);
                    found_new_annotation = FALSE;
                    break;
                }
            }
            if (found_new_annotation == TRUE) {
                num_anno++;
                states[line] = num_anno;
                if (num_anno >= max_characters) {
                    /* Annotations do not fit in the character array (of size max_characters) anymore,
                     * so we gonna double reallocate the memory for the array (of double size) and copy data there */
                    max_characters *= 2;
                    character = realloc(character, max_characters * sizeof(char *));
                    if (character == NULL) {
                        return ENOMEM;
                    }
                    for (i = num_anno; i < max_characters; i++) {
                        character[i] = calloc(MAXLNAME, sizeof(char));
                    }
                }
                strcpy(character[num_anno], annotation_value);
            }
        }
        line++;
    }
    fclose(annotationfile);

    free(annotations);

    num_anno++;

    frequency = calloc(num_anno, sizeof(double));
    if (frequency == NULL) {
        return ENOMEM;
    }

    /* we would need an additional spot in the count array for the missing data,
     * therefore num_anno + 1*/
    count_array = calloc(num_anno + 1, sizeof(int));

    for (i = 0; i < line; i++) {
        if (states[i] == -1) {
            states[i] = num_anno;
            sprintf(character[num_anno], "?");
        }
        count_array[states[i]]++;
    }

    for (i = 0; i <= num_anno; i++) {
        sum_freq += count_array[i];
    }
    printf("\n*** Frequency of %d characters in the MODEL %s ***\n\n", num_anno, model);
    for (i = 0; i < num_anno; i++) {
        if (strcmp(model, "JC") == 0) {
            frequency[i] = ((double) 1) / num_anno;
        } else if (strcmp(model, "F81") == 0) {
            frequency[i] = ((double) count_array[i]) / sum_freq;
        }
        printf("%s = %lf\n", character[i], frequency[i]);
    }

    printf("Frequency of missing data = %lf\n", (double) count_array[i] / (double) sum_freq);

    free(count_array);

    /* we would need two additional spots in the parameter array: for the scaling factor, and for the epsilon,
     * therefore num_anno + 2*/
    parameter = calloc(num_anno + 2, sizeof(double));
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
    s_tree = complete_parse_nh(c_tree, num_anno, collapse_BRLEN);
    if (NULL == s_tree) {
        fprintf(stderr, "A problem occurred while parsing the reference tree.\n");
        return EINVAL;
    }
    root = s_tree->a_nodes[0];
    num_tips = s_tree->nb_taxa;
    for (i = 0; i < num_anno; i++) {
        sum += frequency[i] * frequency[i];
    }
    mu = 1 / (1 - sum);
    parameter[num_anno] = 1.0;
    parameter[num_anno + 1] = s_tree->tip_avg_branch_len / 100.0;

    lnl = calc_lik_bfgs(root, tips, states, num_tips, num_anno, mu, parameter);
    if (lnl == log(0)) {
        fprintf(stderr, "A problem occurred while calculating the likelihood: "
                "Is your tree ok and has at least 2 children per every inner node?\n");
        return EXIT_FAILURE;
    }
    printf("\n*** Initial log likelihood of the tree ***\n\n %lf\n\n", lnl);

    lnl = minimize_params(tips, states, num_tips, num_anno, mu, parameter, character, model);

    if (0 == strcmp("F81", model)) {
        printf("\n*** Optimized frequencies ***\n\n");
        for (i = 0; i < num_anno; i++) {
            printf("%s = %.5f\n", character[i], parameter[i]);
        }
    }
    printf("\n*** Tree scaling factor ***\n\n %.5f \n\n*** Epsilon ***\n\n %.5e",
           parameter[num_anno], parameter[num_anno + 1]);
    printf("\n\n*** Optimised likelihood ***\n\n %lf\n", lnl);

    //Marginal likelihood calculation
    printf("\n*** Calculating Marginal Likelihoods...\n");
    scale = 1.0;
    down_like_marginal(root, num_tips, num_anno, mu, scale, parameter);

    printf("\n*** Predicting likely ancestral states by the Marginal Approximation method...\n\n");
    int res_code = make_samples(num_tips, num_anno, character, parameter, out_annotation_name, out_tree_name);
    if (EXIT_SUCCESS != res_code) {
        return res_code;
    }

    //free all
    free(tips);
    free(character);
    free(frequency);
    free(states);
    free_tree(s_tree, num_anno);

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &samp_fin);
    sec = (double) (samp_fin.tv_sec - samp_ini.tv_sec);
    nano = (double) (samp_fin.tv_nsec - samp_ini.tv_nsec) / 1000.0 / 1000.0 / 1000.0;
    printf("\nTotal execution time = %3.10lf seconds\n\n", (sec + nano));

    return EXIT_SUCCESS;
}
