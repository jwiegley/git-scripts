#!/usr/bin/env bash

#  git-branch-status - print pretty git branch sync status reports
#
#  Copyright 2011      Jehiah Czebotar     <https://github.com/jehiah>
#  Copyright 2013      Fredrik Strandin    <https://github.com/kd35a>
#  Copyright 2014      Kristijan Novoselić <https://github.com/knovoselic>
#  Copyright 2014-2020 bill-auger          <https://github.com/bill-auger>
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
#    * original `git rev-list` grepping - by Jehiah Czebotar
#    * "s'all good!" message            - by Fredrik Strandin
#    * ANSI colors                      - by Kristijan Novoselić
#    * various features and maintenance - by bill-auger

#  please direct bug reports, feature requests, or PRs to one of the upstream repos:
#    * https://github.com/bill-auger/git-branch-status/issues/
#    * https://notabug.org/bill-auger/git-branch-status/issues/
#    * https://pagure.io/git-branch-status/issues/


read -r -d '' USAGE <<-'USAGE_MSG'
USAGE:

  git-branch-status
  git-branch-status [ base-branch-name compare-branch-name ]
  git-branch-status [ -a | --all ]
  git-branch-status [ -b | --branch ] [ filter-branch-name ]
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

  # compare two arbitrary branches - local or remote
  $ git-branch-status local-arbitrary-branch fork/arbitrary-branch
    | local-arbitrary-branch | (even)     | (ahead 1) | fork/arbitrary-branch  |
  $ git-branch-status fork/arbitrary-branch local-arbitrary-branch
    | fork/arbitrary-branch  | (behind 1) | (even)    | local-arbitrary-branch |

  # show all branches - local and remote, regardless of state or relationship
  $ git-branch-status -a
  $ git-branch-status --all
   *| master         | (even)     | (ahead 1) | origin/master             |
    | tracked-branch | (even)     | (even)    | origin/tracked-branch     |
    | (no local)     | n/a        | n/a       | origin/untracked-branch   |
    | local-branch   | n/a        | n/a       | (no upstream)             |
    | master         | (behind 1) | (ahead 1) | a-remote/master           |
    | (no local)     | n/a        | n/a       | a-remote/untracked-branch |

  # show the current branch
  $ git-branch-status -b
  $ git-branch-status --branch
   *| current-branch | (even) | (ahead 2) | origin/current-branch |

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
   *| master         | (even)     | (ahead 1) | origin/master         |
    | tracked-branch | (even)     | (even)    | origin/tracked-branch |
    | local-branch   | n/a        | n/a       | (no upstream)         |

  # show all remote branches - including those not checked-out
  $ git-branch-status -r
  $ git-branch-status --remotes
    | master     | (behind 1) | (even) | a-remote/master           |
    | (no local) | n/a        | n/a    | a-remote/untracked-branch |

  # show all branches with timestamps (like -a -d)
  $ git-branch-status -v
  $ git-branch-status --verbose
    | 1999-12-31 master    | (behind 1) | (even) | 2000-01-01 origin/master  |
    | 1999-12-31 tracked   | (even)     | (even) | 2000-01-01 origin/tracked |
   *| 1999-12-31 local-wip | n/a        | n/a    | (no upstream)             |
USAGE_MSG


## user-defined configuration (see ./gbs-config.sh.inc.example) ##

readonly THIS_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" 2>/dev/null || echo $0)")"
readonly DEF_CFG_FILE="${THIS_DIR}"/gbs-config.sh.inc
readonly CFG_FILE=$(        ( [[ "$GBS_CFG_FILE" && -f "$GBS_CFG_FILE" ]] && echo -n "$GBS_CFG_FILE"        ) || \
                            ( [[ "$DEF_CFG_FILE" && -f "$DEF_CFG_FILE" ]] && echo -n "$DEF_CFG_FILE"        )    )
