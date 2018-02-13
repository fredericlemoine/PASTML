#include "pastml.h"
#include "runpastml.h"
#include <getopt.h>
#include <errno.h>


int main(int argc, char **argv) {
    char *model = "JC";
    char *annotation_name = NULL;
    char *tree_name = NULL;
    char *out_annotation_name = NULL;
    char *out_tree_name = NULL;
    struct timespec;
    double collapse_BRLEN = -1.0;
    int opt;
    char *scaling = "T";
    char *arg_error_string = malloc(sizeof(char) * 1024);

    opterr = 0;

    const char *help_string = "usage: PASTML -a ANNOTATION_FILE -t TREE_NWK [-m MODEL] "
            "[-o OUTPUT_ANNOTATION_FILE] [-n OUTPUT_TREE_NWK] "
            "[-s SCALING_ON_OFF] [-I KEEP_INTERNAL_NODE_IDS_ON_OFF] [-B THRETHOLD_OF_BRANCH_COLLAPSE]\n"
            "\n"
            "required arguments:\n"
            "   -a ANNOTATION_FILE                  path to the annotation csv file containing tip states\n"
            "   -t TREE_NWK                         path to the tree file (in newick format)\n"
            "\n"
            "optional arguments:\n"
            "   -o OUTPUT_ANNOTATION_FILE           path where the output annotation csv file containing node states will be created\n"
            "   -n OUTPUT_TREE_NWK                  path where the output tree file will be created (in newick format)\n"
            "   -m MODEL                            state evolution model (JC or F81)\n"
            "   -s SCALING_ON_OFF                   branch length scaling on (T, by default) or off (F)\n"
            "   -B THRETHOLD_OF_BRANCH_COLLAPSE     define X to collapse branches shorter than 1.0e-X (default: no collapse)\n";

    opt = getopt(argc, argv, "a:t:o:m:n:s:B:");
    do {
        switch (opt) {
            case -1:
                printf(help_string);
                return EINVAL;

            case 'a':
                annotation_name = optarg;
                break;

            case 'o':
                out_annotation_name = optarg;
                break;

            case 'n':
                out_tree_name = optarg;
                break;

            case 't':
                tree_name = optarg;
                break;

            case 'm':
                model = optarg;
                break;

            case 's':
                scaling=optarg;
                break;


            case 'B':
                collapse_BRLEN = pow(0.1, atof(optarg));
                break;

            default: /* '?' */
                snprintf(arg_error_string, 1024, "%s%s", "Unknown arguments...\n\n", help_string);
                printf(arg_error_string);
                free(arg_error_string);
                return EINVAL;
        }
    } while ((opt = getopt(argc, argv, "a:t:o:m:n:s:B:")) != -1);
    /* Make sure that the required arguments are set correctly */
    if (annotation_name == NULL) {
        snprintf(arg_error_string, 1024, "%s%s", "Annotation file (-a) must be specified.\n\n", help_string);
        printf(arg_error_string);
        free(arg_error_string);
        return EINVAL;
    }
    if (tree_name == NULL) {
        snprintf(arg_error_string, 1024, "%s%s", "Tree file (-t) must be specified.\n\n", help_string);
        printf(arg_error_string);
        free(arg_error_string);
        return EINVAL;
    }
    if ((strcmp(model, "JC") != 0) && (strcmp(model, "F81") != 0)) {
        snprintf(arg_error_string, 1024, "%s%s", "Model (-m) must be either JC or F81.\n\n", help_string);
        printf(arg_error_string);
        free(arg_error_string);
        return EINVAL;
    }
    /* No error in arguments */
    free(arg_error_string);

    if (out_annotation_name == NULL) {
        out_annotation_name = calloc(256, sizeof(char));
        sprintf(out_annotation_name, "%s.pastml.out.csv", annotation_name);
    }
    if (out_tree_name == NULL) {
        out_tree_name = calloc(256, sizeof(char));
        sprintf(out_tree_name, "%s.pastml.out.nwk", tree_name);
    }
    return runpastml(annotation_name, tree_name, out_annotation_name, out_tree_name, model, scaling, collapse_BRLEN);
}
