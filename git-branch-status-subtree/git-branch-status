#!/bin/bash

#  git-branch-status - print pretty git branch sync status reports
#
#  Copyright 2011      Jehiah Czebotar     <https://github.com/jehiah>
#  Copyright 2013      Fredrik Strandin    <https://github.com/kd35a>
#  Copyright 2014      Kristijan Novoselić <https://github.com/knovoselic>
#  Copyright 2014-2017 bill-auger          <https://github.com/bill-auger>
#
#  git-branch-status is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 3
#  as published by the Free Software Foundation.
#
#  git-branch-status is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License version 3
#  along with git-branch-status.  If not, see <http://www.gnu.org/licenses/>.

#  credits:
#   * original `git rev-list` grepping - by Jehiah Czebotar
#   * "s'all good!" message            - by Fredrik Strandin
#   * ANSI colors                      - by Kristijan Novoselić
#   * various features and maintenance - by bill-auger

#  please direct comments, bug reports, feature requests, or PRs to one of the upstream repos:
#    * https://github.com/bill-auger/git-branch-status/issues/
#    * https://notabug.org/bill-auger/git-branch-status/issues/


read -r -d '' USAGE <<-'USAGE_MSG'
USAGE:

  git-branch-status
  git-branch-status [ base-branch-name compare-branch-name ]
  git-branch-status [ -a | --all ]
  git-branch-status [ -b | --branch ] [filter-branch-name]
  git-branch-status [ -d | --dates ]
  git-branch-status [ -h | --help ]
  git-branch-status [ -l | --local ]
  git-branch-status [ -r | --remotes ]
  git-branch-status [ -v | --verbose ]


EXAMPLES:

  # show only branches for which upstream differs from local
  $ git-branch-status
    | collab-branch  | (behind 1) | (ahead 2) | origin/collab-branch  |
    | feature-branch | (even)     | (ahead 2) | origin/feature-branch |
    | master         | (behind 1) | (even)    | origin/master         |

  # compare two arbitrary branches (either local and either remote)
  $ git-branch-status local-arbitrary-branch fork/arbitrary-branch
    | local-arbitrary-branch | (even)     | (ahead 1) | fork/arbitrary-branch  |
  $ git-branch-status fork/arbitrary-branch local-arbitrary-branch
    | fork/arbitrary-branch  | (behind 1) | (even)    | local-arbitrary-branch |

  # show all branches - including those synchronized, non-tracking, or not checked-out
  $ git-branch-status -a
  $ git-branch-status --all
    | master         | (even)     | (ahead 1) | origin/master             |
    | tracked-branch | (even)     | (even)    | origin/tracked-branch     |
    | (no local)     | n/a        | n/a       | origin/untracked-branch   |
    | local-branch   | n/a        | n/a       | (no upstream)             |
    | master         | (behind 1) | (ahead 1) | a-remote/master           |
    | (no local)     | n/a        | n/a       | a-remote/untracked-branch |

  # show the current branch
  $ git-branch-status -b
  $ git-branch-status --branch
    | current-branch | (even) | (ahead 2) | origin/current-branch |

  # show a specific branch
  $ git-branch-status          specific-branch
  $ git-branch-status -b       specific-branch
  $ git-branch-status --branch specific-branch
    | specific-branch | (even) | (ahead 2) | origin/specific-branch |

  # show the timestamp of each HEAD
  $ git-branch-status -d
  $ git-branch-status --dates
    | 1999-12-31 master | (behind 2) | (even) | 2000-01-01 origin/master |

  # print this usage message
  $ git-branch-status -h
  $ git-branch-status --help
      "prints this usage message"

  # show all local branches - including those synchronized or non-tracking
  $ git-branch-status -l
  $ git-branch-status --local
    | master         | (even)     | (ahead 1) | origin/master             |
    | tracked-branch | (even)     | (even)    | origin/tracked-branch     |
    | local-branch   | n/a        | n/a       | (no upstream)             |

  # show all remote branches - including those not checked-out
  $ git-branch-status -r
  $ git-branch-status --remotes
    | master     | (behind 1) | (even) | a-remote/master           |
    | (no local) | n/a        | n/a    | a-remote/untracked-branch |

  # show all branches with timestamps (like -a -d)
  $ git-branch-status -v
  $ git-branch-status --verbose
    | 1999-12-31 local   | n/a        | n/a    | (no upstream)             |
    | 1999-12-31 master  | (behind 1) | (even) | 2000-01-01 origin/master  |
    | 1999-12-31 tracked | (even)     | (even) | 2000-01-01 origin/tracked |
