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

cat >"$work/fake-observe" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

out=
while [ "$#" -gt 0 ]; do
  case "$1" in
    -o) out="$2"; shift 2 ;;
    --) shift; break ;;
    *) shift ;;
  esac
done

mkdir -p "$out"
: >"$out/resources.log"
: >"$out/calls.log"

case "${FAKE_SUMMARY_MODE:-clean}" in
  clean)
    printf '%s\n' '{"schema":"p101-resource-tracker-findings-v3","records":1,"fd_leaks":0,"allocation_leaks":0,"bad_releases":0,"exec_inheritances":0,"generic_resource_leaks":0,"generic_bad_releases":0,"malformed":0,"bad_version":0,"refused":0,"log_health":{"complete":true}}' >"$out/resource-report.json"
    ;;
  findings)
    printf '%s\n' '{"schema":"p101-resource-tracker-findings-v3","records":1,"fd_leaks":1,"allocation_leaks":1,"bad_releases":1,"exec_inheritances":1,"generic_resource_leaks":1,"generic_bad_releases":1,"malformed":0,"bad_version":0,"refused":0,"log_health":{"complete":true}}' >"$out/resource-report.json"
    ;;
  incomplete)
    printf '%s\n' '{"schema":"p101-resource-tracker-findings-v3","records":1,"fd_leaks":0,"allocation_leaks":0,"bad_releases":0,"exec_inheritances":0,"generic_resource_leaks":0,"generic_bad_releases":0,"malformed":0,"bad_version":0,"refused":0,"log_health":{"complete":false}}' >"$out/resource-report.json"
    ;;
  missing)
    rm -f "$out/resources.log" "$out/resource-report.json"
    ;;
  nojson)
    rm -f "$out/resource-report.json"
    ;;
esac

if [ -n "${P101_OBSERVE_CHILD_FAULT_CALL:-}" ] && [ "${P101_OBSERVE_CHILD_FAULT_CALL}" -le "${FAKE_FAULTS:-1}" ]; then
  printf 'P101FAULT\t2\t1\t%s\t%s\t5\terror\t1\n' \
    "$P101_OBSERVE_CHILD_FAULT_CALL" "${P101_OBSERVE_CHILD_FAULT_NAME:-open}" \
    >"$P101_OBSERVE_CHILD_FAULT_LOG"
fi

exit "${FAKE_OBSERVE_STATUS:-0}"
EOF
chmod +x "$work/fake-observe"

base=("$tool" -O "$work/fake-observe" -r tracker -d sync -t trace -p report -l "$work/walk")

expect_status 0 "$tool" --help
expect_status 0 "$tool" -h
expect_status 0 env FAKE_SUMMARY_MODE=clean FAKE_FAULTS=1 "${base[@]}" -n 2 -- true
expect_status 1 env FAKE_SUMMARY_MODE=findings FAKE_FAULTS=1 "${base[@]}" -n 1 -F open -- true
expect_status 1 env FAKE_SUMMARY_MODE=findings FAKE_OBSERVE_STATUS=1 "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=incomplete "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=incomplete FAKE_FAULTS=1 "${base[@]}" -n 1 -- true
expect_status 2 env FAKE_SUMMARY_MODE=missing "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=nojson "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=clean FAKE_OBSERVE_STATUS=1 "${base[@]}" -n 0 -- true
expect_status 2 env FAKE_SUMMARY_MODE=clean FAKE_OBSERVE_STATUS=2 FAKE_FAULTS=1 "${base[@]}" -n 1 -- true
expect_status 0 env FAKE_SUMMARY_MODE=clean "${base[@]}" -v -n 0 -E 5 -F read -M short -A 2 -R 2 -- true

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
expect_status 2 "$tool" -O "" -- true
expect_status 2 "$tool" -r "" -- true
expect_status 2 "$tool" -d "" -- true
expect_status 2 "$tool" -t "" -- true
expect_status 2 "$tool" -p "" -- true
expect_status 2 "$tool" -F "" -- true
expect_status 2 "$tool" -M bogus -- true
expect_status 2 "$tool" -M short -- true
expect_status 2 env P101_FAULT_CALL=1 P101_FAULT_NAME=unsetenv P101_FAULT_ERRNO=5 "$tool" -- true
