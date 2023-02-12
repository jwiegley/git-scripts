#!/usr/bin/env bash

# this script clones a repository, including all its remote branches
# Author: julianromera

if [[ -z "$1" || -z "$2" ]];then
  echo "use: $0 <git_repository_to_clone> <directory>"
  exit 1
fi


function clone {

  git clone -q "$1" "$2"
  cd "$2"

  git pull --all

  for remote in $(git branch -r | grep -v \>); do
     git branch --track "${remote#origin/}" "$remote";
  done
}

echo "cloning repository into ... $2"
clone "$1" "$2"
