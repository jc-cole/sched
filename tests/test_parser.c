#include "job.h"
#include "parser.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_size(const char *name, size_t actual, size_t expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", name, expected, actual);
        failures++;
    }
}

static void expect_string(const char *name, const char *actual, const char *expected) {
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", name, expected, actual);
        failures++;
    }
}

static void test_basic_command(void) {
    char *lines[] = {"echo hello\n"};
    size_t count = 0;
    Job *jobs = parse_lines(lines, 1, &count);

    expect_size("basic count", count, 1);
    expect_size("basic argc", jobs[0].argc, 2);
    expect_string("basic argv[0]", jobs[0].argv[0], "echo");
    expect_string("basic argv[1]", jobs[0].argv[1], "hello");

    free_job_array(jobs, count);
}

static void test_whitespace_and_quotes(void) {
    char *lines[] = {"  /bin/echo   \"hello world\"  'again please'\n"};
    size_t count = 0;
    Job *jobs = parse_lines(lines, 1, &count);

    expect_size("quotes count", count, 1);
    expect_size("quotes argc", jobs[0].argc, 3);
    expect_string("quotes argv[0]", jobs[0].argv[0], "/bin/echo");
    expect_string("quotes argv[1]", jobs[0].argv[1], "hello world");
    expect_string("quotes argv[2]", jobs[0].argv[2], "again please");

    free_job_array(jobs, count);
}

static void test_comments_and_blank_lines(void) {
    char *lines[] = {
        "# just a comment\n",
        "\n",
        "echo hi # trailing comment\n",
    };
    size_t count = 0;
    Job *jobs = parse_lines(lines, 3, &count);

    expect_size("comments count", count, 1);
    expect_size("comments argc", jobs[0].argc, 2);
    expect_string("comments argv[0]", jobs[0].argv[0], "echo");
    expect_string("comments argv[1]", jobs[0].argv[1], "hi");

    free_job_array(jobs, count);
}

static void test_unterminated_quote_is_rejected(void) {
    char *lines[] = {"echo \"unterminated\n"};
    size_t count = 0;
    Job *jobs = parse_lines(lines, 1, &count);

    expect_size("unterminated count", count, 0);

    free_job_array(jobs, count);
}

int main(void) {
    test_basic_command();
    test_whitespace_and_quotes();
    test_comments_and_blank_lines();
    test_unterminated_quote_is_rejected();

    if (failures != 0) {
        fprintf(stderr, "parser tests failed: %d\n", failures);
        return 1;
    }

    printf("parser tests passed\n");
    return 0;
}
