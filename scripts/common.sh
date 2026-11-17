# Shared paths for the scripts in this directory. Source it; do not execute it.

REZ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=../config/versions/toolchain.env
source "$REZ_ROOT/config/versions/toolchain.env"
REZ_TOOLS_DIR="${REZ_TOOLS_DIR:-$REZ_ROOT/.tools}"

log() { printf '[rez] %s\n' "$*" >&2; }
die() { printf '[rez] error: %s\n' "$*" >&2; exit 1; }
