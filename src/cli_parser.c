#include "cli_parser.h"

void print_usage(const char *prog) {
    fprintf(stderr,
            "usage: %s --policy rr --quantum <ms> <workload.txt>\n"
            "\n"
            "options:\n"
            "  -p, --policy rr       scheduling policy, currently only rr\n"
            "  -q, --quantum <ms>    positive time slice in milliseconds\n"
            "  -v, --verbose         print scheduler lifecycle events\n"
            "  -h, --help            show this help\n",
            prog);
}

bool parse_positive_int(const char *s, int *out) {
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0') {
        return false;
    }

    if (value <= 0 || value > INT_MAX) {
        return false;
    }

    *out = (int)value;
    return true;
}

bool parse_args(int argc, char *argv[], Config *config) {
    static const struct option long_options[] = {
        {"policy", required_argument, NULL, 'p'},
        {"quantum", required_argument, NULL, 'q'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0},
    };

    config->policy = "rr";
    config->quantum_ms = 100;
    config->workload_path = NULL;
    config->verbose = false;

    while (true) {
        int opt = getopt_long(argc, argv, "p:q:vh", long_options, NULL);

        if (opt == -1) {
            break;
        }

        switch (opt) {
            case 'p':
                if (strcmp(optarg, "rr") != 0) {
                    fprintf(stderr, "unsupported policy: %s\n", optarg);
                    return false;
                }
                config->policy = optarg;
                break;

            case 'q':
                if (!parse_positive_int(optarg, &config->quantum_ms)) {
                    fprintf(stderr, "invalid quantum: %s\n", optarg);
                    return false;
                }
                break;

            case 'v':
                config->verbose = true;
                break;

            case 'h':
                print_usage(argv[0]);
                exit(0);

            default:
                return false;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "missing workload file\n");
        return false;
    }

    config->workload_path = argv[optind];
    optind++;

    if (optind < argc) {
        fprintf(stderr, "unexpected argument: %s\n", argv[optind]);
        return false;
    }

    return true;
}
