#!/usr/bin/env bash
# RULE 0 tripwire. Scans for the shape of a site-specific value, never for a literal one:
# a scanner containing the real password or device ID would be the leak it prevents.
#
#   tools/check_secrets.sh            scan tracked files
#   tools/check_secrets.sh --staged   scan what is about to be committed
#
# It cannot recognise a sniffed MiLight device ID, a hostname, or a person's name.
# Read your own diff.

set -uo pipefail

case "${1:-tracked}" in
  tracked) STAGED=0 ;;
  --staged) STAGED=1 ;;
  *) echo "usage: $0 [--staged]" >&2; exit 2 ;;
esac

cd "$(git rev-parse --show-toplevel)" || exit 2

if [ "$STAGED" -eq 1 ]; then
  files=$(git diff --cached --name-only --diff-filter=ACMR)
  content=$(git diff --cached --diff-filter=ACMR -U0 | grep '^+' | grep -v '^+++')
else
  files=$(git ls-files)
  content=$(git grep -nI '' -- . ':!tools/check_secrets.sh' 2>/dev/null)
fi

fail=0

while IFS= read -r f; do
  [ -n "$f" ] || continue
  case "$f" in
    local/README.md) ;;
    CLAUDE.local.md|local/*|include/secrets.h|logs/*|compile_commands.json)
      echo "BLOCKED  $f — private by design, must never be committed"; fail=1 ;;
  esac
done <<< "$files"

# No \b anywhere: git's regex engine silently matches nothing on it, which turns the whole
# check into a no-op. Filtering is done with plain grep on already-extracted content.
ipv4='[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}'
mac='[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}'
cred='(PASSWORD|PASSWD|SECRET|TOKEN|API_?KEY|PSK)[[:space:]"]*[=:][[:space:]]*"?[A-Za-z0-9._/+-]{6,}'

# RFC 5737 documentation ranges, unspecified/loopback/broadcast, and obvious placeholders.
allow='192\.0\.2\.|198\.51\.100\.|203\.0\.113\.|0\.0\.0\.0|127\.0\.0\.1|255\.255\.255\.255|<[A-Z_]+>|your-|example|placeholder|TODO|xx:xx'

for pat in "$ipv4" "$mac" "$cred"; do
  hits=$(printf '%s\n' "$content" | grep -E "$pat" | grep -Ev "$allow")
  if [ -n "$hits" ]; then
    echo "SUSPECT  /$pat/"
    printf '%s\n' "$hits" | sed 's/^/         /'
    fail=1
  fi
done

if [ "$fail" -ne 0 ]; then
  cat >&2 <<'EOF'

RULE 0: this repository is destined to be public. Site-specific values belong in local/
or include/secrets.h, both gitignored. Use <HUB_IP> or an RFC 5737 address (192.0.2.x).
Make a false positive obviously generic rather than widening the allowlist.
EOF
  exit 1
fi

[ "$STAGED" -eq 1 ] && echo "clean — staged content" || echo "clean — tracked content"
