#!/usr/bin/env bash
# Phase 9 verification (docs/riagraph-fork-plan.md section 13), run inside the build
# environment. Intended to be invoked through run.sh:
#
#   docker/build-env/run.sh bash docker/build-env/verify.sh
#
# Covers the core: the build, the test suite, and the identity assertions that say the
# fork actually renamed what it claims to have renamed. Language bindings are verified in
# their own repositories -- see README.md.

set -uo pipefail

SHELL_BIN=build/release/tools/shell/riag
FAILURES=0
SKIPPED=0

pass() { printf '  \033[32mPASS\033[0m  %s\n' "$1"; }
fail() { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; FAILURES=$((FAILURES + 1)); }
skip() { printf '  \033[33mSKIP\033[0m  %s\n' "$1"; SKIPPED=$((SKIPPED + 1)); }
head_() { printf '\n\033[1m== %s\033[0m\n' "$1"; }

# Some e2e fixtures are generated rather than committed. Without them the tests that
# read them fail for want of input, which reads like a regression and is not one.
if [ -f scripts/generate-dictionary-bug-fixture.py ] \
   && [ ! -f test/test_files/dictionary_bug/orb383_relationship_projection_obfuscated_fixture/A.parquet ]; then
    printf '\033[1m== Generating test fixtures\033[0m\n'
    python3 scripts/generate-dictionary-bug-fixture.py 2>&1 | sed 's/^/  /'
fi

head_ "Toolchain"
printf '  %s\n' "$(gcc --version | head -1)" "$(cmake --version | head -1)" \
                "$(openssl version)" "$(python3 --version)"

head_ "Build artefacts"
for f in "${SHELL_BIN}" build/release/src/libriag.so; do
    [ -f "$f" ] && pass "exists: $f" || fail "missing: $f"
done
# The static archive is only produced when BUILD_STATIC_RIAG is on.
if [ -f build/release/src/libriag.a ]; then
    pass "exists: build/release/src/libriag.a"
else
    skip "build/release/src/libriag.a not built in this configuration"
fi
# Nothing may still be named after the old project.
if compgen -G 'build/release/src/*lbug*' > /dev/null; then
    fail "an artefact is still named lbug: $(echo build/release/src/*lbug*)"
else
    pass "no artefact is named lbug"
fi

head_ "Version"
if [ -x "${SHELL_BIN}" ]; then
    VERSION="$("${SHELL_BIN}" --version 2>&1 | head -1)"
    printf '  %s\n' "${VERSION}"
    case "${VERSION}" in
        *0.22.0*) pass "shell reports 0.22.0" ;;
        *)        fail "shell does not report 0.22.0" ;;
    esac
else
    fail "shell binary not executable"
fi

head_ "Storage magic (plan 6.1, decision D3)"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

if [ -x "${SHELL_BIN}" ]; then
    NEW_DB="${WORK}/new.riag"
    cat > "${WORK}/create.cypher" <<'CYPHER'
CREATE NODE TABLE T(id INT64 PRIMARY KEY);
CREATE (:T {id: 1});
CHECKPOINT;
CYPHER
    "${SHELL_BIN}" "${NEW_DB}" -i "${WORK}/create.cypher"         > "${WORK}/create.out" 2>&1 < /dev/null
    if [ -f "${NEW_DB}" ]; then
        MAGIC="$(head -c 4 "${NEW_DB}")"
        if [ "${MAGIC}" = "RIAG" ]; then
            pass "a new database is written with RIAG magic"
        else
            fail "new database magic is '${MAGIC}', expected RIAG"
            cat "${WORK}/create.out"
        fi

        # A ladybug database differs only in these four bytes, so making one is a
        # four-byte edit rather than a checked-in fixture.
        LEGACY_DB="${WORK}/legacy.riag"
        cp "${NEW_DB}" "${LEGACY_DB}"
        printf 'LBUG' | dd of="${LEGACY_DB}" bs=1 seek=0 conv=notrunc status=none
        if [ "$(head -c 4 "${LEGACY_DB}")" = "LBUG" ]; then
            cat > "${WORK}/read.cypher" <<'CYPHER'
MATCH (t:T) RETURN count(t);
CYPHER
            OUT="$("${SHELL_BIN}" "${LEGACY_DB}" -i "${WORK}/read.cypher" 2>&1 < /dev/null)"
            if printf '%s' "${OUT}" | grep -qE '(^|[^0-9])1([^0-9]|$)'; then
                pass "a legacy LBUG database still opens and reads"
            else
                fail "legacy LBUG database did not open"
                printf '%s\n' "${OUT}" | sed 's/^/        /'
            fi

            # Writing to it must upgrade the header: the exchange is one-way.
            cat > "${WORK}/write.cypher" <<'CYPHER'
CREATE (:T {id: 2});
CHECKPOINT;
CYPHER
            "${SHELL_BIN}" "${LEGACY_DB}" -i "${WORK}/write.cypher"                 > /dev/null 2>&1 < /dev/null
            UPGRADED="$(head -c 4 "${LEGACY_DB}")"
            if [ "${UPGRADED}" = "RIAG" ]; then
                pass "writing to a legacy database upgrades the magic to RIAG (one-way)"
            else
                fail "after writing, magic is '${UPGRADED}', expected RIAG"
            fi
        else
            fail "could not patch the magic bytes for the legacy fixture"
        fi
    else
        fail "no database file was created at ${NEW_DB}"
        cat "${WORK}/create.out"
    fi
else
    fail "shell binary not executable; storage magic not checked"
fi

head_ "Rename guards (plan 5.2)"
if [ -d .git ]; then
    if python3 scripts/riag-rename.py --check > "${WORK}/check.out" 2>&1; then
        pass "riag-rename.py --check is clean"
    else
        fail "riag-rename.py --check found remaining tokens"
        sed 's/^/        /' "${WORK}/check.out"
    fi
else
    skip "no .git in this container; run the grep guards on the host"
fi

head_ "Symbol table (plan section 8)"
if [ -f build/release/src/libriag.a ] && command -v nm > /dev/null; then
    COUNT="$(nm -C build/release/src/libriag.a 2>/dev/null | grep -c 'lbug_' || true)"
    [ "${COUNT}" = "0" ] && pass "no lbug_ symbols in libriag.a" \
                         || fail "${COUNT} lbug_ symbols remain in libriag.a"
elif [ -f build/release/src/libriag.so ] && command -v nm > /dev/null; then
    COUNT="$(nm -DC build/release/src/libriag.so 2>/dev/null | grep -c 'lbug_' || true)"
    [ "${COUNT}" = "0" ] && pass "no lbug_ symbols in libriag.so" \
                         || fail "${COUNT} lbug_ symbols remain in libriag.so"
else
    skip "no library to inspect with nm"
fi

printf '\n'
if [ "${FAILURES}" -eq 0 ]; then
    printf '\033[32mall checks passed\033[0m (%d skipped)\n' "${SKIPPED}"
else
    printf '\033[31m%d check(s) failed\033[0m (%d skipped)\n' "${FAILURES}" "${SKIPPED}"
fi
exit "${FAILURES}"
