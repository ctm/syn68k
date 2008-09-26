#! /bin/bash

set -o errexit -o nounset -o noclobber 

function die ()
{
    echo $* 1>&2
    exit 1
}

function main () 
{

  local test_count
  local crc
  local noncc
  local notnative

  for ((test_count=1; test_count<1000000000; test_count*=10)); do
    for crc in "" "-crc"; do
      for noncc in "" "-noncc"; do
	for notnative in "" "-notnative"; do
	  #echo $crc $noncc $notnative > /tmp/pg.$$
	  ./tst-i486-linux-glibc.PROBABLY_GOOD $test_count $crc $noncc $notnative >| /tmp/pg.$$
	  ./tst-i486-linux-glibc $test_count $crc $noncc $notnative >| /tmp/br.$$
	  diff /tmp/pg.$$ /tmp/br.$$ || die $test_count $crc $noncc $notnative
	done
      done
    done
  done
    
}

main "$@" 
