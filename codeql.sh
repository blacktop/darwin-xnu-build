#!/usr/bin/env bash

# CREDIT: https://github.com/pwn0rz/xnu-build

set -o errexit
set -o nounset
set -o pipefail
if [[ "${TRACE-0}" == "1" ]]; then
    set -o xtrace
fi

# Help
if [[ "${1-}" =~ ^-*h(elp)?$ ]]; then
    echo 'Usage: codeql.sh

This script creates the macOS 13.2 xnu kernel codeql database

'
    exit
fi

# Colors
export ESC_SEQ="\x1b["
export COL_RESET=$ESC_SEQ"39;49;00m"
export COL_RED=$ESC_SEQ"31;01m"
export COL_GREEN=$ESC_SEQ"32;01m"
export COL_YELLOW=$ESC_SEQ"33;01m"
export COL_BLUE=$ESC_SEQ"34;01m"
export COL_MAGENTA=$ESC_SEQ"35;01m"
export COL_CYAN=$ESC_SEQ"36;01m"

function running() {
    echo -e "$COL_MAGENTA ⇒ $COL_RESET"$1
}

function info() {
    echo -e "$COL_BLUE[info]$COL_RESET" $1
}

: ${KERNEL_CONFIG:=RELEASE}
: ${ARCH_CONFIG:=ARM64}
: ${MACHINE_CONFIG:=VMAPPLE}

function install_codeql() {
    if ! [ -x "$(command -v codeql)" ]; then
        running "Installing CodeQL..."
        brew install codeql
    fi
}

function create_db() {
    WORK_DIR="$PWD"
    BUILD_DIR=${WORK_DIR}/build
    FAKEROOT_DIR=${WORK_DIR}/fakeroot
    DATABASE_DIR=${WORK_DIR}/xnu-codeql
    rm -rf ${BUILD_DIR}
    rm -rf ${FAKEROOT_DIR}
    rm -rf ${DATABASE_DIR}
    running "📦 Creating the CodeQL database..."
    KERNEL_CONFIG=${KERNEL_CONFIG} ARCH_CONFIG=${ARCH_CONFIG} MACHINE_CONFIG=${MACHINE_CONFIG} codeql database create ${DATABASE_DIR} --language=cpp -v --command=${WORK_DIR}/build.sh --source-root=${WORK_DIR}
    info "Deleting log files..."
    rm -rf ${DATABASE_DIR}/log
    info "Zipping the CodeQL database..."
    zip -r -X xnu-codeql.zip xnu-codeql/*
}

main() {
    install_codeql
    create_db
    echo "  🎉 CodeQL Database Create Done!"
}

main "$@"