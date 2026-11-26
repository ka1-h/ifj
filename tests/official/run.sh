#!/bin/bash
# Compiles each tests/official/<dir>/*.scala, runs it in ic26int with the .in file and compares the output with the .out file.
# Usage (after make): tests/official/run.sh [basic|extensions]

cd "$(dirname "$0")/../.." || exit 1
dir="tests/official/${1:-basic}"
tmp=$(mktemp -d)
pass=0
fail=0

for src in "$dir"/*.scala; do
    name=$(basename "$src" .scala)
    ./ifj <"$src" >"$tmp/$name.code" 2>/dev/null
    code=$?
    if [ $code -ne 0 ]; then
        echo "FAIL  $name (compiler exit $code)"
        fail=$((fail + 1))
        continue
    fi
    ic26int "$tmp/$name.code" <"$dir/$name.in" >"$tmp/$name.res" 2>"$tmp/$name.err"
    code=$?
    if [ $code -eq 0 ] && diff -q "$tmp/$name.res" "$dir/$name.out" >/dev/null; then
        echo "OK    $name"
        pass=$((pass + 1))
    else
        echo "FAIL  $name (interpreter exit $code)"
        head -3 "$tmp/$name.err"
        diff "$tmp/$name.res" "$dir/$name.out" | head -10
        fail=$((fail + 1))
    fi
done

rm -rf "$tmp"
echo "passed: $pass, failed: $fail"