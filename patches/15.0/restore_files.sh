#!/bin/bash

# 1: The tag to restore from
# 2: The path to restore from
restore_files_from_tag() {
	# Search for all files that were deleted at this tag in the given path,
	# and restore them in the current directory. This doesn't touch
	# files in the same directory that are still present.

	local TAG="${1}"
	local DIR="${2}"

	for file in $(git diff --name-only --diff-filter=D "${TAG}"..HEAD -- "${DIR}"); do
		echo "Checking out ${file}"
		git checkout "${TAG}" "${file}"
	done
}

restore_files_from_tag xnu-8796.101.5 EXTERNAL_HEADERS/TrustCache
restore_files_from_tag xnu-10063.121.3 iokit/DriverKit
