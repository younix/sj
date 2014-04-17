# sj

Simple jabber client based on ideas from "Irc It".

## Goals

The XMPP protocol is a monster and totally over engineered.  But you have to
deal with it cause of is wide usages and good features.  To beat this monster
this project try to dived it intro smaller parts and to create one program
to handle one aspect of XMPP.

The program "sj" just do a few things:

  * opens a connection with an XMPP server
  * do authentication (+binding +session registration)
  * perfoms keep-alive pings to the server
  * segmenting tags and routing them to other software daemons:
    * presenced
    * messaged
    * iqd

## TODO

  * replace socket-handling with UCSPI
  * add SSL support
  * designing interface for backend programs
  * replace linked list with one of queue.h

## required

  * mini-xml-lib

## tested on

 * OpenBSD
 * MacOSX

## plugins

You should be able to write plugins for this client in any language of
your choice.  The interface are just plain text files.
