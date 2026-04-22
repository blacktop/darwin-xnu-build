#!/bin/bash

choose_xnu() {
    gum choose "26.3" "26.4" \
    case ${MACOS_VERSION} in
        '26.3')
            RELEASE_URL='https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-263/release.json'
            KDK_NAME='Kernel Debug Kit 26.3 build 25D246'
            KDKROOT='/Library/Developer/KDKs/KDK_26.3_25D246.kdk/'
            RC_DARWIN_KERNEL_VERSION='25.3.0'
            ;; 
        '26.4')
            RELEASE_URL='https://raw.githubusercontent.com/apple-oss-distributions/distribution-macOS/macos-264/release.json'
            KDK_NAME='Kernel Debug Kit 26.4 build 25E246'
            KDKROOT='/Library/Developer/KDKs/KDK_26.4_25E246.kdk/'
            RC_DARWIN_KERNEL_VERSION='25.4.0'
            ;; 
        *)
            echo "Unsupported macOS version!"
            exit 1
            ;; 
    esac
}
