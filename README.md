# sj

Simple jabber client based on ideas from "Irc It".

Official website: http://klemkow.net/sj.html

Paper of the [slcon2](http://suckless.org/conference/): http://klemkow.net/sj.pdf

## Goals

The XMPP protocol is a monster and totally over engineered.  But you have to
deal with it cause of is wide usages and good features.  To beat this monster
this project try to dived it intro smaller parts and to create one program
to handle one aspect of XMPP.

The program "sj" just do a few things:

  * opens a connection with an XMPP server
  * do authentication (+binding +session registration)
  * perfoms keep-alive pings to the server
  * segmenting tags and routing them to other daemons:
    * presenced
    * messaged
    * iqd

## usage

```sh
# set base directory
export SJ_DIR=/home/user/.xmpp

# start daemon
sj -u user -s server.org -r resources &
password:

# set presence to 'online'
presence

# add a contact to your roster
roster -a other@server.com -n joe

# subscribe his online status
presence -to other@server.com subscribe

# let him see your online status
presence -to other@server.com subscribed

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
  * write manpages for all tools
  * messaged: convert xml-tags into entities
  * create of accounts in-band
  * change passwords

  * XEP-0077: In-Band Registration
  * XEP-0030: Service Discovery
  * XEP-0012: Last Activity
  * XEP-0202: Entity Time

## required

  * mxml 2.8
  * libbsd (for GNU-systems like Linux)

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

 * git clone https://github.com/younix/sj.git
 * cd sj
 * git submodule init
 * git submodule update
 * make

## plugins

You should be able to write plugins for this client in any language of
your choice.  The interface are just plain text files.
