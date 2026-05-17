# sched

`sched` is a Linux user-space round-robin process scheduler written in C. It reads a workload file, launches each line as a real child process, and time-slices those processes with `SIGSTOP`/`SIGCONT` using a `timerfd`-driven event loop.

This is an educational systems project, not a production process supervisor. The goal is to make Unix process control visible: `fork`, `execvp`, process groups, signals, child reaping, timers, and CLI/test hygiene all show up in one small codebase.

## Demo

Example workload:

```text
echo start
sleep 1
sleep 2
sleep 3
echo end
```

Run it with a 100ms round-robin quantum:

```sh
./bin/sched --policy rr --quantum 100 workload.txt
```

Example output:

```text
sched: policy=rr quantum=100ms jobs=5

PID      STATUS        QUANTA   COMMAND
1234     exited(0)     1        echo start
1235     exited(0)     4        sleep 1
1236     exited(0)     9        sleep 2
1237     exited(0)     19       sleep 3
1238     exited(0)     1        echo end

child output: children.log
```

Use `--verbose` to see scheduler lifecycle events:

```sh
./bin/sched --policy rr --quantum 100 --verbose workload.txt
```

Verbose mode prints launches, scheduling decisions, and state transitions as jobs are continued, stopped, exited, or terminated.

## Features

- Round-robin scheduling for real Unix child processes.
- Configurable quantum in milliseconds.
- Workload parser with whitespace, quotes, blank lines, and comments.
- Child output redirection to `children.log`.
- Process group management for coordinated signal handling.
- `SIGCHLD` and `SIGINT` handling through `signalfd`.
- Quantum expiration through `timerfd`.
- `poll`-based event loop.
- Normal and verbose output modes.
- Parser tests, CLI integration tests, and Valgrind leak/fd checks.

## Build

Requirements:

- Linux
- GCC or compatible C compiler
- Make
- Valgrind, optional but used by `make test` when available

Build:

```sh
make
```

Run the sample workload:

```sh
./bin/sched --policy rr --quantum 100 workload.txt
```

Clean generated build output:

```sh
make clean
```

## Usage

```text
usage: ./bin/sched --policy rr --quantum <ms> <workload.txt>

options:
  -p, --policy rr       scheduling policy, currently only rr
  -q, --quantum <ms>    positive time slice in milliseconds
  -v, --verbose         print scheduler lifecycle events
  -h, --help            show this help
```

Workload files contain one command per line. Each command is parsed into an `argv` array and executed with `execvp`.

Supported workload syntax:

```text
# comments and blank lines are ignored
echo "hello world"
sleep 1
/bin/echo 'single quoted argument'
```

## Testing

Run the full test suite:

```sh
make test
```

The suite includes:

- Parser unit tests for tokenization, quotes, comments, and syntax errors.
- CLI integration tests for invalid arguments and workload edge cases.
- Normal/verbose output checks.
- Missing executable behavior.
- Valgrind checks for heap leaks, open scheduler file descriptors, and memory errors.

You can also run the Valgrind check directly:

```sh
make valgrind
```

## Architecture

The project is split into small modules:

- `cli_parser`: validates policy, quantum, verbosity, and workload path.
- `parser`: tokenizes workload lines into `Job` structs with `argv` arrays.
- `launcher`: forks each job, redirects child output, assigns process groups, and verifies `execvp` readiness.
- `rr`: runs the round-robin scheduler loop.
- `signals`: creates `signalfd` descriptors for signal-driven event handling.
- `timer`: manages the scheduler quantum timer with `timerfd`.
- `reap`: updates job state from `waitpid` results.
- `util`: output helpers for normal summaries and verbose lifecycle logs.

At runtime, the scheduler waits on three event sources with `poll`:

- the quantum timer,
- `SIGCHLD` notifications,
- `SIGINT` from the user.

When a quantum expires, the current child is stopped and the next stopped job is continued. When a child exits or terminates, the reaper updates its final status and the scheduler moves on.

## Limitations

- Linux-specific: uses `timerfd` and `signalfd`.
- Currently implements only round-robin scheduling.
- Workload parsing is intentionally smaller than shell parsing.
- No pipes, redirects, glob expansion, variable expansion, or shell built-ins.
- Child output is redirected to a fixed `children.log` file.
- Intended for learning and demonstration, not as a production process manager.

## Roadmap

- Add a `--log-file` option for child output.
- Add scheduler metrics such as wall-clock runtime and turnaround time.
- Add another policy, such as first-come-first-served, for comparison.
- Improve parser error reporting with line numbers.
- Add CI for build and test automation.
