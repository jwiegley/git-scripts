# git-branch-status - print pretty git branch sync status reports

By default, the `git-branch-status` command shows the divergence relationship between branches for which the upstream differs from it's local counterpart.

A number of command-line switches exist selecting various output modes to compare any or all local or remote branches.

![git-branch-status screenshot][scrot]

```
USAGE:

  git-branch-status
  git-branch-status [-a | --all]
  git-branch-status [-b | --branch] [branch-name]
  git-branch-status [-d | --dates]
  git-branch-status [-h | --help]
  git-branch-status [-r | --remotes]
  git-branch-status [-v | --verbose]

EXAMPLES:

  # show only branches for which upstream differs from local
  $ git-branch-status
    | collab-branch  | (behind 1) | (ahead 2) | origin/collab-branch  |
    | feature-branch | (even)     | (ahead 2) | origin/feature-branch |
    | master         | (behind 1) | (even)    | origin/master         |

  # show all branches - including those up-to-date, with no upstream, or not checked-out
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
```

_NOTE: please direct comments, bug reports, feature requests, or PRs to one of the upstream repos:_
* [https://github.com/bill-auger/git-branch-status/issues/][github-issues]
* [https://notabug.org/bill-auger/git-branch-status/issues/][notabug-issues]


[scrot]:          http://bill-auger.github.io/git-branch-status-scrot.png "git-branch-status screenshot"
[github-issues]:  https://github.com/bill-auger/git-branch-status/issues/
[notabug-issues]: https://notabug.org/bill-auger/git-branch-status/issues/
