#! /bin/bash

# $Id: autogen.sh 65 2004-12-24 19:51:17Z ctm $

set -o errexit -o nounset -o noclobber

glibtoolize --force
aclocal
autoheader
automake --add-missing
autoconf
