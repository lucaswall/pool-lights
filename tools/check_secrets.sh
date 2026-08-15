#!/usr/bin/env bash
# RULE 0 backstop. .gitignore is the actual control — git refuses to stage an ignored
# file without -f, so secrets.h, local/ and CLAUDE.local.md cannot reach a commit by
# accident. This only catches the mistake gitignore cannot: a real address or credential
# typed into a tracked file.
#
#   tools/check_secrets.sh [--staged]

set -uo pipefail
cd "$(git rev-parse --show-toplevel)" || exit 2

if [ "${1:-}" = "--staged" ]; then
  content=$(git diff --cached --diff-filter=ACMR -U0 | grep '^+' | grep -v '^+++')
else
  content=$(git grep -nI '' -- . ':!tools/check_secrets.sh' 2>/dev/null)
fi

# No \b: git's regex engine matches nothing on it, which would silently disable the check.
ipv4='[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}'
mac='[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}'
cred='(PASSWORD|PASSWD|SECRET|TOKEN|API_?KEY|PSK)[[:space:]"]*[=:][[:space:]]*"?[A-Za-z0-9._/+-]{6,}'
allow='192\.0\.2\.|198\.51\.100\.|203\.0\.113\.|0\.0\.0\.0|127\.0\.0\.1|255\.255\.255\.255|<[A-Z_]+>|your-|example|placeholder|TODO|xx:xx'

fail=0
for pat in "$ipv4" "$mac" "$cred"; do
  hits=$(printf '%s\n' "$content" | grep -E "$pat" | grep -Ev "$allow")
  if [ -n "$hits" ]; then
    printf 'SUSPECT /%s/\n%s\n' "$pat" "$hits" | sed 's/^/        /'
    fail=1
  fi
done

if [ "$fail" -ne 0 ]; then
  echo "RULE 0: use <HUB_IP> or an RFC 5737 address (192.0.2.x). Real values go in local/." >&2
  exit 1
fi
echo clean
