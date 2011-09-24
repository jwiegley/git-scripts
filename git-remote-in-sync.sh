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
		echo "WARN: This repo doesn't contain any branch, but contains a bunch of files!" >&2
		ls -Alh $(git root)
		ret=1
	else
		echo "INFO: This repo doesn't contain any branch, and is empty"
		# note that stashing doesn't work without a branch, so the above check is sufficient
		exit 0
	fi
else
	echo -n "INFO: Valid git repo with branches: "
	git branch | tr '\n' ' '
	echo
fi

if [ -z "$list" ]
then
	echo "WARN: At least one branch, but no remotes found!  The content here might be unique!" >&2
	ret=1
else
	if [ -n "$@" ]
	then
		echo "INFO: working with remote(s): $@"
		echo "INFO: fyi, all remotes: "`git remote`
	else
		echo "INFO: working with all remotes: $list"
	fi
fi

for remote in $list;
do
	echo "INFO: Checking remote $remote.."
	# Check commits and branches
	# An approach where you `git push --all -n $remote 2>&1` and check whether the result is
	# 'Everything up-to-date' can (and often will) yield
	# "To prevent you from losing history, non-fast-forward updates were rejected"
	# Since such an approach does traffic with the remote anyway, I choose to fetch the origin
	# and inspect it's branches.  This is a bit inefficient network-wise, but couldn't see a better solution
	if ! git fetch $remote
	then
		echo "  WARN: could not git fetch $remote" >&2
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
			echo "  WARN: Branch $branch_local is not contained within remote $remote!" >&2
			ret=1
		else
			echo "  INFO: Branch $branch_local is contained within remote $remote"
		fi
		if ! git branch -r | grep -q "^  $remote/$branch_local"
		then
			echo "  WARN: Branch $branch_local exists, but not $remote/$branch_local" >&2
			ret=1
		else
			echo "  INFO: Branch $branch_local exists also as $remote/$branch_local"
		fi
	done
	IFS=$IFS_BACKUP

	# Check tags
	out=`git push --tags -n $remote 2>&1`
	if [ "$out" != 'Everything up-to-date' ];
	then
		echo -e "  WARN: Some tags are not in $remote!\n$out" >&2
		ret=1
	else
		echo "  INFO: All tags ("`git tag`") exist at $remote as well"
	fi
done

# Check WC/index
cur_branch=`git branch | grep '^\* ' | cut -d ' ' -f2`
cur_branch=${cur_branch// /}
exp="# On branch $cur_branch
nothing to commit (working directory clean)"
out=`git status`
wc_ok=1
if [ "$out" != "$exp" ]
then
	# usually i'd use bash regex, but this case is multiple-lines so bash regex is no go
	out=$(echo "$out" | egrep -v "^(# On branch $cur_branch|# Your branch is behind .* commits, and can be fast-forwarded.|#|nothing to commit \(working directory clean\))$")
	if [ -n "$out" ]
	then
		wc_ok=0
		echo "WARN: Dirty WC or index" >&2
		git status
		ret=1
	fi
fi
if [ $wc_ok -eq 1 ]
then
	echo "INFO: Working copy/index are clean"
	git status
fi

# Check stash
if [ `git stash list | wc -l` -gt 0 ];
then
	echo "WARN: Dirty stash:" >&2
	GIT_PAGER= git stash list >&2
	ret=1
else
	echo "INFO: Stash clean"
	GIT_PAGER= git stash list
fi
exit $ret
