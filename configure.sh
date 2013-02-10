#!/bin/bash
#
conf () {
  pushd $1
  if [ -f ./configure ]; then
          ./configure $2
  else
          autoreconf -i -m
          ./configure $2
  fi
  if [ $? != 0 ]
  then
          echo build failed
          exit
  fi
  popd
}

conf libdwarf/libdwarf
conf libunwind