USAGE_MSG


### constants ###

readonly MAX_COL_W=27 # should be >= 12
readonly CWHITE='\033[0;37m'
readonly CRED='\033[0;31m'
readonly CGREEN='\033[0;32m'
readonly CYELLOW='\033[1;33m'
readonly CBLUE='\033[1;34m'
readonly CEND='\033[0m'
readonly CDEFAULT=$CWHITE
readonly CTRACKING=$CBLUE
readonly CAHEAD=$CYELLOW
readonly CBEHIND=$CRED
readonly CEVEN=$CGREEN
readonly CNOUPSTREAM=$CRED
readonly CNOLOCAL=$CRED
readonly HRULE_CHAR='-'
readonly JOIN_CHAR='~'
readonly JOIN_REGEX="s/$JOIN_CHAR/ /"
readonly STAR='*'
readonly DELIM='|'
readonly NO_UPSTREAM="(no${JOIN_CHAR}upstream)"
readonly NO_LOCAL="(no${JOIN_CHAR}local)"
readonly LOCALS_SYNCED_MSG="All tracking branches are synchronized with their upstreams"
readonly BRANCH_SYNCED_MSG="This tracking branch is synchronized with it's upstream"
readonly REMOTES_SYNCED_MSG="All remote branches with identically named local branches are synchronized with them"
readonly UNTRACKED_SYNCHED_MSG="These branches are synchronized"


### variables ###
n_tracked_differences=0
were_any_branches_compared=0
local_w=0
behind_w=0
ahead_w=0
remote_w=0
declare -a local_msgs=()
declare -a behind_msgs=()
declare -a ahead_msgs=()
declare -a remote_msgs=()
declare -a local_colors=()
declare -a behind_colors=()
declare -a ahead_colors=()
declare -a remote_colors=()


### helpers ###

function GetRefs # (refs_dir)
{
  refs_dir=$1

  git for-each-ref --format="%(refname:short) %(upstream:short)" $refs_dir 2> /dev/null
}

function GetLocalRefs
{
  GetRefs refs/heads
}

function GetRemoteRefs # (remote_repo_name)
{
  remote_repo=$1

  GetRefs refs/remotes/$remote_repo
}

function GetStatus # (local_commit remote_commit)
{
  local_commit=$1
  remote_commit=$2

  git rev-list --left-right ${local_commit}...${remote_commit} -- 2>/dev/null
}

function GetCurrentBranch
{
  git rev-parse --abbrev-ref HEAD
}

function GetUpstreamBranch # (local_branch)
{
  local_branch=$1

  git rev-parse --abbrev-ref $local_branch@{upstream} 2> /dev/null
}

function IsCurrentBranch # (branch_name)
{
  branch=$1
  this_branch=$(AppendHeadDate $branch)
  current_branch=$(AppendHeadDate $(GetCurrentBranch))

  [ "$this_branch" == "$current_branch" ] && echo 1 || echo 0
}

function IsLocalBranch # (branch_name)
{
  branch=$1
  is_local_branch=$(git branch -a | grep -E "^.* $branch$")

  [ "$is_local_branch" ] && echo 1 || echo 0
}

function IsTrackedBranch # (base_branch_name compare_branch_name)
{
  base_branch=$1
  compare_branch=$2
  upstream_branch=$(GetUpstreamBranch $base_branch)
  [ "$compare_branch" == "$upstream_branch" ] && echo 1 || echo 0
}

