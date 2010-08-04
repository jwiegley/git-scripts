#!/bin/bash
# checks if the specified remote(s) are in sync with what we have
# in other words: do we have anything which is not at the remote?
# Any A commit, tag, branch, dirty WC/index or stashed state?
# written by Dieter Plaetinck

list="$@"
[ -z "$list" ] && list=`git remote`
ret=0

for remote in $list;
do
	# check commits and branches
	out=`git push --all -n $remote 2>&1`
	if [ "$out" != 'Everything up-to-date' ];
	then
		echo -e "***** Dirty commits/branches: *****\n$out"
		ret=1
	fi

	# check tags
	out=`git push --tags -n $remote 2>&1`
	if [ "$out" != 'Everything up-to-date' ];
	then
		echo -e "***** Dirty tags: *****\n$out"
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
	echo "***** Dirty WC or index *****"
	git status
	ret=1
fi

# stash
if [ `git stash list | wc -l` -gt 0 ];
then
	echo "***** Dirty stash: *****"
	git stash list
	ret=1
fi

exit $ret