source "$CFG_FILE" 2> /dev/null
readonly FETCH_PERIOD=$(    ( [[ "$GBS_FETCH_PERIOD" =~ ^-?[0-9]+$     ]] && echo -n "$GBS_FETCH_PERIOD"    ) || \
                            ( [[ "$CFG_FETCH_PERIOD" =~ ^-?[0-9]+$     ]] && echo -n "$CFG_FETCH_PERIOD"    ) || \
                                                                             echo -n '-1'                        )
readonly LAST_FETCH_FILE=$( ( touch "$GBS_LAST_FETCH_FILE" 2> /dev/null   && echo -n "$GBS_LAST_FETCH_FILE" ) || \
                            ( touch "$CFG_LAST_FETCH_FILE" 2> /dev/null   && echo -n "$CFG_LAST_FETCH_FILE" ) || \
                                                                             echo -n ~/.GBS_LAST_FETCH           )
readonly USE_ANSI_COLOR=$(  ( [[ "$GBS_USE_ANSI_COLOR" =~ ^[01]$       ]] && echo -n "$GBS_USE_ANSI_COLOR"  ) || \
                            ( [[ "$CFG_USE_ANSI_COLOR" =~ ^[01]$       ]] && echo -n "$CFG_USE_ANSI_COLOR"  ) || \
                                                                             echo -n '1'                         )


## constants ##

readonly INNER_PADDING_W=13 # '| ' + ' | ' + ' | ' + ' | ' + ' |'
readonly MARGIN_PAD_W=2
readonly MARGINS_PAD_W=$(( $MARGIN_PAD_W * 2 ))
readonly ALL_PADDING_W=$(( $INNER_PADDING_W + $MARGINS_PAD_W ))
readonly MAX_DIVERGENCE_W=12 # e.g. "(behind 999)"
readonly MIN_TTY_W=60 # ASSERT: (0 < (ALL_PADDING_W + (MAX_DIVERGENCE_W * 2)) <= MIN_TTY_W)
readonly MARGIN_PAD=$(printf "%$(( $MARGIN_PAD_W ))s")
readonly CWHITE=$(  (( $USE_ANSI_COLOR )) && echo '\033[0;37m' )
readonly CRED=$(    (( $USE_ANSI_COLOR )) && echo '\033[0;31m' )
readonly CGREEN=$(  (( $USE_ANSI_COLOR )) && echo '\033[0;32m' )
readonly CYELLOW=$( (( $USE_ANSI_COLOR )) && echo '\033[1;33m' )
readonly CBLUE=$(   (( $USE_ANSI_COLOR )) && echo '\033[1;34m' )
readonly CEND=$(    (( $USE_ANSI_COLOR )) && echo '\033[0m'    )
readonly CDEFAULT=$CWHITE
readonly CTRACKING=$CBLUE
readonly CAHEAD=$CYELLOW
readonly CBEHIND=$CRED
readonly CEVEN=$CGREEN
readonly CNOUPSTREAM=$CRED
readonly CNOLOCAL=$CRED
readonly HRULE_CHAR='-'
readonly JOIN_CHAR='~'
readonly JOIN_REGEX="s/$JOIN_CHAR/ /g"
readonly TRIM_REGEX="s/.*\(.\{$MAX_DIVERGENCE_W\}\)$/\1/"
readonly STAR='*'
readonly DELIM='|'
readonly NO_UPSTREAM="(no${JOIN_CHAR}upstream)"
readonly NO_LOCAL="(no${JOIN_CHAR}local)"
readonly NOT_REPO_MSG="Not a git repo"
readonly BARE_REPO_MSG="Bare repo"
readonly NO_COMMITS_MSG="No commits"
readonly TTY_W_MSG="TTY must be wider than $MIN_TTY_W chars"
readonly INVALID_BRANCH_MSG="No such branch:"
readonly INVALID_LOCAL_BRANCH_MSG="No such local branch:"
readonly NO_RESULTS_MSG="Nothing to compare"
readonly NO_REFS_MSG="(No data)"
readonly LOCALS_SYNCED_MSG="All tracking branches are synchronized with their upstreams"
readonly BRANCH_SYNCED_MSG="This tracking branch is synchronized with it's upstream"
readonly UNTRACKED_SYNCHED_MSG="These branches are synchronized with no tracking relationship"
readonly REMOTES_SYNCED_MSG="All local branches with corresponding names on this remote are synchronized with that remote"
readonly EXIT_SUCCESS=0
readonly EXIT_FAILURE=1
# readonly SHOW_DATES      # Init()
# readonly SHOW_ALL_SYNCED # Init()
# readonly SHOW_ALL_LOCAL  # Init()
# readonly SHOW_ALL_REMOTE # Init()
# readonly FILTER_BRANCH   # Init()
# readonly COMPARE_BRANCH  # Init()


## variables ##

WereAnyDivergences=0
WereAnyCompared=0
LocalW=0
BehindW=0
AheadW=0
RemoteW=0
declare -a LocalMsgs=()
declare -a BehindMsgs=()
declare -a AheadMsgs=()
declare -a RemoteMsgs=()
declare -a LocalColors=()
declare -a BehindColors=()
declare -a AheadColors=()
declare -a RemoteColors=()


## helpers ##

AssertIsValidRepo()
{
  [ "$(git rev-parse --is-inside-work-tree 2> /dev/null)" == 'true' ] || \
  ! (( $(AssertIsNotBareRepo) ))                                      && echo 1 || echo 0
}

AssertHasCommits()
{
  [ "$(git cat-file -t HEAD 2> /dev/null)" ] && echo 1 || echo 0
}

AssertIsNotBareRepo()
{
  [ "$(git rev-parse --is-bare-repository 2> /dev/null)" != 'true' ] && echo 1 || echo 0
}

GetRefs() # (refs_dir)
{
  local refs_dir=$1
  local fmt='%(refname:short) %(upstream:short)'
  local sort=$( (( $SHOW_DATES )) && echo '--sort=creatordate')

  git for-each-ref --format="$fmt" $sort $refs_dir 2> /dev/null
}

GetLocalRefs()
{
  GetRefs refs/heads
}

GetRemoteRefs() # (remote_repo_name)
{
  local remote_repo=$1

  GetRefs refs/remotes/$remote_repo
}

GetStatus() # (base_commit compare_commit)
{
  local base_commit=$1
  local compare_commit=$2

  git rev-list --left-right ${base_commit}...${compare_commit} -- 2>/dev/null
}

GetCurrentBranch()
{
  git rev-parse --abbrev-ref HEAD
}

GetUpstreamBranch() # (local_branch)
{
  local local_branch=$1

  git rev-parse --abbrev-ref $local_branch@{upstream} 2> /dev/null
}

IsCurrentBranch() # (branch_name)
{
  local branch=$1
  local this_branch=$(AppendHeadDate $branch)
  local current_branch=$(AppendHeadDate $(GetCurrentBranch))

  [ "$this_branch" == "$current_branch" ] && echo 1 || echo 0
}

IsLocalBranch() # (branch_name)
{
  local branch=$1
  local is_local_branch=$(git branch -a | grep -E "^.* $branch$")

  [ "$is_local_branch" ] && echo 1 || echo 0
}

IsTrackedBranch() # (base_branch_name compare_branch_name)
{
  local base_branch=$1
  local compare_branch=$2
  local upstream_branch=$(GetUpstreamBranch $base_branch)

  [ "$compare_branch" == "$upstream_branch" ] && echo 1 || echo 0
}

DoesBranchExist() # (branch_name)
{
  local branch=$1
  local is_known_branch=$(git branch -a | grep -E "^.* (remotes\/)?$branch$")

  [ "$is_known_branch" ] && echo 1 || echo 0
}

AppendHeadDate() # (commit_ref)
{
  local commit=$1
  local author_date=$(git log -n 1 --format=format:"%ai" $commit 2> /dev/null)
  local date=''

  (($SHOW_DATES)) && [ "$author_date" ] && date="${author_date:0:10}$JOIN_CHAR"

  echo $date$commit
}

