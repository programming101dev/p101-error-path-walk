# p101-error-path-walk

`p101-error-path-walk` is a launcher for p101 programs. It runs a command once as
a baseline and then runs it again with `P101_FAULT_CALL=N` set for each failure
index. Each run delegates the actual observation/report bundle to `p101-observe`,
so every case gets the same resource log, call log, trace tree, resource report,
and correlated report layout as an ordinary observed run.

The intent is to exercise error paths mechanically:

1. run the program normally;
2. fail p101 call 1;
3. fail p101 call 2;
4. continue through the requested range;
5. run `p101-observe` for that case;
6. let `p101-observe` feed the logs to `p101-resource-tracker`, `p101-trace`, and
   `p101-report`;
7. stop automatically when `P101_FAULT_LOG` says no fault fired.

This tool controls the child process environment. Programs that create their
usual `struct p101_env` with the updated `lib_env` automatically pick up
`P101_FAULT_CALL`, `P101_RESOURCE_LOG`, and `P101_CALL_LOG`.

## Usage

```sh
p101-error-path-walk [-h] [-v] [-n <count>] [-l <prefix>] [-O <p101-observe>] [-r <p101-resource-tracker>] [-t <p101-trace>] [-p <p101-report>] [-E <errno>] [-F <name>] -- <command> [args...]
```

Options:

- `-h` displays help.
- `-v` enables verbose p101 tracing in the walker.
- `-n <count>` is the maximum injected failure index to try after the baseline
  run. The default is `1024`, but the walk stops early when no fault fires; use
  `0` for a baseline-only run.
- `-l <prefix>` chooses the prefix for per-run observe directories and fault
  logs.
- `-O <p101-observe>` chooses the observation conductor executable. The default
  is `p101-observe` through `PATH`.
- `-r <p101-resource-tracker>` chooses the analyzer executable. The default is
  `p101-resource-tracker` through `PATH`.
- `-t <p101-trace>` chooses the trace renderer executable. The default is
  `p101-trace` through `PATH`.
- `-p <p101-report>` chooses the correlated reporter executable. The default is
  `p101-report` through `PATH`.
- `-E <errno>` chooses the errno injected by failed wrappers. The default is
  `EIO`.
- `-F <name>` counts and fails only a named p101 wrapper, such as `open`,
  `read`, `malloc`, or `socket`.

Example:

```sh
p101-error-path-walk -O ../p101-observe/build-clang/p101-observe -r ../p101-resource-tracker/build-clang/p101-resource-tracker -t ../p101-trace/build-clang/p101-trace -p ../p101-report/build-clang/p101-report -- ./my-p101-program config.txt
p101-error-path-walk -F open -E 24 -l /tmp/my-run -- ./my-p101-program config.txt
```

Exit status is `0` when every walked error path is resource-clean, `1` when the
observed cases contain leaks or bad releases, and `2` when the walker, baseline
run, `p101-observe`, or one of the helper analyzers failed.

## Build and check

Configure a compiler once, then run the gate:

```sh
./change-compiler.sh -c clang
./check.sh
```

The project was cloned from `template-c-program`, so it keeps the usual strict
Programming 101 build pipeline: clang-format, clang-tidy, cppcheck, Clang Static
Analyzer, Unity tests, fuzz smoke, coverage, and compiler-switch scripts.