function DoesBranchExist # (branch_name)
{
  branch=$1
  is_known_branch=$(git branch -a | grep -E "^.* (remotes\/)?$branch$")

  [ "$is_known_branch" ] && echo 1 || echo 0
}

function AppendHeadDate # (commit_ref)
{
  commit=$1
  author_date=$(git log -n 1 --format=format:"%ai" $commit 2> /dev/null)

  (($SHOW_DATES)) && [ "$author_date" ] && date="${author_date:0:10}$JOIN_CHAR"

  echo $date$commit
}

function GetCommitMsg # (commit_ref)
{
  git log -n 1 --format=format:"%s" $1
}

function PrintHRule # (rule_width)
{
  printf "  $(head -c $1 < /dev/zero | tr '\0' $HRULE_CHAR)\n"
}


### business ###

function Reset
{
  n_tracked_differences=0
  were_any_branches_compared=0
  local_w=0
  behind_w=0
  ahead_w=0
  remote_w=0
  local_msgs=()
  behind_msgs=()
  ahead_msgs=()
  remote_msgs=()
  local_colors=()
  behind_colors=()
  ahead_colors=()
  remote_colors=()
}

function GenerateReport # (local_branch_name remote_branch_name)
{
  base_branch=$1
  compare_branch=$2
  does_base_branch_exist=$(DoesBranchExist $base_branch)
  does_compare_branch_exist=$(DoesBranchExist $compare_branch)
  is_tracked_branch=$(IsTrackedBranch $base_branch $compare_branch)

  # filter heads
  [ "$base_branch" == 'HEAD' ] && return

  # filter branches per CLI arg
  [ "$FILTER_BRANCH" -a "$base_branch" != "$FILTER_BRANCH" ] && return

  # parse local<->remote or arbitrary branches sync status
  if (($does_base_branch_exist)) && (($does_compare_branch_exist))
  then status=$(GetStatus $base_branch $compare_branch) ; (($?)) && return ;

       n_behind=$(echo $status | tr " " "\n" | grep -c '^>')
       n_ahead=$( echo $status | tr " " "\n" | grep -c '^<')
       n_differences=$(($n_behind + $n_ahead))
       n_tracked_differences=$(($n_tracked_differences + $n_differences))
       were_any_branches_compared=1

       # filter branches by status
       (($SHOW_ALL_UPSTREAM)) || (($n_differences)) || return

       # set data for branches with remote counterparts or arbitrary branches
       (($is_tracked_branch)) && color=$CTRACKING || color=$CDEFAULT
       local_color=$color
       if (($n_behind))
       then behind_msg="(behind$JOIN_CHAR$n_behind)" ; behind_color=$CBEHIND ;
       else behind_msg="(even)"                      ; behind_color=$CEVEN   ;
       fi
       if (($n_ahead))
       then ahead_msg="(ahead$JOIN_CHAR$n_ahead)"    ; ahead_color=$CAHEAD   ;
       else ahead_msg="(even)"                       ; ahead_color=$CEVEN    ;
       fi
       remote_color=$color
  elif (($does_base_branch_exist)) && !(($does_compare_branch_exist)) && (($SHOW_ALL_LOCAL))
  then # dummy data for local branches with no upstream counterpart
       local_color=$CDEFAULT
       behind_color="$CDEFAULT"  ; behind_msg="n/a"              ;
       ahead_color="$CDEFAULT"   ; ahead_msg="n/a"               ;
       remote_color=$CNOUPSTREAM ; compare_branch="$NO_UPSTREAM" ;
  elif !(($does_base_branch_exist)) && (($does_compare_branch_exist)) && (($SHOW_ALL_REMOTE))
  then # dummy data for remote branches with no local counterpart
       local_color=$CNOLOCAL     ; base_branch="$NO_LOCAL"       ;
       behind_color="$CDEFAULT"  ; behind_msg="n/a"              ;
       ahead_color="$CDEFAULT"   ; ahead_msg="n/a"               ;
       remote_color=$CDEFAULT
  else return
  fi

  # populate lists
  local_msg="$(AppendHeadDate $base_branch)"     ; local_msg="${local_msg:0:$MAX_COL_W}"   ;
  remote_msg="$(AppendHeadDate $compare_branch)" ; remote_msg="${remote_msg:0:$MAX_COL_W}" ;
  local_msgs=(    ${local_msgs[@]}    "$local_msg"    )
  behind_msgs=(   ${behind_msgs[@]}   "$behind_msg"   )
  ahead_msgs=(    ${ahead_msgs[@]}    "$ahead_msg"    )
  remote_msgs=(   ${remote_msgs[@]}   "$remote_msg"   )
  local_colors=(  ${local_colors[@]}  "$local_color"  )
  behind_colors=( ${behind_colors[@]} "$behind_color" )
  ahead_colors=(  ${ahead_colors[@]}  "$ahead_color"  )
  remote_colors=( ${remote_colors[@]} "$remote_color" )

  # determine max column widths
  if [ ${#local_msg}  -gt $local_w  ] ; then local_w=${#local_msg}   ; fi ;
  if [ ${#behind_msg} -gt $behind_w ] ; then behind_w=${#behind_msg} ; fi ;
  if [ ${#ahead_msg}  -gt $ahead_w  ] ; then ahead_w=${#ahead_msg}   ; fi ;
  if [ ${#remote_msg} -gt $remote_w ] ; then remote_w=${#remote_msg} ; fi ;
}

function PrintReportLine
{
  # fetch data
  local_msg=$(  echo ${local_msgs[$result_n]}  | sed "$JOIN_REGEX" )
  behind_msg=$( echo ${behind_msgs[$result_n]} | sed "$JOIN_REGEX" )
  ahead_msg=$(  echo ${ahead_msgs[$result_n]}  | sed "$JOIN_REGEX" )
  remote_msg=$( echo ${remote_msgs[$result_n]} | sed "$JOIN_REGEX" )
  local_color="${local_colors[$result_n]}"
  behind_color="${behind_colors[$result_n]}"
  ahead_color="${ahead_colors[$result_n]}"
  remote_color="${remote_colors[$result_n]}"

  # calculate column offsets
  local_offset=1
  behind_offset=$(( $local_w  - ${#local_msg}  ))
  ahead_offset=$((  $behind_w - ${#behind_msg} ))
  remote_offset=$(( $ahead_w  - ${#ahead_msg}  ))
  end_offset=$((    $remote_w - ${#remote_msg} ))

  # build output messages and display
  if (($(IsCurrentBranch $local_msg))) ; then star=$STAR ; else star=" " ; fi ;
  local_msg="%$((  $local_offset  ))s$star$(echo -e $DELIM $local_color$local_msg$CEND)"
  behind_msg="%$(( $behind_offset ))s $(    echo -e $DELIM $behind_color$behind_msg$CEND)"
  ahead_msg="%$((  $ahead_offset  ))s $(    echo -e $DELIM $ahead_color$ahead_msg$CEND)"
  remote_msg="%$(( $remote_offset ))s $(    echo -e $DELIM $remote_color$remote_msg$CEND)"
  end_msg="%$((    $end_offset    ))s $DELIM"
  printf "$local_msg$behind_msg$ahead_msg$remote_msg$end_msg\n"
}

function PrintReport # (table_header_line no_results_msg)
{
  header=$1
  no_results_msg=$2
  n_notable_differences=${#local_msgs[@]}
  (($were_any_branches_compared)) && !(($n_tracked_differences)) && all_in_sync=1 || all_in_sync=0

  !(($n_notable_differences)) && !(($all_in_sync)) && return

  # pretty print results
  printf "\n  $header\n"
  if (($n_notable_differences))
  then rule_w=$(($local_w+$behind_w+$ahead_w+$remote_w+13))

       PrintHRule $rule_w
       for (( result_n = 0 ; result_n < $n_notable_differences ; result_n++ ))
       do PrintReportLine
       done
       PrintHRule $rule_w
  fi

  # print synchronized message if all compared upstreams had no diffs
  if (($all_in_sync))
  then rule_w=$((${#no_results_msg}+4))

       PrintHRule $rule_w
       echo -e "  $DELIM $CEVEN$no_results_msg$CEND $DELIM"
       PrintHRule $rule_w
  fi

  Reset
}


### main entry ###

# parse CLI switches
show_dates=0
show_all=0
show_all_local=0
show_all_upstream=0
show_all_remote=0

case "$1" in
  '-a'|'--all'     ) show_all=1                                                ;;
  '-b'|'--branch'  ) [ "$2" ] && branch_a="$2" || branch_a=$(GetCurrentBranch) ;;
  '-d'|'--dates'   ) show_dates=1                                              ;;
  '-h'|'--help'    ) echo "$USAGE" ; exit                                      ;;
  '-l'|'--local'   ) show_all_local=1 ; show_all_upstream=1 ;                  ;;
  '-r'|'--remotes' ) show_all_remote=1                                         ;;
  '-v'|'--verbose' ) show_all=1 ; show_dates=1                                 ;;
  *                ) branch_a="$1" branch_b="$2"                               ;;
esac

[ "$branch_a" ] && !(($(DoesBranchExist $branch_a))) && echo "no such branch: '$branch_a'" && exit
[ "$branch_b" ] && !(($(DoesBranchExist $branch_b))) && echo "no such branch: '$branch_b'" && exit
[ "$branch_a" ] && [ -z "$branch_b" ] && !(($(IsLocalBranch $branch_a))) && echo "no such local branch: '$branch_a'" && exit
[ "$branch_a" ] && show_all_local=1 # force "no upstream" message for non-tracking branches

readonly FILTER_BRANCH=$branch_a
readonly COMPARE_BRANCH=$branch_b
readonly SHOW_DATES=$show_dates
readonly SHOW_ALL=$show_all
readonly SHOW_ALL_LOCAL=$(($show_all    + $show_all_local))    # show local branches that are not tracking any upstream
readonly SHOW_ALL_UPSTREAM=$(($show_all + $show_all_upstream)) # show local branches that are synchronized with their upstream
readonly SHOW_ALL_REMOTE=$(($show_all   + $show_all_remote))   # show all remote branches


if [ -z "$COMPARE_BRANCH" ]
then [ "$FILTER_BRANCH" ] && synched_msg="$BRANCH_SYNCED_MSG" || synched_msg="$LOCALS_SYNCED_MSG"

     # compare sync status of local branches to their upstreams
     while read local upstream ; do GenerateReport $local $upstream ; done < <(GetLocalRefs) ;
     PrintReport "local <-> upstream" "$synched_msg"
else is_tracked_branch=$(IsTrackedBranch $FILTER_BRANCH $COMPARE_BRANCH)
     (($is_tracked_branch)) && synched_msg="$BRANCH_SYNCED_MSG" || synched_msg="$UNTRACKED_SYNCHED_MSG"

     # compare sync status of arbitrary branches per cli args
     GenerateReport $FILTER_BRANCH $COMPARE_BRANCH
     PrintReport "$FILTER_BRANCH <-> $COMPARE_BRANCH" "$synched_msg"
fi


(($SHOW_ALL_REMOTE)) && [ -z "$FILTER_BRANCH" ] || exit

# compare sync status of remote branches to local branches with the same names
for remote_repo in `git remote`
do while read remote_branch
   do local_branch=${remote_branch#$remote_repo/}

      GenerateReport $local_branch $remote_branch
   done < <(GetRemoteRefs $remote_repo)

   PrintReport "local <-> $remote_repo" "$REMOTES_SYNCED_MSG"
done