CurrentTtyW()
{
  local tty_dims=$(stty -F /dev/tty size 2> /dev/null || stty -f /dev/tty size 2> /dev/null)
  local tty_w=$(echo $tty_dims | cut -d ' ' -f 2)

  (( $tty_w )) && echo "$tty_w" || echo "$MIN_TTY_W"
}

PrintHRule() # (rule_width)
{
  local rule_w=$1
  local h_rule="$(dd if=/dev/zero bs=$rule_w count=1 2> /dev/null | tr '\0' $HRULE_CHAR)"

  echo "$MARGIN_PAD$h_rule"
}

Exit() # (exit_msg exit_status)
{
  local exit_msg=$1
  local exit_status=$2

  case "$exit_status" in
       $EXIT_SUCCESS      ) echo "$exit_msg"        ; exit $EXIT_SUCCESS ;;
       $EXIT_FAILURE | '' ) echo "fatal: $exit_msg" ; exit $EXIT_FAILURE ;;
  esac
}


## business ##

Init() # (cli_args)
{
  local show_dates=0
  local show_all=0
  local show_all_synced=0
  local show_all_local=0
  local show_all_remote=0
  local branch_a=''
  local branch_b=''

  # parse CLI switches
  case "$1" in
       '-a'|'--all'     ) show_all=1                                                ;;
       '-b'|'--branch'  ) [ "$2" ] && branch_a="$2" || branch_a=$(GetCurrentBranch) ;;
       '-d'|'--dates'   ) branch_a="$2"     ; branch_b="$3"     ; show_dates=1      ;;
       '-h'|'--help'    ) Exit "$USAGE" $EXIT_SUCCESS                               ;;
       '-l'|'--local'   ) show_all_local=1  ; show_all_synced=1 ;                   ;;
       '-r'|'--remotes' ) show_all_remote=1 ; show_all_synced=1 ;                   ;;
       '-v'|'--verbose' ) show_all=1        ; show_dates=1      ;                   ;;
       *                ) branch_a="$1"     ; branch_b="$2"     ;                   ;;
  esac

  # sanity checks
  (( $(AssertIsValidRepo  )       ))                                        || Exit "$NOT_REPO_ERR"
  (( $(AssertIsNotBareRepo)       ))                                        || Exit "$BARE_REPO_MSG"
  (( $(AssertHasCommits   )       ))                                        || Exit "$NO_COMMITS_MSG"
  (( $(CurrentTtyW) >= $MIN_TTY_W ))                                        || Exit "$TTY_W_MSG"
  [ -z "$branch_a" ] || (($(DoesBranchExist $branch_a)))                    || Exit "$INVALID_BRANCH_MSG '$branch_a'"
  [ -z "$branch_b" ] || (($(DoesBranchExist $branch_b)))                    || Exit "$INVALID_BRANCH_MSG '$branch_b'"
  [ -z "$branch_a" ] || (($(IsLocalBranch   $branch_a))) || [ "$branch_b" ] || Exit "$INVALID_LOCAL_BRANCH_MSG '$branch_a'"
  [ -z "$branch_a" ] || show_all_local=1 # force "no upstream" message for non-tracking branches

  readonly SHOW_DATES=$show_dates
  readonly SHOW_ALL_SYNCED=$(( $show_all + $show_all_synced )) # show branches that are synchronized with their counterparts
  readonly SHOW_ALL_LOCAL=$((  $show_all + $show_all_local  )) # show local branches that are not tracking any upstream
  readonly SHOW_ALL_REMOTE=$(( $show_all + $show_all_remote )) # show all remote branches
  readonly FILTER_BRANCH=$branch_a
  readonly COMPARE_BRANCH=$branch_b
}

FetchRemotes()
{
  if   (( ${FETCH_PERIOD} >= 0 ))
  then local now_ts=$(date +%s)
       local last_fetch_ts=$(( $(cat ${LAST_FETCH_FILE} 2> /dev/null) + 0 ))

       if   (( ${now_ts} - ${last_fetch_ts} >= ${FETCH_PERIOD} * 60 ))
       then git fetch --all
            echo -n ${now_ts} > ${LAST_FETCH_FILE}
       fi
  fi
}

