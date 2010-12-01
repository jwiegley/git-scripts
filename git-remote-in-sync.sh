#!/bin/bash
# checks if the specified remote(s) are in sync with what we have
# in other words: do we have anything which is not at the remote?
# Any commit, tag, branch, dirty WC/index or stashed state?
# This is especially useful if you're wondering:
# "is it safe to delete this clone? is any work here that needs to be
# distributed/shared/pushed first?"
# Note that there are some special cases, like a branch here may
# have a different name on the remote.  This script is not aware of
# stuff like that.  Also, `git status` will not detect if the local
# repo is ahead of an svn repo (using git svn)

# written by Dieter Plaetinck

list="$@"
[ -z "$list" ] && list=`git remote`
ret=0

if [ -z "`git branch`" ]
then
	if [ $(cd $(git root) && ls -Al | egrep -v "^(.git|total)" | wc -l) -gt 0 ]
	then
		echo "This repo doesn't contain any branch, but contains a bunch of files!" >&2
		ls -Alh $(git root)
		ret=1
	else
		echo "This repo doesn't contain any branch, and is empty"
		# note that stashing doesn't work without a branch, so the above check is sufficient
		exit 0
	fi
fi

if [ -z "$list" ]
then
	echo "At least one branch, but no remotes found!  The content here might be unique!" >&2
	ret=1
fi

for remote in $list;
do
	# Check commits and branches
	# An approach where you `git push --all -n $remote 2>&1` and check whether the result is
	# 'Everything up-to-date' can (and often will) yield
	# "To prevent you from losing history, non-fast-forward updates were rejected"
	# Since such an approach does traffic with the remote anyway, I choose to fetch the origin
	# and inspect it's branches.  This is a bit inefficient network-wise, but couldn't see a better solution
	if ! git fetch $remote
	then
		echo "could not git fetch $remote" >&2
		ret=1
	fi
	IFS_BACKUP=$IFS
	IFS=$'\n'
	for branch in `git branch`
	do
		branch=${branch/\*/};
		branch_local=${branch// /}; # git branch prefixes branches with spaces. and spaces in branchnames are illegal.
		if ! git branch -r --contains $branch_local 2>/dev/null | grep -q "^  $remote/"
		then
			echo "Branch $branch_local is not contained within remote $remote!" >&2
			ret=1
		fi
		if ! git branch -r | grep -q "^  $remote/$branch_local"
		then
			echo "Branch $branch_local exists, but not $remote/$branch_local" >&2
			ret=1
		fi
	done
	IFS=$IFS_BACKUP

	# Check tags
	out=`git push --tags -n $remote 2>&1`
	if [ "$out" != 'Everything up-to-date' ];
	then
		echo -e "***** Dirty tags: *****\n$out" >&2
		ret=1
	fi
done

# Check WC/index
cur_branch=`git branch | grep '^\* ' | cut -d ' ' -f2`
cur_branch=${cur_branch// /}
exp="# On branch $cur_branch
nothing to commit (working directory clean)"
out=`git status`
if [ "$out" != "$exp" ]
then
	# usually i'd use bash regex, but this case is multiple-lines so bash regex is no go
	out=$(echo "$out" | egrep -v "^(# On branch $cur_branch|# Your branch is behind .* commits, and can be fast-forwarded.|#|nothing to commit \(working directory clean\))$")
	if [ -n "$out" ]
	then
		echo "***** Dirty WC or index *****" >&2
		git status
		ret=1
	fi
fi

# Check stash
if [ `git stash list | wc -l` -gt 0 ];
then
	echo "***** Dirty stash: *****" >&2
	GIT_PAGER= git stash list
	ret=1
fi

exit $ret
