/* readline.h --- Simple implementation of readline.
   Copyright (C) 2005 Free Software Foundation, Inc.
   Written by Simon Josefsson

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

#ifndef GL_READLINE_H
#define GL_READLINE_H

#if HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#else
extern char *readline (const char *prompt);
#endif

#endif /* GL_READLINE_H */