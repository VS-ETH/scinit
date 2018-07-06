#!/bin/bash

echo "Checking code format"
CHANGED_FILES="$(git diff --name-only | grep -E '^(src|test).*(.cpp$|.h)$')"
if [ -n "${CHANGED_FILES}" ] ; then
    echo "#####################################################################"
    echo "### ERROR: Inconsistent formatting, please fix files and resubmit ###"
    echo "#####################################################################"
    for i in ${CHANGED_FILES} ; do
        git diff "${i}"
    done
    exit 255
fi