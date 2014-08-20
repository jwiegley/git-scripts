#/bin/bash

# this script clones a repository, including all its remote branches
# Author: julianromera

GIT=`which git`

if [ "x$1" = "x" -o "x$2" = "x" ];then
  echo "use: $0 <git_repository_to_clone> <directory>"
  exit 1
fi

if [ "x$GIT" = "x" ];then
  echo "No git command found. install it"
fi

function clone {

  $GIT clone -q $1 $2
  res=$?
  
  cd $2
  
  $GIT pull --all
  
  for remote in `$GIT branch -r | grep -v \>`; do
     $GIT branch --track ${remote#origin/} $remote;
  done
}

echo "cloning repository into ... $2"
clone $1 $2
