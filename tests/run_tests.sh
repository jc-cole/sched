#!/usr/bin/env bash

set -u

SCHED=${SCHED:-./bin/sched}
TMP_DIR=$(mktemp -d)
FAILURES=0

cleanup() {
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

record_failure() {
    printf 'FAIL: %s\n' "$1" >&2
    FAILURES=$((FAILURES + 1))
}

record_pass() {
    printf 'PASS: %s\n' "$1"
}

expect_exit() {
    name=$1
    expected=$2
    shift 2

    out_file="$TMP_DIR/$name.out"
    err_file="$TMP_DIR/$name.err"

    "$@" >"$out_file" 2>"$err_file"
    rc=$?

    if [ "$expected" = "0" ] && [ "$rc" -ne 0 ]; then
        record_failure "$name exited $rc, expected 0"
        return
    fi

    if [ "$expected" = "nonzero" ] && [ "$rc" -eq 0 ]; then
        record_failure "$name exited 0, expected nonzero"
        return
    fi

    record_pass "$name"
}

expect_contains() {
    name=$1
    file=$2
    text=$3

    if grep -Fq -- "$text" "$file"; then
        record_pass "$name"
    else
        record_failure "$name missing \"$text\""
    fi
}

expect_not_contains() {
    name=$1
    file=$2
    text=$3

    if grep -Fq -- "$text" "$file"; then
        record_failure "$name unexpectedly contained \"$text\""
    else
        record_pass "$name"
    fi
}

expect_fd_clean() {
    name=$1
    file=$2

    if ! grep -Fq -- "FILE DESCRIPTORS:" "$file"; then
        record_failure "$name missing Valgrind fd summary"
        return
    fi

    if grep -Eq 'Open file descriptor [0-9]+: .*children\.log|anon_inode:\[(timerfd|signalfd)\]' "$file"; then
        record_failure "$name found scheduler-owned fd open at exit"
        return
    fi

    record_pass "$name"
}

write_file() {
    path=$1
    content=$2
    printf '%b' "$content" >"$path"
}

help_out="$TMP_DIR/help.out"
"$SCHED" --help >"$help_out" 2>&1
help_rc=$?
if [ "$help_rc" -eq 0 ]; then
    record_pass "help exits 0"
else
    record_failure "help exited $help_rc"
fi
expect_contains "help prints usage" "$help_out" "usage:"
expect_contains "help shows verbose" "$help_out" "--verbose"

valid_workload="$TMP_DIR/valid.workload"
write_file "$valid_workload" '/bin/echo valid\n'

expect_exit "missing workload rejected" nonzero "$SCHED" --policy rr --quantum 100
expect_exit "invalid quantum rejected" nonzero "$SCHED" --policy rr --quantum abc "$valid_workload"
expect_exit "zero quantum rejected" nonzero "$SCHED" --policy rr --quantum 0 "$valid_workload"
expect_exit "unsupported policy rejected" nonzero "$SCHED" --policy fifo --quantum 100 "$valid_workload"
expect_exit "extra positional arg rejected" nonzero "$SCHED" --policy rr --quantum 100 "$valid_workload" extra

simple_workload="$TMP_DIR/simple.workload"
write_file "$simple_workload" '/bin/echo simple-ok\n'
expect_exit "simple workload exits 0" 0 "$SCHED" --policy rr --quantum 100 "$simple_workload"
expect_contains "simple summary includes success" "$TMP_DIR/simple workload exits 0.out" "exited(0)"
expect_contains "simple child output captured" children.log "simple-ok"

comments_workload="$TMP_DIR/comments.workload"
write_file "$comments_workload" '# ignored\n\n/bin/echo only-real\n'
expect_exit "comments and blanks ignored" 0 "$SCHED" --policy rr --quantum 100 "$comments_workload"
expect_contains "comments produce one job" "$TMP_DIR/comments and blanks ignored.out" "jobs=1"
expect_contains "comments child output captured" children.log "only-real"

quoted_workload="$TMP_DIR/quoted.workload"
write_file "$quoted_workload" '/bin/echo "hello world"\n'
expect_exit "quoted argument workload exits 0" 0 "$SCHED" --policy rr --quantum 100 "$quoted_workload"
expect_contains "quoted child output captured" children.log "hello world"

no_newline_workload="$TMP_DIR/no-newline.workload"
printf '/bin/echo no-newline' >"$no_newline_workload"
expect_exit "no final newline exits 0" 0 "$SCHED" --policy rr --quantum 100 "$no_newline_workload"
expect_contains "no final newline child output captured" children.log "no-newline"

missing_command_workload="$TMP_DIR/missing-command.workload"
write_file "$missing_command_workload" '/no/such/sched-command\n/bin/echo after-missing\n'
expect_exit "missing command workload exits 0" 0 "$SCHED" --policy rr --quantum 100 "$missing_command_workload"
expect_contains "missing command reported" "$TMP_DIR/missing command workload exits 0.out" "exited(127)"
expect_contains "remaining command still ran" children.log "after-missing"

verbose_workload="$TMP_DIR/verbose.workload"
write_file "$verbose_workload" '/bin/echo verbose-ok\n'
expect_exit "verbose workload exits 0" 0 "$SCHED" --policy rr --quantum 100 --verbose "$verbose_workload"
expect_contains "verbose includes launch" "$TMP_DIR/verbose workload exits 0.out" "launch:"
expect_contains "verbose includes schedule" "$TMP_DIR/verbose workload exits 0.out" "schedule:"
expect_contains "verbose includes event" "$TMP_DIR/verbose workload exits 0.out" "event:"

expect_exit "normal output exits 0" 0 "$SCHED" --policy rr --quantum 100 "$verbose_workload"
expect_not_contains "normal excludes debug statuses" "$TMP_DIR/normal output exits 0.out" "Current job statuses"
expect_not_contains "normal excludes launch result" "$TMP_DIR/normal output exits 0.out" "launch jobs result"
expect_not_contains "normal excludes scheduler result" "$TMP_DIR/normal output exits 0.out" "round robin result"
expect_contains "normal includes summary header" "$TMP_DIR/normal output exits 0.out" "PID      STATUS"

if command -v valgrind >/dev/null 2>&1; then
    valgrind_out="$TMP_DIR/valgrind.out"
    valgrind --leak-check=full --track-fds=yes --trace-children=no "$SCHED" --policy rr --quantum 100 "$simple_workload" >"$valgrind_out" 2>&1
    valgrind_rc=$?

    if [ "$valgrind_rc" -eq 0 ]; then
        record_pass "valgrind exits 0"
    else
        record_failure "valgrind exited $valgrind_rc"
    fi

    expect_contains "valgrind no leaks" "$valgrind_out" "All heap blocks were freed"
    expect_fd_clean "valgrind scheduler fds closed" "$valgrind_out"
    expect_contains "valgrind no errors" "$valgrind_out" "ERROR SUMMARY: 0 errors"
else
    printf 'SKIP: valgrind not installed\n'
fi

if [ "$FAILURES" -ne 0 ]; then
    printf '\n%d test checks failed\n' "$FAILURES" >&2
    exit 1
fi

printf '\nintegration tests passed\n'
