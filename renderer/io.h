/*
 * io.h -- melt input/output
 * Copyright (C) 2002-2014 Meltytech, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _DEMO_IO_H_
#define _DEMO_IO_H_

#ifdef __cplusplus
extern "C"
{
#endif

extern char *chomp( char * );
extern char *trim( char * );
extern char *strip_quotes( char * );
extern char *get_string( char *, int, char * );
extern int *get_int( int *, int );
extern void term_init( );
extern int term_read( );
extern void term_exit( );
extern char get_keypress( );
extern void wait_for_any_key( char * );
extern void beep( );

#ifdef __cplusplus
}
#endif

#endif