GenerateReports()
{
  if   [ "$COMPARE_BRANCH" ]
  then CustomReport $(IsTrackedBranch $FILTER_BRANCH $COMPARE_BRANCH)
  else (( !(( $SHOW_ALL_REMOTE )) || (( $SHOW_ALL_LOCAL )) )) && LocalReport
       (( $SHOW_ALL_REMOTE                                 )) && RemoteReport
  fi
}

CustomReport() # (is_tracked_branch)
{
  local is_tracked_branch=$1
  local synchronized_msg
  (($is_tracked_branch)) && synchronized_msg="$BRANCH_SYNCED_MSG" || synchronized_msg="$UNTRACKED_SYNCHED_MSG"

  # compare sync status of arbitrary branches per cli args
  GenerateReport $FILTER_BRANCH $COMPARE_BRANCH
  PrintReport "$FILTER_BRANCH <-> $COMPARE_BRANCH" "$synchronized_msg"
}

LocalReport()
{
  local synchronized_msg
  [ "$FILTER_BRANCH" ] && synchronized_msg="$BRANCH_SYNCED_MSG" || synchronized_msg="$LOCALS_SYNCED_MSG"

  # compare sync status of local branches to their upstreams
  while read local upstream ; do GenerateReport $local $upstream ; done < <(GetLocalRefs) ;
  PrintReport "local <-> upstream" "$synchronized_msg"
}

