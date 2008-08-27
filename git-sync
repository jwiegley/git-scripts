#!/bin/bash

DIRS="$@"
if [[ -z "$DIRS" ]]; then
    DIRS="."
fi

find "$DIRS" \( -name .git -o -name '*.git' \) -type d | \
    while read repo_dir
    do
	if [[ -f "$repo_dir"/config ]]
	then
	    # If this is a git-svn repo, use git svn fetch
	    if grep -q '^\[svn-remote ' "$repo_dir"/config
	    then
		echo git svn fetch: $repo_dir
		GIT_DIR="$repo_dir" git svn fetch

	    # If this is a gc-utils repo, use gc-utils update
	    elif grep -q '^\[gc-utils\]' "$repo_dir"/config
	    then
		echo gc-utils update: $repo_dir
		(cd "$repo_dir"; gc-utils update)

	    else
		for remote in $(GIT_DIR="$repo_dir" git remote)
		do
		    if [[ $remote != mirror ]]; then
			echo git fetch: $repo_dir -- $remote
			GIT_DIR="$repo_dir" git fetch $remote
		    fi
		done
	    fi

	    for remote in $(GIT_DIR="$repo_dir" git remote)
	    do
		if [[ $remote == mirror ]]; then
		    echo git push: $repo_dir -- $remote
		    GIT_DIR="$repo_dir" git push --mirror $remote
		fi
	    done
	fi
    done
