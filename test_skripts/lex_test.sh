#!/usr/bin/env bash
# Skript na verifikáciu tokenov pre lexer IFJ26

set -u

TEST_DIR="../tests"
TOKENS_DIR="../tests/tokens"
PROJECT_BIN="../ifj"

# Farby pre výstup
if [[ -t 1 ]]; then
    GREEN=$'\033[32m'
    RED=$'\033[31m'
    YELLOW=$'\033[33m'
    BOLD=$'\033[1m'
    RESET=$'\033[0m'
else
    GREEN=""
    RED=""
    YELLOW=""
    BOLD=""
    RESET=""
fi

if [[ ! -x "${PROJECT_BIN}" ]]; then
    echo "Binárka ${PROJECT_BIN} nebola nájdená. Spusti najprv 'make'." >&2
    exit 1
fi

total=0
passed=0
failed=0
skipped=0

printf "${BOLD}Spúšťam kontrolu tokenov v %s (očakávané v %s)${RESET}\n\n" "${TEST_DIR}" "${TOKENS_DIR}"

shopt -s nullglob
for file in "${TEST_DIR}"/test*.txt; do
    ((total++))
    base="$(basename "${file}")"
    
    # Priradenie napr. test1.txt -> tests/tokens/output1.txt
    num="${base//[^0-9]/}"
    output_file="${TOKENS_DIR}/output${num}.txt"

    # Ak neexistuje zodpovedajúci outputX.txt v zložke tokens, test sa preskočí
    if [[ ! -f "${output_file}" ]]; then
        printf "${YELLOW}[SKIP]${RESET} %-20s (chýba tokens/output%s.txt)\n" "${base}" "${num}"
        ((skipped++))
        continue
    fi

    # Uloženie aktuálne vygenerovaných tokenov do tmp súboru
    tmp_output=$(mktemp)
    "${PROJECT_BIN}" < "${file}" > "${tmp_output}" 2>&1
    exit_code=$?

    # Porovnanie vygenerovaných tokenov s očakávanými v tokens/outputX.txt
    if [[ ${exit_code} -eq 0 ]] && diff -u -b "${output_file}" "${tmp_output}" > /dev/null 2>&1; then
        printf "${GREEN}[PASS]${RESET} %-20s Tokeny sa presne zhodujú\n" "${base}"
        ((passed++))
    else
        printf "${RED}[FAIL]${RESET} %-20s Tokeny sa NEZHODUJÚ s tokens/output%s.txt\n" "${base}" "${num}"
        
        echo "   --- Rozdiely (Očakávané v tokens/output${num}.txt vs Vygenerované) ---"
        diff -u -b "${output_file}" "${tmp_output}" | head -n 12 | sed 's/^/   /'
        echo "   ------------------------------------------------------------"
        ((failed++))
    fi

    rm -f "${tmp_output}"
done
shopt -u nullglob

printf "\nZhrnutie: %d celkovo | ${GREEN}%d prešlo${RESET} | ${RED}%d zlyhalo${RESET} | ${YELLOW}%d preskočené${RESET}\n" \
    "${total}" "${passed}" "${failed}" "${skipped}"

(( failed == 0 )) || exit 1
exit 0