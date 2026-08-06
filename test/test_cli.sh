#!/usr/bin/env bash
set -euo pipefail

tool="${1:?usage: test_cli.sh <p101-error-path-walk>}"
work="$(mktemp -d "${TMPDIR:-/tmp}/p101-error-path-walk-cli.XXXXXX")"
trap 'rm -rf "$work"' EXIT

expect_status() {
  local expected="$1"
  shift
  local actual

  set +e
  "$@" >"$work/stdout" 2>"$work/stderr"
  actual=$?
  set -e
  if [ "$actual" -ne "$expected" ]; then
    printf 'expected exit %s, got %s: ' "$expected" "$actual" >&2
    printf '%q ' "$@" >&2
    printf '\n' >&2
    cat "$work/stderr" >&2
    exit 1
  fi
}

cat >"$work/fake-run" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

out=
while [ "$#" -gt 0 ]; do
  case "$1" in
    -o) out="$2"; shift 2 ;;
    --observe-tool|--analyze-tool|--model-tool) shift 2 ;;
    --) shift; break ;;
    *) shift ;;
  esac
done

mkdir -p "$out/capture" "$out/analysis"
: >"$out/capture/resources.log"
: >"$out/capture/calls.log"

case "${FAKE_SUMMARY_MODE:-clean}" in
  clean)
    printf '%s\n' '{"schema":"p101-resource-policy-findings-v1","findings":[],"summary":{"records":1,"processes":1,"findings":0,"process_metrics":[]}}' >"$out/analysis/resource-report.json"
    printf '%s\n' '{"schema":"p101-analysis-findings-v1","findings":[],"summary":{"findings":0,"resource_findings":0,"synchronization_findings":0,"trace_findings":0}}' >"$out/analysis/correlated-report.json"
    ;;
  findings)
    printf '%s\n' '{"schema":"p101-resource-policy-findings-v1","findings":[{"id":"P101-FD-001"}],"summary":{"records":1,"processes":1,"findings":1,"process_metrics":[]}}' >"$out/analysis/resource-report.json"
    printf '%s\n' '{"schema":"p101-analysis-findings-v1","findings":[{"id":"P101-FD-001"}],"summary":{"findings":1,"resource_findings":1,"synchronization_findings":0,"trace_findings":0}}' >"$out/analysis/correlated-report.json"
    ;;
  sync-findings)
    printf '%s\n' '{"schema":"p101-resource-policy-findings-v1","findings":[],"summary":{"records":1,"processes":1,"findings":0,"process_metrics":[]}}' >"$out/analysis/resource-report.json"
    printf '%s\n' '{"schema":"p101-analysis-findings-v1","findings":[{"id":"P101-SYNC-001"}],"summary":{"findings":1,"resource_findings":0,"synchronization_findings":1,"trace_findings":0}}' >"$out/analysis/correlated-report.json"
    ;;
  incomplete)
    printf '%s\n' '{"schema":"p101-resource-policy-findings-v1","findings":[],"summary":{"records":1}}' >"$out/analysis/resource-report.json"
    printf '%s\n' '{"schema":"p101-analysis-findings-v1","findings":[],"summary":{"findings":0}}' >"$out/analysis/correlated-report.json"
    ;;
  missing)
    rm -f "$out/capture/resources.log" "$out/analysis/resource-report.json" "$out/analysis/correlated-report.json"
    ;;
  nojson)
    rm -f "$out/analysis/resource-report.json"
    printf '%s\n' '{"schema":"p101-analysis-findings-v1","findings":[],"summary":{"findings":0}}' >"$out/analysis/correlated-report.json"
    ;;
esac

if [ -n "${P101_OBSERVE_CHILD_FAULT_CALL:-}" ] && [ "${P101_OBSERVE_CHILD_FAULT_CALL}" -le "${FAKE_FAULTS:-1}" ]; then
  printf 'P101FAULT\t3\t1\t%s\t%s\t5\terror\t1\tbefore-call\tretry-safe\n' \
    "$P101_OBSERVE_CHILD_FAULT_CALL" "${P101_OBSERVE_CHILD_FAULT_NAME:-open}" \
    >"$P101_OBSERVE_CHILD_FAULT_LOG"
fi

exit "${FAKE_PIPELINE_STATUS:-0}"
EOF
chmod +x "$work/fake-run"

base=("$tool" -U "$work/fake-run" -O observe -Y analyze -B model -l "$work/walk")

expect_status 0 "$tool" --help
expect_status 0 "$tool" -h
expect_status 0 env FAKE_SUMMARY_MODE=clean FAKE_FAULTS=1 "${base[@]}" -n 2 -- true
expect_status 1 env FAKE_SUMMARY_MODE=findings FAKE_FAULTS=1 "${base[@]}" -n 1 -F p101_open -- true
expect_status 1 env FAKE_SUMMARY_MODE=sync-findings FAKE_PIPELINE_STATUS=1 "${base[@]}" -n 0 -- true
expect_status 1 env FAKE_SUMMARY_MODE=findings FAKE_PIPELINE_STATUS=1 "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=incomplete "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=incomplete FAKE_FAULTS=1 "${base[@]}" -n 1 -- true
expect_status 2 env FAKE_SUMMARY_MODE=missing "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=nojson "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=clean FAKE_PIPELINE_STATUS=1 "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=clean FAKE_PIPELINE_STATUS=2 FAKE_FAULTS=1 "${base[@]}" -n 1 -- true
expect_status 0 env FAKE_SUMMARY_MODE=clean "${base[@]}" -v -n 0 -E 5 -F p101_read -M short -A 2 -R 2 -- true

expect_status 2 "$tool"
expect_status 2 "$tool" -z
expect_status 2 "$tool" $'-\001'
expect_status 2 "$tool" -n
expect_status 2 "$tool" -n nope -- true
expect_status 2 "$tool" -n 100001 -- true
expect_status 2 "$tool" -E 0 -- true
expect_status 2 "$tool" -A nope -- true
expect_status 2 "$tool" -R 0 -- true
expect_status 2 "$tool" -l "" -- true
expect_status 2 "$tool" -U "" -- true
expect_status 2 "$tool" -O "" -- true
expect_status 2 "$tool" -Y "" -- true
expect_status 2 "$tool" -B "" -- true
expect_status 2 "$tool" -F "" -- true
expect_status 2 "$tool" -M bogus -- true
expect_status 2 "$tool" -M short -- true
expect_status 2 env P101_FAULT_CALL=1 P101_FAULT_NAME=p101_unsetenv P101_FAULT_ERRNO=5 "$tool" -- true
