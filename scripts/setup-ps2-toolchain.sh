#!/usr/bin/env bash
# Installs the pinned compilers and binutils into $REZ_TOOLS_DIR/ps2 (see tools/README.md).
# Needs libc6:i386 and libstdc++6:i386.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

dest="$REZ_TOOLS_DIR/ps2"
cc_dir="$dest/$EE_GCC_ID"
mkdir -p "$dest"
[[ -e /lib/ld-linux.so.2 ]] || die "i386 loader missing; install libc6:i386 (see header)"

if [[ ! -x "$cc_dir/bin/ee-gcc" ]]; then
    tmp="$(mktemp -d)"
    log "Fetching $EE_GCC_ID from $EE_GCC_REPO@$EE_GCC_COMMIT"
    git -C "$tmp" init -q
    git -C "$tmp" remote add origin "$EE_GCC_REPO"
    git -C "$tmp" config core.sparseCheckout true
    echo "$EE_GCC_PATH/" > "$tmp/.git/info/sparse-checkout"
    GIT_LFS_SKIP_SMUDGE=1 git -C "$tmp" fetch -q --depth 1 origin "$EE_GCC_COMMIT"
    git -C "$tmp" checkout -q FETCH_HEAD
    cp -r "$tmp/$EE_GCC_PATH" "$cc_dir"
    rm -rf "$tmp"
fi
cc1_dir="$cc_dir/lib/gcc-lib/ee/2.9-ee-991111-01"
sha256sum -c - >&2 <<SUMS || die "ee-gcc hash mismatch"
$EE_GCC_SHA256_EE_GCC  $cc_dir/bin/ee-gcc
$EE_GCC_SHA256_CC1  $cc1_dir/cc1
$EE_GCC_SHA256_CPP  $cc1_dir/cpp
$EE_GCC_SHA256_AS  $cc_dir/ee/bin/as
SUMS

if [[ ! -x "$dest/binutils/mips-ps2-decompals-ld" ]]; then
    tgz="$REZ_TOOLS_DIR/dl/$(basename "$PS2_BINUTILS_URL")"
    mkdir -p "$REZ_TOOLS_DIR/dl" "$dest/binutils"
    [[ -f "$tgz" ]] || curl -fSL --retry 4 -o "$tgz" "$PS2_BINUTILS_URL"
    echo "$PS2_BINUTILS_SHA256  $tgz" | sha256sum -c - >&2 || die "binutils hash mismatch"
    tar -xzf "$tgz" -C "$dest/binutils"
fi

# The other EE GCC builds some functions are compiled with (config/compilers.json).
while IFS='|' read -r id archive sum; do
    [[ -n "$id" ]] || continue
    [[ -x "$dest/$id/bin/ee-gcc" ]] && continue
    file="$REZ_TOOLS_DIR/dl/$archive"
    mkdir -p "$REZ_TOOLS_DIR/dl" "$dest/$id"
    [[ -f "$file" ]] || curl -fSL --retry 4 -o "$file" "$EE_GCC_EXTRA_URL/$archive"
    echo "$sum  $file" | sha256sum -c - >&2 || die "$archive hash mismatch"
    tar -xf "$file" -C "$dest/$id"
    log "installed $id"
done <<< "$EE_GCC_EXTRA"

# Only the dtls13010 package's GNU as is used (ee-gcc2.9-991111-01+asdtls); its ee/bin/as is a
# wrapper that runs as.bin through the bundled glibc 2.3.6 loader, so check that it starts.
dtls_as="$dest/ee-gcc2.9-991111-01-dtls13010/ee/bin/as"
"$dtls_as" --version </dev/null | grep -q '2.9-ee-991111' || die "dtls13010 as does not run: $dtls_as"

"$cc_dir/bin/ee-gcc" -B"$cc1_dir/" -B"$cc_dir/ee/bin/" --version >/dev/null \
    || die "ee-gcc does not run (missing i386 libraries?)"
log "ee-gcc: $cc_dir/bin/ee-gcc ($("$cc_dir/bin/ee-gcc" --version))"
log "binutils: $dest/binutils"
