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

[ $fail = 0 ] && echo "All tests passed." || echo "Some tests FAILED."
exit $fail
