# sj

Simple jabber client based on ideas from "Irc It".

Official website: http://klemkow.net/sj.html

Paper of the [slcon2](http://suckless.org/conference/): http://klemkow.net/sj.pdf

## Goals

The XMPP protocol is a monster and totally overengineered.  But you have to
deal with it because of its widespread use and good features.  To beat this monster,
this project tries to divide it into smaller parts and to create one program
to handle one aspect of XMPP.

The program "sj" just does a few things:

  * opens a connection to an XMPP server
  * do authentication (+binding +session registration)
  * perfoms keep-alive pings to the server
  * segmenting tags and routing them to other daemons:
    * presenced
    * messaged
    * iqd

## requires

  * mxml 2.8 or 3.x
  * libbsd (for GNU-systems like Linux)
  * [ucspi-tcp](http://cr.yp.to/ucspi-tcp.html)
  * [ucspi-tools](https://github.com/younix/ucspi)

## usage

```sh
# set base directory
export SJ_DIR=/home/user/.xmpp

# start daemon
tcpclient server.org 5222 sj -u user -s server.org -r sj &
password:

# set presence to 'online'
presence

# add a contact to your roster
roster -a other@server.com -n joe

# subscribe his online status
presence -t other@server.com subscribe

# let him see your online status
presence -t other@server.com subscribed

# view buddies on your roster
roster
other@server.org                both    joe
```

## TODO

  * ~~replace socket-handling with UCSPI~~
  * ~~add SSL support~~
  * ~~designing interface for backend programs~~
  * ~~replace linked list with one of queue.h~~
  * write front end tools
    * web front end for mobile chatting
  * write manpages for all tools
  * messaged
    * (de)escape messaged xml save
    * detect filesystem changes
  * create of accounts in-band
  * change passwords
  * integrations into a service management solution like svc of djb

  * XEP-0077: In-Band Registration
  * XEP-0030: Service Discovery
  * XEP-0012: Last Activity
  * XEP-0202: Entity Time

## tested with

 * OpenBSD
   * gcc 4.2.1
   * gcc 4.8.2
   * clang 3.5
 * MacOSX
 * GNU/Linux
   * gcc (Debian 4.7.2-5)
   * clang (Debian 3.0-6.2)

## build

```sh
git clone https://github.com/younix/sj.git
cd sj
git submodule init
git submodule update
make
```

## plugins

You should be able to write plugins for this client in any language of
your choice.  The interface are just plain text files.

## front ends

See [sj tools](https://github.com/GReagle/sjt) for some front ends.

## known issues

### POSIXLY_CORRECT for GNU getopt()

The following command will produce error "tlsc: invalid option" on systems
with GNU getopt()

`tcpclient server.org 5222 sj -u user -s server.org -r sj`

unless you set the environment variable POSIXLY_CORRECT.  See the
[issue for tlsc](https://github.com/younix/ucspi/issues/5) for more
details.  You can also get around this problem by using sj environment
variables (e.g. SJ_USER) instead of command line options.

### TLSC_NO_VERIFICATION

Currently, you need to set the environment variable `export
TLSC_NO_VERIFICATION=1` in order to avoid "tlsc: tls_error: ssl verify
setup failure".
