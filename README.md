# p101-error-path-walk

`p101-error-path-walk` is a launcher for p101 programs. It runs a command once as
a baseline and then runs it again with `P101_FAULT_CALL=N` set for each failure
index. Each case delegates to the shared `p101 run` pipeline: `p101-observe`
captures immutable event streams, `p101-event-model` builds one causal model,
and `p101 analyze` applies the resource, synchronization, trace, and correlated
report policies to that model.

The intent is to exercise error paths mechanically:

1. run the program normally;
2. fail p101 call 1;
3. fail p101 call 2;
4. continue through the requested range;
5. capture the case with `p101-observe -C`;
6. verify the capture, build one model, and apply the shared policies;
7. stop automatically when `P101_FAULT_LOG` says no fault fired.

The final output also groups faulted runs by wrapper name, so repeated failures
at the same kind of call site are easier to discuss.

For the security-course framing, see
[`docs/security-error-paths.md`](docs/security-error-paths.md). The short version:
error paths are where many C vulnerability shapes live, and this tool makes
those paths run mechanically.

This tool controls the child process environment. Programs that create their
usual `struct p101_env` with the updated `lib_env` automatically pick up
`P101_FAULT_CALL`, `P101_RESOURCE_LOG`, and `P101_CALL_LOG`.

## Usage

```sh
p101-error-path-walk [-h] [-v] [-n <count>] [-l <prefix>] [-U <p101-run>] [-O <p101-observe>] [-Y <p101-analyze>] [-B <p101-event-model>] [-E <errno>] [-F <name>] [-M <mode>] [-A <amount>] [-R <count>] -- <command> [args...]
```

Options:

- `-h` displays help.
- `-v` enables verbose p101 tracing in the walker.
- `-n <count>` is the maximum injected failure index to try after the baseline
  run. The default is `1024`, but the walk stops early when no fault fires; use
  `0` for a baseline-only run.
- `-l <prefix>` chooses the prefix for per-case run directories.
- `-U <p101-run>` chooses the shared capture/analyze driver.
- `-O <p101-observe>` chooses the observation conductor executable. The default
  is `p101-observe` through `PATH`.
- `-Y <p101-analyze>` chooses the shared analysis driver.
- `-B <p101-event-model>` chooses the causal model builder.
- `-E <errno>` chooses the errno injected by failed wrappers. The default is
  `EIO`.
- `-F <name>` counts and fails only a named p101 wrapper, such as `open`,
  `read`, `malloc`, or `socket`.
- `-M <mode>` selects `error`, `eintr`, `timeout`, or `short`. `eintr` and
  `timeout` inject `EINTR` and `ETIMEDOUT`; `short` performs a real bounded
  `read`, `write`, `pread`, or `pwrite` and returns its partial result.
- `-A <amount>` sets the maximum byte count used by short-I/O injection.
- `-R <count>` injects the selected outcome at the chosen call and the next
  `count - 1` matching calls, which exercises retry loops.

Example:

```sh
p101 walk -- ./my-p101-program config.txt
p101-error-path-walk -F open -E 24 -l /tmp/my-run -- ./my-p101-program config.txt
p101-error-path-walk -F read -M eintr -R 3 -- ./my-p101-program input
p101-error-path-walk -F write -M short -A 1 -- ./my-p101-program
```

Exit status is `0` when every walked error path is clean under the shared
policies, `1` when the analyzed cases contain resource, synchronization, or
trace-policy findings, and `2` when the walker,
baseline run, capture verification, model construction, or policy analysis
failed.
Generic `P101RESOURCE` leaks and invalid lifecycle transitions count the same
way as descriptor and allocation findings.

## Boundaries

`p101-error-path-walk` can only inject failures at p101 wrapper calls that
participate in the `lib_env` fault mechanism. It does not force failures in
direct libc calls, third-party libraries, or code paths that never run under the
chosen command. A clean walk is evidence that the exercised p101-visible error
paths satisfied the current policies; it is not proof that every possible error
path in the program is correct.

Each case directory contains separate `capture/` and `analysis/` directories.
The capture receipt and analysis receipt make the evidence replayable; the
walker never treats an unverified or incomplete event stream as a clean result.

## Build and check

Configure a compiler once, then run the gate:

```sh
./change-compiler.sh -c clang
./check.sh
```

The project was cloned from `template-c-program`, so it keeps the usual strict
Programming 101 build pipeline: clang-format, clang-tidy, cppcheck, Clang Static
Analyzer, Unity tests, fuzz smoke, coverage, and compiler-switch scripts.
