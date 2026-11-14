#!/usr/bin/env bash

#find project directory
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="${PROJECT_DIR}/tests/semantics"
PROJECT_BIN="${PROJECT_DIR}/ifj"

if [[ ! -x "${PROJECT_BIN}" ]]; then
    echo "Binary not found. Run make first." >&2
    exit 1
fi

passed=0
failed=0

shopt -s nullglob
files=("${TEST_DIR}"/*.scala)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "No tests found." >&2
    exit 1
fi

for file in "${files[@]}"; do
    name="$(basename "${file}")"
    expected_file="${file%.scala}.rc"

    if [[ ! -f "${expected_file}" ]]; then
        echo "[FAIL] ${name}: missing .rc file"
        ((failed+=1))
        continue
    fi

    expected="$(cat "${expected_file}")"

    #check expected return code
    if [[ ! "${expected}" =~ ^[0-9]+$ ]]; then
        echo "[FAIL] ${name}: invalid expected code"
        ((failed+=1))
        continue
    fi

    #run compiler and save return code
    "${PROJECT_BIN}" < "${file}" > /dev/null 2>&1
    actual=$?

    if [[ "${actual}" -eq "${expected}" ]]; then
        echo "[PASS] ${name}"
        ((passed+=1))
    else
        echo "[FAIL] ${name}: expected ${expected}, got ${actual}"
        ((failed+=1))
    fi
done

echo
echo "Passed: ${passed} | Failed: ${failed}"

if [[ "${failed}" -ne 0 ]]; then
    exit 1
fi

exit 0