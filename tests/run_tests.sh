#!/usr/bin/env bash
# Sanity tests for Part I. Run from the repository root: make test
set -u
B=./bin
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
pass() { echo "  ok   $1"; }
bad()  { echo "  FAIL $1"; fail=1; }

echo "== I/O round-trips (img_quant with all bits must be byte-identical)"
for f in imgs-pgm8/*.pgm imgs-ppm/*.ppm; do
    [ -f "$f" ] || continue
    bits=$(head -c 20 "$f" | grep -q "65535" && echo 16 || echo 8)
    # PNM files written by us use a canonical header, so compare samples with img_cmp
    $B/img_quant -b "$bits" "$f" "$T/rt" >/dev/null &&
        $B/img_cmp "$f" "$T/rt" | tail -1 | grep -q "inf$" && pass "$f" || bad "$f"
done
for f in imgs-raw16/*.raw; do
    [ -f "$f" ] || continue
    dims=$(echo "$f" | sed -E 's/.*-1x([0-9]+)x([0-9]+)\.raw/\1 \2/')
    set -- $dims
    $B/img_quant -b 16 -rows "$1" -cols "$2" -bpp 16 -be "$f" "$T/rt.raw" >/dev/null &&
        cmp -s "$f" "$T/rt.raw" && pass "$f (BE)" || bad "$f (BE)"
    dd if="$f" conv=swab of="$T/le.raw" status=none
    $B/img_quant -b 16 -rows "$1" -cols "$2" -bpp 16 -le "$T/le.raw" "$T/rt.raw" >/dev/null &&
        cmp -s "$T/le.raw" "$T/rt.raw" && pass "$f (LE)" || bad "$f (LE)"
    # Same entropies regardless of byte order
    a=$($B/img_hist -noplot -rows "$1" -cols "$2" -bpp 16 -be "$f" | tail -1)
    b=$($B/img_hist -noplot -rows "$1" -cols "$2" -bpp 16 -le "$T/le.raw" | tail -1)
    [ "$a" = "$b" ] && pass "$f entropies BE == LE" || bad "$f entropies BE == LE"
done

echo "== Quantization: PSNR close to 6.02*b + 10.8 dB for mid-point reconstruction"
f=imgs-pgm8/lena.pgm
if [ -f "$f" ]; then
    for b in 7 6 5 4 3; do
        $B/img_quant -b $b "$f" "$T/q.pgm" >/dev/null
        line=$($B/img_cmp "$f" "$T/q.pgm" | tail -1)
        maxe=$(echo "$line" | awk '{print $2}')
        psnr=$(echo "$line" | awk '{print $4}')
        exp_max=$(( 1 << (8 - b - 1) ))
        ok=$(awk -v p="$psnr" -v b="$b" 'BEGIN{t=6.02*b+10.8; d=p-t; if(d<0)d=-d; print (d<2.5)?1:0}')
        [ "$maxe" = "$exp_max" ] && [ "$ok" = 1 ] && pass "b=$b maxerr=$maxe psnr=$psnr" ||
            bad "b=$b maxerr=$maxe (expected $exp_max) psnr=$psnr"
    done
fi

echo "== Error handling"
$B/img_quant -b 9 imgs-pgm8/lena.pgm "$T/x.pgm" 2>/dev/null && bad "rejects -b 9 on 8-bit image" || pass "rejects -b 9 on 8-bit image"
$B/img_hist -noplot -bits 2-8 imgs-pgm8/lena.pgm >/dev/null 2>&1 && bad "rejects bit range 2-8" || pass "rejects bit range 2-8"
$B/img_cmp imgs-pgm8/lena.pgm imgs-ppm/lena.ppm >/dev/null 2>&1 && bad "rejects size mismatch" || pass "rejects size mismatch"
$B/img_hist -noplot -rows 10000 -cols 10000 imgs-pgm8/lena.pgm >/dev/null 2>&1 && bad "rejects short RAW" || pass "rejects short RAW"

echo "== Lossless codec: decoded file must be byte-identical to the original"
# lossless_rt file [RAW options...]
lossless_rt() {
    local f=$1; shift
    if $B/img_lossless_enc "$@" "$f" "$T/ll.c" 2>"$T/ll.err" && $B/img_lossless_dec "$T/ll.c" "$T/ll.d" 2>>"$T/ll.err" &&
        cmp -s "$f" "$T/ll.d"; then
        local a b; a=$(stat -c %s "$f"); b=$(stat -c %s "$T/ll.c")
        pass "$(basename "$f") ($a -> $b bytes)"
    else
        bad "$(basename "$f"): $(head -c 200 "$T/ll.err")"
    fi
}
for f in imgs-pgm8/*.pgm imgs-ppm/*.ppm; do
    [ -f "$f" ] && lossless_rt "$f"
done
for f in imgs-ppm/kodak/*.ppm.gz; do
    [ -f "$f" ] || continue
    gunzip -c "$f" > "$T/$(basename "${f%.gz}")" && lossless_rt "$T/$(basename "${f%.gz}")"
done
for f in imgs-raw16/*.raw; do
    [ -f "$f" ] || continue
    dims=$(echo "$f" | sed -E 's/.*-1x([0-9]+)x([0-9]+)\.raw/\1 \2/')
    set -- $dims
    lossless_rt "$f" -rows "$1" -cols "$2" -bpp 16 -be
    dd if="$f" conv=swab of="$T/le.raw" status=none
    lossless_rt "$T/le.raw" -rows "$1" -cols "$2" -bpp 16 -le
done

echo "== Lossless codec: special cases (tests/make_special_images.py)"
python3 tests/make_special_images.py "$T/special" >/dev/null
while IFS=$'\t' read -r name opts; do
    # shellcheck disable=SC2086
    lossless_rt "$T/special/$name" $opts
done < "$T/special/manifest.txt"

echo "== Lossless codec: speed profiles and stripes (-fast, -fastest, -t n)"
lossless_rt_opts() {  # lossless_rt_opts "encoder options" file
    local o=$1 f=$2
    # shellcheck disable=SC2086
    if $B/img_lossless_enc $o "$f" "$T/ll.c" 2>"$T/ll.err" && $B/img_lossless_dec "$T/ll.c" "$T/ll.d" 2>>"$T/ll.err" &&
        cmp -s "$f" "$T/ll.d"; then
        pass "[$o] $(basename "$f")"
    else
        bad "[$o] $(basename "$f"): $(head -c 200 "$T/ll.err")"
    fi
}
for f in imgs-pgm8/lena.pgm imgs-ppm/tulips.ppm imgs-ppm/arial.ppm; do
    [ -f "$f" ] || continue
    for o in "-fast" "-fastest" "-t 2" "-t 7" "-fast -t 3"; do lossless_rt_opts "$o" "$f"; done
done
for f in imgs-raw16/jkt*.raw; do
    [ -f "$f" ] && lossless_rt_opts "-t 4 -rows 1124 -cols 1124 -bpp 16" "$f"
done
for name in p5_1x1.pgm p5_row_1x50.pgm p5_col_50x1.pgm p6_16bit.ppm p5_maxval1000.pgm raw16be_trailing-1x30x45.raw; do
    opts=$(grep -P "^$name\t" "$T/special/manifest.txt" | cut -f2)
    lossless_rt_opts "-t 5 $opts" "$T/special/$name"
done
# the same file decoded with 1 thread and with several threads
if [ -f imgs-ppm/tulips.ppm ]; then
    $B/img_lossless_enc -t 4 imgs-ppm/tulips.ppm "$T/st.c"
    $B/img_lossless_dec -t 1 "$T/st.c" "$T/st1.ppm" && $B/img_lossless_dec -t 4 "$T/st.c" "$T/st4.ppm" &&
        cmp -s "$T/st1.ppm" "$T/st4.ppm" && cmp -s imgs-ppm/tulips.ppm "$T/st1.ppm" &&
        pass "4 stripes decoded with 1 and with 4 threads" || bad "4 stripes decoded with 1 and with 4 threads"
fi

echo "== Lossless codec: corrupted input must be rejected (no crash, no wrong output)"
$B/img_lossless_enc imgs-pgm8/lena.pgm "$T/ok.c" 2>/dev/null || $B/img_lossless_enc "$T/special/p5_basic.pgm" "$T/ok.c"
n=$(stat -c %s "$T/ok.c")
head -c $((n / 2)) "$T/ok.c" > "$T/trunc.c"
$B/img_lossless_dec "$T/trunc.c" "$T/x" 2>/dev/null; rc=$?
[ $rc -ne 0 ] && [ $rc -lt 128 ] && pass "truncated file rejected" || bad "truncated file (exit $rc)"
python3 -c "import sys; d=bytearray(open(sys.argv[1],'rb').read()); d[len(d)//2]^=0x55; open(sys.argv[2],'wb').write(d)" "$T/ok.c" "$T/flip.c"
$B/img_lossless_dec "$T/flip.c" "$T/x" 2>/dev/null; rc=$?
[ $rc -ne 0 ] && [ $rc -lt 128 ] && pass "corrupted byte rejected" || bad "corrupted byte (exit $rc)"
$B/img_lossless_dec imgs-pgm8/lena.pgm "$T/x" 2>/dev/null; rc=$?
[ $rc -ne 0 ] && [ $rc -lt 128 ] && pass "non-compressed input rejected" || bad "non-compressed input (exit $rc)"

[ $fail = 0 ] && echo "All tests passed." || echo "Some tests FAILED."
exit $fail
