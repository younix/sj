#!/usr/bin/env ksh

export MALLOC_OPTIONS="S"

. ./tap-functions -u

plan_tests 12

# prepare

iqd="../iqd"
messaged="../messaged"
presenced="../presenced"

tmpdir=$(mktemp -d sj_tests_XXXXXX)

#
# iqd tests
#

# prepare
mkdir "$tmpdir/ext"
ln -s '../../../xmpp_time' "$tmpdir/ext/urn:xmpp:time"
touch "$tmpdir/in"

# start iqd with input
cat iq.xml | $iqd -d $tmpdir
ok $? "iqd starting and ending"

test -s "$tmpdir/1234"
ok $? "iqd tag delivery"

test -s "$tmpdir/in"
ok "$?" "xmpp:time response"

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

grep -q '^....-..-.. ..:.. <eve@server.org> $' "$tmpdir/eve@server.org/out"
ok $? "empty messages are accepted"

grep -q '^....-..-.. ..:.. <cari@server.org/.*> consectetur$' "$tmpdir/cari@server.org/out"
ok $? "message without active element is accepted"

#
# presenced tests
#

touch "$tmpdir/in"
mkdir "$tmpdir/user@host"
echo -n "test status" > "$tmpdir/user@host/mystatus"

$presenced -d $tmpdir < presence.xml
ok $? "presenced stating and ending"

test -s "$tmpdir/user@host/status"
ok $? "presenced create status file"

test -s "$tmpdir/in"
ok $? "presenced write status change into \"in\" file"

# clean up
rm -rf $tmpdir

# vim: set spell spelllang=en:
