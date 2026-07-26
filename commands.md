# Commands

Quick reference for `p101-error-path-walk`. Every script also supports `--help`.
Run `./change-compiler.sh -c <compiler>` once before building.

| Command | What it does |
| --- | --- |
| `./change-compiler.sh -c <cc>` | Configure the build with a compiler (also `./change-compiler.sh <cc>`). `--help` lists detected compilers. |
| `./change-compiler.sh -c <cc> -s address,undefined` | Configure with specific sanitizers |
| `./change-compiler.sh -c <cc> --coverage` | Configure an instrumented build for coverage (gcov) |
| `./build.sh` | Strict analysis build: format-check, clang-tidy, cppcheck, static analyzer, `-Werror`, sanitizers. `-q` = quiet |
| `./build.sh -f` | Auto-fix in place: clang-tidy `--fix` + clang-format |
| `./build.sh -C` | Format check only, no build (hook-friendly); non-zero if unclean |
| `./check.sh` | **The gate:** format + strict build + tests + fuzz smoke -> one PASS/FAIL. `--cov <pct>` adds a coverage gate |
| `./test.sh` | Build & run the Unity test suite (ctest) |
| `./test-all.sh` | Run the tests across every supported compiler |
| `./fuzz.sh` | Run the libFuzzer target (coverage-guided + sanitizers); PASS/FAIL. `-t <secs>` sets the time budget |
| `./coverage-report.sh` | HTML coverage report. `--report-only` skips the run; `--min <pct>` fails under a threshold |
| `./report.sh coverage` \| `profile` | One entry point for the coverage / profiling reports |
| `./doctor.sh` | Report what actually works on this machine for this project |
| `./clean.sh` | Remove `build-` / `coverage-` / `profile-` output (`-n` previews) |
| `./copy-template.sh <dir>` | Start a new project from this template |

Program examples:

| Command | What it does |
| --- | --- |
| `p101-error-path-walk -- ./prog` | Run `./prog` normally, then walk fault injections until no fault fires |
| `p101-error-path-walk -n 0 -- ./prog` | Baseline only |
| `p101-error-path-walk -n 20 -l /tmp/run -- ./prog config.txt` | Run baseline plus fault calls up to 20 using `/tmp/run-*` logs |
| `p101-error-path-walk -F open -- ./prog config.txt` | Walk only fault-capable calls named `open` |
| `p101-error-path-walk -E 24 -- ./prog config.txt` | Inject errno `24` instead of the default `EIO` |
| `p101-error-path-walk -r ../resource-tracker/build-clang/main -- ./prog` | Use an in-tree resource-tracker build |

Less common: `./build-all.sh` (build with every compiler), `./check-compilers.sh`
(detect installed compilers), `./check-env.sh` (verify required tools).
