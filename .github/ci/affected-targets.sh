#!/usr/bin/env bash
#
# Read changed file paths on stdin (one per line). Emit to stdout either:
#     full
# when the whole suite must run (build-system / cross-cutting / multi-module / unrecognized
# change), or
#     <space-separated ninja targets>|<ctest -R regex>
# for a single affected module.
#
# Kept intentionally conservative: anything not confidently isolated to one module falls back to
# "full" so we never skip coverage a change actually needs. No associative arrays, so it runs on
# the ancient bash on macOS as well as the CI container.
set -euo pipefail

# Map one path to a module tag. "BROAD" forces a full run; "" (unmapped) also forces full.
module_of() {
    case "$1" in
        CMakeLists.txt|*/CMakeLists.txt|*.cmake|cmake/*|.github/*|.gitlab*|vcpkg*|tools/*|thirdparty/*|resources/*) echo BROAD ;;
        libs/core/*|include/core/*|libs/kiplatform/*|qa/mocks/*|qa/qa_utils/*)             echo BROAD ;;
        common/*|include/*|qa/tests/common/*)  echo common ;;
        pcbnew/*|qa/tests/pcbnew/*)            echo pcbnew ;;
        eeschema/*|qa/tests/eeschema/*)        echo eeschema ;;
        libs/kimath/*|qa/tests/libs/kimath/*)  echo kimath ;;
        libs/sexpr/*|qa/tests/libs/sexpr/*)    echo sexpr ;;
        libs/kinng/*)                          echo kinng ;;
        gerbview/*|qa/tests/gerbview/*)        echo gerbview ;;
        api/*|qa/tests/api/*)                  echo api ;;
        *) echo "" ;;
    esac
}

# Map a module tag to "<ninja targets>|<ctest regex>".
targets_for() {
    case "$1" in
        common)   echo "qa_common|^qa_common$" ;;
        pcbnew)   echo "qa_pcbnew|^qa_pcbnew" ;;
        eeschema) echo "qa_eeschema qa_spice|^qa_(eeschema|spice)" ;;
        kimath)   echo "qa_kimath|^qa_kimath$" ;;
        sexpr)    echo "qa_sexpr|^qa_sexpr$" ;;
        kinng)    echo "qa_kinng|^qa_kinng$" ;;
        gerbview) echo "qa_gerbview|^qa_gerbview$" ;;
        api)      echo "qa_api|^qa_api$" ;;
    esac
}

mods=""
seen_any=0
while IFS= read -r f; do
    [ -z "$f" ] && continue
    seen_any=1
    m=$(module_of "$f")
    if [ "$m" = "BROAD" ] || [ -z "$m" ]; then
        echo full; exit 0
    fi
    mods="${mods}${m}
"
done

[ "$seen_any" = 0 ] && { echo full; exit 0; }

uniq_mods=$(printf '%s' "$mods" | grep . | sort -u)
count=$(printf '%s\n' "$uniq_mods" | grep -c . || true)
if [ "$count" -ne 1 ]; then
    echo full; exit 0
fi

targets_for "$uniq_mods"
