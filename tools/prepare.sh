#!/usr/bin/env bash
# prepare.sh - build a VMS-ready source tree in staging/<name>-<version>/
#
#   1. fetch + verify the upstream tarball
#   2. extract it, apply patches/series, lay overlay/ over the top
#   3. run the upstream configure on this host, with every platform answer
#      taken from VMS probe results (probed.site) or hand-settled values
#      (vms-manual.site) instead of from Linux
#   4. copy configure's defines.h into the tree
#   5. write the MMS source lists and the configuration snapshot
#
# Nothing in staging/ is ever edited by hand: fix things in patches/ or overlay/.
set -euo pipefail

top=$(cd "$(dirname "$0")/.." && pwd)
. "$top/upstream.conf"
name=$UPSTREAM_NAME-$UPSTREAM_VERSION
tarball=$top/cache/$(basename "$UPSTREAM_URL")
stage=$top/staging/$name
hostcfg=$top/cache/hostcfg-$name
cfgdir=$top/overlay/vms/config
snapshot=$top/snapshot
# Configuration answers come from this node's VSI C run; both architectures
# share one CRTL feature set (see docs/vms-environment.md).
PRIMARY_NODE=${PRIMARY_NODE:-ia64}
PRIMARY_TRIPLET=ia64-hp-openvms

step() { echo "prepare: $*"; }
die() { echo "prepare: error: $*" >&2; exit 1; }

"$top/tools/fetch.sh" >/dev/null

# --- 2. extract, patch, overlay -------------------------------------------
step "extracting $name"
rm -rf "$stage"
mkdir -p "$top/staging"
tar -xf "$tarball" -C "$top/staging"
[ -d "$stage" ] || die "tarball did not unpack to $stage"
# Some tarballs (less) unpack read-only; the copies pushed to VMS must be
# writable, or the next push cannot replace them.
chmod -R u+w "$stage"

while read -r p; do
    case $p in ''|'#'*) continue ;; esac
    step "patch $p"
    patch -d "$stage" -p1 -s --no-backup-if-mismatch -F0 < "$top/patches/$p" ||
        die "patch $p does not apply cleanly"
done < "$top/patches/series"

# overlay/ may only add files; changes to upstream files belong in patches/.
(cd "$top/overlay" && find . -type f) | while read -r f; do
    [ -e "$stage/$f" ] && die "overlay/$f would replace an upstream file; use a patch"
    true
done
cp -a "$top/overlay/." "$stage/"

# --- 3. host configure with VMS answers ----------------------------------
step "configure (host, VMS answers)"
rm -rf "$hostcfg"
mkdir -p "$hostcfg"
site=$hostcfg/vms.site
# Answers: the VSI C configure run (vms_configure.sh) if there is one, else the
# function/header probes; vms-manual.site last so it always wins.
answers=$cfgdir/configure-$PRIMARY_NODE.cache
if [ ! -f "$answers" ]; then
    # First pass of a new release: stage the tree for vms_configure.sh, which
    # writes the answers.  The result is not buildable on VMS yet.
    answers=$hostcfg/no-answers.site; : > "$answers"
    step "WARNING: no $(basename "$cfgdir")/configure-$PRIMARY_NODE.cache yet:" \
         "Linux answers (run tools/vms_configure.sh, then prepare again)"
fi
step "answers from $(basename "$answers")"
python3 "$top/tools/nextheaders_site.py" "$stage/configure" "$cfgdir/crtl_modules.txt" \
    > "$cfgdir/next-headers.site"
cat "$answers" "$cfgdir/next-headers.site" "$cfgdir/vms-manual.site" > "$site"
mapfile -t cfgargs < <(grep -v -e '^#' -e '^$' "$cfgdir/configure.args")
# Same --host as vms_configure.sh so configure takes the same code paths.
(cd "$hostcfg" && CONFIG_SITE=$site "$stage/configure" -q -C \
    --build="$("$stage/build-aux/config.guess" 2>/dev/null || gcc -dumpmachine)" --host=$PRIMARY_TRIPLET CC=gcc "${cfgargs[@]}" \
    > configure.out 2>&1) || { tail -20 "$hostcfg/configure.out"; die "configure failed"; }

# --- 4. generated headers and config.h -------------------------------------
printvar() {  # printvar <dir> <make variable>
    make -s -C "$hostcfg/$1" -f Makefile -f "$top/tools/printvar.mk" "print-$2"
}
# less has no gnulib: configure writes only defines.h (AC_CONFIG_HEADERS).
cp "$hostcfg/defines.h" "$stage/defines.h"