RemoteReport()
{
  local synchronized_msg="$REMOTES_SYNCED_MSG"
  local remote_repo

  # compare sync status of remote branches to local branches with the same names
  for remote_repo in $(git remote)
  do  while read remote_branch
      do    local local_branch=${remote_branch#$remote_repo/}

            GenerateReport $local_branch $remote_branch
      done < <(GetRemoteRefs $remote_repo)

      PrintReport "local <-> $remote_repo" "$synchronized_msg"
  done
}

GenerateReport() # (base_branch compare_branch)
{
  local base_branch=$1
  local compare_branch=$2
  local does_base_branch_exist=$(   DoesBranchExist $base_branch                )
  local does_compare_branch_exist=$(DoesBranchExist $compare_branch             )
  local is_tracked_branch=$(        IsTrackedBranch $base_branch $compare_branch)
  local local_msg   ; local behind_msg   ; local ahead_msg   ; local remote_msg   ;
  local local_color ; local behind_color ; local ahead_color ; local remote_color ;

  # filter heads
  [ "$base_branch" != 'HEAD' ] || return

  # filter branches per CLI arg
  [ -z "$FILTER_BRANCH" -o "$base_branch" == "$FILTER_BRANCH" ] || return

  # parse local<->remote or arbitrary branches sync status
  if   (( ! $does_base_branch_exist )) && (( ! $does_compare_branch_exist )) ; then return ;
  elif ((   $does_base_branch_exist )) && ((   $does_compare_branch_exist ))
  then local status=$(GetStatus $base_branch $compare_branch) ; (( ! $? )) || return ;

       local n_behind=$(echo $status | tr " " "\n" | grep -c '^>')
       local n_ahead=$( echo $status | tr " " "\n" | grep -c '^<')
       local n_divergences=$(( $n_behind + $n_ahead ))
       (( $WereAnyDivergences + $n_divergences )) && WereAnyDivergences=1
       WereAnyCompared=1

       # filter branches by status
       (( $SHOW_ALL_SYNCED )) || (( $n_divergences > 0 )) || return

       # set data for branches with remote counterparts or arbitrary branches
       (($is_tracked_branch)) && color=$CTRACKING || color=$CDEFAULT
       local_color=$color
       if   (( $n_behind ))
       then behind_msg="(behind$JOIN_CHAR$n_behind)" ; behind_color=$CBEHIND ;
       else behind_msg="(even)"                      ; behind_color=$CEVEN   ;
       fi
       if   (( $n_ahead ))
       then ahead_msg="(ahead$JOIN_CHAR$n_ahead)"    ; ahead_color=$CAHEAD   ;
       else ahead_msg="(even)"                       ; ahead_color=$CEVEN    ;
       fi
       remote_color=$color
  elif ((   $does_base_branch_exist )) && (( $SHOW_ALL_LOCAL ))
  then # dummy data for local branches with no upstream counterpart
       local_color=$CDEFAULT
       behind_color="$CDEFAULT"  ; behind_msg="n/a"              ;
       ahead_color="$CDEFAULT"   ; ahead_msg="n/a"               ;
       remote_color=$CNOUPSTREAM ; compare_branch="$NO_UPSTREAM" ;
  elif (( ! $does_base_branch_exist )) && (( $SHOW_ALL_REMOTE ))
  then # dummy data for remote branches with no local counterpart
       local_color=$CNOLOCAL     ; base_branch="$NO_LOCAL"       ;
       behind_color="$CDEFAULT"  ; behind_msg="n/a"              ;
       ahead_color="$CDEFAULT"   ; ahead_msg="n/a"               ;
       remote_color=$CDEFAULT
  else return
  fi
  local_msg="$( AppendHeadDate $base_branch         )"
  behind_msg=$( echo $behind_msg | sed "$TRIM_REGEX")
  ahead_msg=$(  echo $ahead_msg  | sed "$TRIM_REGEX")
  remote_msg="$(AppendHeadDate $compare_branch      )"

  # populate lists
  LocalMsgs=(    ${LocalMsgs[@]}    "$local_msg"    )
  BehindMsgs=(   ${BehindMsgs[@]}   "$behind_msg"   )
  AheadMsgs=(    ${AheadMsgs[@]}    "$ahead_msg"    )
  RemoteMsgs=(   ${RemoteMsgs[@]}   "$remote_msg"   )
  LocalColors=(  ${LocalColors[@]}  "$local_color"  )
  BehindColors=( ${BehindColors[@]} "$behind_color" )
  AheadColors=(  ${AheadColors[@]}  "$ahead_color"  )
  RemoteColors=( ${RemoteColors[@]} "$remote_color" )

  # determine uniform column widths
  if [ ${#local_msg}  -gt $LocalW  ] ; then LocalW=${#local_msg}   ; fi ;
  if [ ${#behind_msg} -gt $BehindW ] ; then BehindW=${#behind_msg} ; fi ;
  if [ ${#ahead_msg}  -gt $AheadW  ] ; then AheadW=${#ahead_msg}   ; fi ;
  if [ ${#remote_msg} -gt $RemoteW ] ; then RemoteW=${#remote_msg} ; fi ;
}

PrintReport() # (table_header_line synchronized_msg)
{
  local table_header_line=$1
  local synchronized_msg=$2
  local available_w=$(( $(CurrentTtyW) - $AheadW - $BehindW - $ALL_PADDING_W ))
  local n_results=${#LocalMsgs[@]}
  local are_all_in_sync
  local result_n
  (( $WereAnyCompared )) && !(( $WereAnyDivergences )) && are_all_in_sync=1 || are_all_in_sync=0

  # truncate column widths to fit
  while (( $LocalW + $RemoteW > $available_w ))
  do    (( $LocalW >= $RemoteW )) && LocalW=$((  $LocalW  - 1 ))
        (( $LocalW <= $RemoteW )) && RemoteW=$(( $RemoteW - 1 ))
  done

  # print comparison header
  if   (( $are_all_in_sync ))
  then printf "\n$CGREEN$MARGIN_PAD$table_header_line$CEND\n"
  elif (( $n_results > 0 ))
  then printf "\n$MARGIN_PAD$table_header_line\n"
  else printf "\n$CRED$MARGIN_PAD$table_header_line$CEND$MARGIN_PAD"
  fi

  # pretty print divergence results
  if   (( $n_results > 0 ))
  then local rule_w=$(( $LocalW + $BehindW + $AheadW + $RemoteW + $INNER_PADDING_W ))

       PrintHRule $rule_w
       for (( result_n = 0 ; result_n < $n_results ; ++result_n ))
       do  PrintReportLine
       done
       PrintHRule $rule_w
  else ([ -z "$(GetRemoteRefs $remote_repo)" ] && printf "$CRED$NO_REFS_MSG$CEND\n") || \
       (!(( $are_all_in_sync ))                && echo "$NO_RESULTS_MSG"           )
  fi

  # print "synchronized" message if all compared upstreams had no divergence
  if   (( $are_all_in_sync ))
  then local l_border="$DELIM " ; local r_border=" $DELIM" ;
       local borders_pad_w=$(( ${#l_border} + ${#r_border} ))
       local wrap_w=$(( $(CurrentTtyW) - $MARGINS_PAD_W - $borders_pad_w ))
       local line
       local rule_w=0

       # wrap message and determine box width
       synchronized_msg=$(echo "$synchronized_msg" | fold -s -w $wrap_w | tr ' ' "$JOIN_CHAR")
       for line in $synchronized_msg
       do  line=${line/%$JOIN_CHAR/}
           [ ${#line} -gt $rule_w ] && rule_w=${#line}
       done
       rule_w=$(( $rule_w + $MARGINS_PAD_W ))

       # display message
       PrintHRule $rule_w
       for line in $synchronized_msg
       do  line=${line/%$JOIN_CHAR/}
           local pad_w=$(( $rule_w - ${#line} - $MARGINS_PAD_W ))
           local line_fmt="$MARGIN_PAD$l_border$CEVEN$line$CEND%$(( $pad_w ))s$r_border"
           printf "$line_fmt\n" | sed "$JOIN_REGEX"
       done
       PrintHRule $rule_w
  fi

  Reset
}

PrintReportLine()
{
  # select data
  local local_msg=$( echo ${LocalMsgs[ $result_n]}         | sed "$JOIN_REGEX")
  local behind_msg=$(echo ${BehindMsgs[$result_n]:$trim_w} | sed "$JOIN_REGEX")
  local ahead_msg=$( echo ${AheadMsgs[ $result_n]:$trim_w} | sed "$JOIN_REGEX")
  local remote_msg=$(echo ${RemoteMsgs[$result_n]}         | sed "$JOIN_REGEX")
  local end_msg ; local star ;
  local_msg="${local_msg:0:$LocalW}"
  remote_msg="${remote_msg:0:$RemoteW}"
  local local_color="${LocalColors[$result_n]}"
  local behind_color="${BehindColors[$result_n]}"
  local ahead_color="${AheadColors[$result_n]}"
  local remote_color="${RemoteColors[$result_n]}"

  # calculate column offsets
  local local_offset=1
  local behind_offset=$(( $LocalW  - ${#local_msg}  ))
  local ahead_offset=$((  $BehindW - ${#behind_msg} ))
  local remote_offset=$(( $AheadW  - ${#ahead_msg}  ))
  local end_offset=$((    $RemoteW - ${#remote_msg} ))

  # build output messages and display
  (( $(IsCurrentBranch $local_msg) )) && star=$STAR || star=" "
  local_msg="%$((  $local_offset  ))s$star$(echo -e $DELIM $local_color$local_msg$CEND  )"
  behind_msg="%$(( $behind_offset ))s $(    echo -e $DELIM $behind_color$behind_msg$CEND)"
  ahead_msg="%$((  $ahead_offset  ))s $(    echo -e $DELIM $ahead_color$ahead_msg$CEND  )"
  remote_msg="%$(( $remote_offset ))s $(    echo -e $DELIM $remote_color$remote_msg$CEND)"
  end_msg="%$((    $end_offset    ))s $DELIM"
  printf "$local_msg$behind_msg$ahead_msg$remote_msg$end_msg\n"
}

Reset()
{
  WereAnyDivergences=0
  WereAnyCompared=0
  LocalW=0
  BehindW=0
  AheadW=0
  RemoteW=0
  LocalMsgs=()
  BehindMsgs=()
  AheadMsgs=()
  RemoteMsgs=()
  LocalColors=()
  BehindColors=()
  AheadColors=()
  RemoteColors=()
}


## main entry ##

Init $*
FetchRemotes
GenerateReports
