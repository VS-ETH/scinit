#!/bin/bash

echo "Checking code format"
if [ -n $(git diff --name-only | grep -E "^(src|test).*(.cpp$|.h)$") ] ; then
    echo "#####################################################################"
    echo "### ERROR: Inconsistent formatting, please fix files and resubmit ###"
    echo "#####################################################################"
    git diff --name-only | grep -E "^(src|test).*(.cpp$|.h)$" | xargs git diff
    return 255
fi