#!/bin/bash
set -e
shopt -s nullglob extglob

cd "`git rev-parse --git-path objects`"

# packed objects
for p in pack/pack-*([0-9a-f]).idx ; do
    git show-index < $p | cut -f 2 -d ' '
done

# loose objects
for o in [0-9a-f][0-9a-f]/*([0-9a-f]) ; do
    echo ${o/\/}
done
