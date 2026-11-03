#!/bin/bash
# Syntakticke testy, spustenie odkialkolvek po "make": tests/syntax/run.sh
# Ocakavany vysledok je v hlavicke kazdeho testu: "Ocakavany vysledok: kod N".
#   kod 0 - syntakticky platny program: test prejde, ak prekladac nevrati
#           chybu 1 (lexikalna), 2 (syntakticka) ani 99 (interna); semanticke
#           chyby (3-10) syntaktickej analyze nevadia
#   kod 2 - syntakticka chyba: prekladac musi vratit presne 2

cd "$(dirname "$0")/../.." || exit 1

pass=0
fail=0
use_vg=0
command -v valgrind >/dev/null && use_vg=1

for f in tests/syntax/*.scala; do
    name=$(basename "$f")
    expected=$(sed -n 's/.*Ocakavany vysledok: kod \([0-9]*\).*/\1/p' "$f" | head -n 1)
    if [ -z "$expected" ]; then
        echo "SKIP  $name (chyba hlavicka s ocakavanym vysledkom)"
        continue
    fi

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

    if [ "$expected" -eq 0 ]; then
        case "$code" in
            1|2|99) ok=0 ;;
            *) [ "$code" -lt 128 ] && ok=1 || ok=0 ;;  # >= 128 = pad (signal)
        esac
    else
        [ "$code" -eq "$expected" ] && ok=1 || ok=0
    fi

    if [ $ok -eq 1 ] && [ -z "$mem" ]; then
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
