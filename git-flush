#!/bin/sh

# The job of git-flush is to recompactify your repository to be as small
# as possible, by dropping all reflogs, stashes, and other cruft that may
# be bloating your pack files.

rm -fr .git/refs/original
perl -i -ne 'print unless /refs\/original/;' .git/info/refs .git/packed-refs

git reflog expire --expire=0 --all

if [ "$1" = "-f" ]; then
    git stash clear
fi

git repack -adf #--window=200 --depth=50
git prune
