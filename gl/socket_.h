/* Provide a sys/socket header file for systems lacking it (read: mingw32).
   Copyright (C) 2005, 2006 Free Software Foundation, Inc.
   Written by Simon Josefsson.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

/* This file is supposed to be used on platforms that lack
   sys/socket.h.  It is intended to provide definitions and prototypes
   needed by an application.

   Currently only mingw32 is supported, which has the header files
   winsock2.h and ws2tcpip.h that declare the sys/socket.h definitions
   we need. */

#if HAVE_WINSOCK2_H
# include <winsock2.h>
#endif
#if HAVE_WS2TCPIP_H
# include <ws2tcpip.h>
#endif

/* For shutdown(). */
#if !defined(SHUT_RD) && defined (SD_RECEIVE)
# define SHUT_RD SD_RECEIVE
#endif
#if !defined(SHUT_WR) && defined (SD_SEND)
# define SHUT_WR 1
#endif
#if !defined(SHUT_RDWR) && defined (SD_BOTH)
# define SHUT_RDWR 2
#endif

#endif /* _SYS_SOCKET_H */