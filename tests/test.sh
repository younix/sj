#!/usr/bin/env ksh

. ./tap-functions -u

plan_tests 6

# prepare

iqd="../iqd"
messaged="../messaged"

tmpdir=$(mktemp -d sj_tests_XXXXXX)

#
# iqd tests
#

$iqd -d $tmpdir < iq.xml
ok $? "iqd starting and ending"

test -s "$tmpdir/1234"
ok $? "iqd tag delivery"

#
# messaged tests
#

$messaged -j "me@server.org" -d $tmpdir < message.xml
ok $? "messaged stating and ending"

test -d "$tmpdir/alice@server.org"
ok $? "messaged create folder"

test -s "$tmpdir/alice@server.org/out"
ok $? "messaged create out file"

test -p "$tmpdir/alice@server.org/in"
ok $? "messaged create in file as named pipe"

# clean up
rm -rf $tmpdir

# vim: set spell spelllang=en:
