# Error paths as vulnerability paths

`p101-error-path-walk` is a correctness tool, but it is also a small security
lab. Many C vulnerabilities happen after the happy path has already failed:

- cleanup code frees the same object twice;
- one owner closes a descriptor and another owner closes it again;
- a retry path keeps using a partially initialized object;
- a privilege boundary inherits a descriptor that should have been closed;
- an allocation failure skips the destructor for work already completed.

The walker makes those branches run on purpose. It runs the program once as a
baseline, then runs it again with exactly one p101 wrapper call failed:

```text
baseline
fail p101 call #1
fail p101 call #2
fail p101 call #3
...
```

Each injected run follows the same capture/model/policy pipeline as `p101 run`,
so the same run produces:

- a resource log;
- a call log;
- a normalized resource-policy report;
- a call tree;
- a correlated report that ties a finding back to a source site.

That means the lesson is not “remember to check errors.” The lesson is:

> Every cleanup path is executable code. If you can execute it mechanically, you
> can test whether it owns resources correctly.

## Suggested lab framing

1. Give students a small p101 program that is clean on the happy path.
2. Ask them to predict which wrapper failures are likely to expose bugs.
3. Run `p101-error-path-walk`.
4. Fix the first finding only.
5. Run the walker again.
6. Repeat until all injected error paths are clean.

The important part is the loop. The tool should feel less like a magic verdict
and more like a debugger for rarely executed branches.

## Finding classes

Use the stable diagnostic IDs emitted by the shared analysis policies when
writing assignments:

- `P101-FD-001`: leaked descriptor
- `P101-FD-002`: double close
- `P101-FD-003`: close of unknown descriptor
- `P101-ALLOC-001`: leaked allocation
- `P101-ALLOC-002`: double free
- `P101-ALLOC-003`: free of unknown pointer
- `P101-ALLOC-004`: realloc of unknown pointer

These are vulnerability-shaped defects. A course should still teach sanitizers
and platform tools; p101 instrumentation only sees calls routed through p101
wrappers. That limitation is useful pedagogically because it makes the wrapper
boundary visible, but it must be stated plainly.
