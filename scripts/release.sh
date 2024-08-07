#!/bin/bash

set -euo pipefail

usage="$(basename "$0") [-h] [-t,--tag] VERSION

Helper script to create a source and binary rpm of the project.

Options:
-h,--help     show this help text
-t,--tag      run git tag
"

# A temporary variable to hold the output of `getopt`
TEMP=$(getopt -o t --long tag: -- "$@")

# If getopt has reported an error, exit script with an error
if [ $? != 0 ]; then
	# echo 'Error parsing options' >&2
	echo "${usage}" >&2
	exit 1
fi

if ! git diff-index --quiet --cached HEAD; then
    echo "Error: Uncommited changes found." >&2
    exit 1
fi

eval set -- "$TEMP"

_tag=0
# Now go through all the options
while true; do
	case "$1" in
	-t | --tag)
		_tag=1
		shift
		;;
	-h | --help)
		shift
		echo "${usage}"
		exit 1
		;;
	--)
		shift
		break
		;;
	*)
		echo "Internal error! $1"
		exit 1
		;;
	esac
done

if [ $# -ne 1 ]; then
	  echo "Usage: $0 <new-version>"
    exit 1
fi

# Check if a version argument is provided
if [ -z "$1" ]; then
	echo "Usage: $0 <new-version>"
	exit 1
fi

set -x

_toplevel=$(git rev-parse --show-toplevel)

# New version tag
NEW_VERSION=$1

# Path to the manpage file
MANPAGE_PATH="doc/squashfs-mount.1"

# Get the current month and year
CURRENT_DATE=$(date "+%B %Y")

# Use sed to update the date in the manpage
sed -e "s/@DATE@/${CURRENT_DATE}/" \
	-e "s/@VERSION@/${NEW_VERSION}/" \
  -e '1i\\\" -- This is a generated file. DO NOT EDIT --' \
	"${_toplevel}/${MANPAGE_PATH}.in" > "${_toplevel}/${MANPAGE_PATH}"
echo ${NEW_VERSION} > ${_toplevel}/VERSION

# Add and commit the updated manpage
git add "${_toplevel}/$MANPAGE_PATH"
git add ${_toplevel}/VERSION
git commit -m "Release ${NEW_VERSION}"

# Tag the new release
if [ "$_tag" -eq "1" ]; then
    git tag -a "$NEW_VERSION" -m "Release version $NEW_VERSION"
fi
