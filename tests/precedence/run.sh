#!/bin/bash
# run from anywhere after "make": tests/precedence/run.sh

cd "$(dirname "$0")/../.." || exit 1

pass=0
fail=0
use_vg=0
command -v valgrind >/dev/null && use_vg=1

for f in tests/precedence/*.ifj; do
    name=$(basename "$f")
    case "$name" in
        prog*) expected=0 ;;
        err*)  expected=2 ;;
        *)     continue ;;
    esac

    if [ $use_vg -eq 1 ]; then
        valgrind -q --leak-check=full --log-file=vg.log ./ifj "$f" >/dev/null 2>&1
        code=$?
        mem=$(cat vg.log)
        rm -f vg.log
    else
        ./ifj "$f" >/dev/null 2>&1
        code=$?
        mem=""
    fi

    if [ "$code" -eq "$expected" ] && [ -z "$mem" ]; then
        echo "OK    $name"
        pass=$((pass + 1))
    else
        echo "FAIL  $name (exit $code, expected $expected)"
        [ -n "$mem" ] && echo "$mem"
        fail=$((fail + 1))
    fi
done

echo "passed: $pass, failed: $fail"
[ $fail -eq 0 ]