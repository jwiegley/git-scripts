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
	# check commits and branches
	out=`git push --all -n $remote 2>&1`
	if [ "$out" != 'Everything up-to-date' ];
	then
		echo -e "***** Dirty commits/branches: *****\n$out" >&2
		ret=1
	fi

	# check tags
	out=`git push --tags -n $remote 2>&1`
	if [ "$out" != 'Everything up-to-date' ];
	then
		echo -e "***** Dirty tags: *****\n$out" >&2
		ret=1
	fi
done

# added/deleted/modified files in the currently checked out branch or index.
# this should do I think. (other branch, esp remote tracking branch should not be checked, right? maybe if this one is dirty..)
cur_branch=`git branch | cut -d ' ' -f2`
exp="# On branch $cur_branch
nothing to commit (working directory clean)"
out=`git status`
if [ "$out" != "$exp" ];
then
	echo "***** Dirty WC or index *****" >&2
	git status
	ret=1
fi

# stash
if [ `git stash list | wc -l` -gt 0 ];
then
	echo "***** Dirty stash: *****" >&2
	GIT_PAGER= git stash list
	ret=1
fi

exit $ret