# --- 5. MMS source lists ---------------------------------------------------
# LESS.EXE: the Makefile's OBJ (with REGEX_O and lesskey_parse), as .c files.
src_srcs=$(printvar . OBJ | tr ' ' '\n' | sed -n 's/\.o$/.c/p' | sort -u)
[ -n "$src_srcs" ] || die "no sources in the Makefile's OBJ"
dups=$(printf '%s\n' "$src_srcs" $(grep -v '^#' "$top/overlay/vms/extra-sources.txt") \
           | grep . | xargs -n1 basename | sort | uniq -d)
[ -z "$dups" ] || die "duplicate object names: $dups"
lib_srcs=

mkdir -p "$stage/vms"
: > "$hostcfg/lib-sources.txt"
echo "$src_srcs" > "$hostcfg/src-sources.txt"
cp "$top/overlay/vms/extra-sources.txt" "$hostcfg/extra-sources.txt"
GEN_MMS_SRC_BASE= GEN_MMS_CONFIG_DIR= GEN_MMS_CONFIG_H=defines.h python3 "$top/tools/gen_mms.py" \
    "$cfgdir/ccflags.txt" "$hostcfg/lib-sources.txt" "$hostcfg/src-sources.txt" "$hostcfg/extra-sources.txt" \
    > "$stage/vms/sources.mms"

if [ -d "$stage/vms/kit" ]; then
step "PCSI kit inputs"
: "${KIT_PRODUCER:=ISSINOHO}"
# Versions: our VMS patch level is the ECO, so $UPSTREAM_VERSION-vms$VMS_PATCH_LEVEL is
# V<major>.<minor>-<update>E<level>.  less's versions are one number, which PCSI rejects as
# a major version (%PCSI-E-S_INVVER for V710.0): 710 is V7.10, hundreds and the rest, which
# keeps the order (711 V7.11, 800 V8.0).
IFS=. read -r major minor update _ <<< "$UPSTREAM_VERSION"
if [ -z "${minor:-}" ]; then
    minor=$((major % 100)); major=$((major / 100))
fi
pcsiversion="V$major.$minor-${update:-0}E$VMS_PATCH_LEVEL"
kitversion="$UPSTREAM_VERSION-vms$VMS_PATCH_LEVEL"
kit=$stage/vms/kit
subst() {
    sed -e "s/@PRODUCER@/$KIT_PRODUCER/g" -e "s/@BASE@/$1/g" \
        -e "s/@PCSIVERSION@/$pcsiversion/g" -e "s/@VERSION@/$UPSTREAM_VERSION/g" \
        -e "s/@KITVERSION@/$kitversion/g" -e "s/@ARCH@/$2/g"
}
for base in I64VMS X86VMS; do
    subst $base "" < "$kit/less.pcsi\$desc_template" > "$kit/LESS-$base.PCSI\$DESC"
    subst $base "" < "$kit/less.pcsi\$text_template" > "$kit/LESS-$base.PCSI\$TEXT"
done
rm -f "$kit/less.pcsi\$desc_template" "$kit/less.pcsi\$text_template"
mv "$kit/less\$startup.com" "$kit/LESS\$STARTUP.COM"
mv "$kit/less\$setup.com" "$kit/LESS\$SETUP.COM"
subst "" "IA64 and x86-64" < "$kit/readme.vms" > "$kit/README.VMS"; rm -f "$kit/readme.vms"
mkdir -p "$kit/doc"
cp "$stage/COPYING" "$kit/doc/COPYING."
cp "$stage/NEWS" "$kit/doc/NEWS."
for man in less.nro; do
    up=$(basename "$man" .nro | tr a-z A-Z)
    cp "$stage/$man" "$kit/doc/$up.1"
    groff -man -Tascii -P-cbou "$stage/$man" > "$kit/doc/$up.TXT" 2>/dev/null
    [ -s "$kit/doc/$up.TXT" ] || die "groff did not render $man"
done
printf 'KIT_PRODUCER=%s\nPCSI_VERSION=%s\nKIT_VERSION=%s\n' "$KIT_PRODUCER" "$pcsiversion" \
    "$kitversion" > "$kit/kit.env"
fi

# --- snapshot: the resolved configuration, committed and reviewed ----------
mkdir -p "$snapshot"
cp "$hostcfg/defines.h" "$snapshot/defines.h"
echo "$src_srcs" > "$snapshot/src-sources.txt"
# Every cached answer, and where it came from.
cat "$cfgdir/next-headers.site" "$cfgdir/vms-manual.site" > "$hostcfg/manual.site"
python3 "$top/tools/cfgreport.py" "$hostcfg/config.cache" "$answers" \
    "$hostcfg/manual.site" > "$snapshot/cache-answers.txt"
step "inherited-from-Linux answers: $(grep -c ' host$' "$snapshot/cache-answers.txt" || true)" \
     "(see snapshot/cache-answers.txt)"

step "staged $stage"
if ! git -C "$top" diff --quiet -- snapshot 2>/dev/null; then
    step "snapshot/ changed - review with: git diff -- snapshot"
fi
