#!/usr/bin/env bash

set -o errexit
set -o nounset
set -o pipefail
if [[ "${TRACE-0}" == "1" ]]; then
    set -o xtrace
fi

SRC="$(dirname "$0")"

# Config
: ${FORCE_WRITE:=0}
# : ${VERSIONS_PATH:="$SRC/ci-versions.json"}
VERSIONS_PATH=${VERSIONS_PATH:-"$SRC/ci-versions.json"}

help() {
    echo 'Usage: workflows-from-version.sh [-f]

This script generates workflows for each xnu versio specified in ci-versions.json

Where:
    -f|--force           will overwrite
    -v|--versions-path   path to versions spec json file
'
    exit 0
}

workflows_from_ci_versions() {
    # Read versions from the JSON file and iterate
    jq -c '.[]' "$VERSIONS_PATH" | while IFS='' read -r config; do

      export MACHINE_CONFIG=$(echo "$config" | jq -r '.["machine-config"]')
      export XCODE_VERSION=$(echo "$config" | jq -r '.["xcode-version"]')
      export MACOS_VERSION=$(echo "$config" | jq -r '.["macos-version"]')

      file="$SRC/workflows/xnu-${MACOS_VERSION}.yml"

      if ! [ -f "$file" ] || [ "$FORCE_WRITE" -ne "0" ]; then
        # Using envsubst to replace variables within the template
        envsubst < "$SRC/xnu-{version}.yml" > "$file"
      fi
    done
}

main() {
    # Parse arguments
    while test $# -gt 0; do
        case "$1" in
        -h | --help)
            help
            ;;
        -f | --force)
            FORCE_WRITE=1
            shift
            ;;
        -v|--versions-path)
            if [ "$1" = "-v" ]; then
                # For -v option, the value is the next argument
                if [ $# -ge 2 ]; then
                    VERSIONS_PATH="$2"
                    shift 2
                else
                    echo "Error: -v requires a path."
                    exit 1
                fi
            elif [[ "$1" =~ ^--versions-path= ]]; then
                # For --versions-path=value option, split the value from the option
                VERSIONS_PATH="${1#*=}"
                shift
            else
                # For --versions-path value option
                if [ $# -ge 2 ]; then
                    VERSIONS_PATH="$2"
                    shift 2
                else
                    echo "Error: --versions-path requires a path."
                    exit 1
                fi
            fi
        ;;
        *)
            break
            ;;
        esac
    done
    workflows_from_ci_versions
}

main "$@"
