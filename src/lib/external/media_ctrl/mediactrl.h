/*
 * mediactrl.c -- Jog Shuttle device support
 * Copyright (C) 2001-2007 Dan Dennedy <dan@dennedy.org>
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MEDIACTRL_H
#define MEDIACTRL_H

#include <linux/input.h>
#include <sys/time.h>

// just to make the code more readable
#define KEY_RELEASE 0x00
#define KEY_PRESS 0x01

// not used yet
#define MEDIA_ST_ACTIVE 0x02
#define MEDIA_ST_INACTIVE 0x01

// media ctrl event types
#define MEDIA_CTRL_EVENT_NONE 0x00
#define MEDIA_CTRL_EVENT_KEY 0x01
#define MEDIA_CTRL_EVENT_JOG 0x02
#define MEDIA_CTRL_EVENT_SHUTTLE 0x03
#define MEDIA_CTRL_EVENT_STICK 0x04

// the disconnect event - not used yet
#define MEDIA_CTRL_DISCONNECT 0x01

#define MEDIA_CTRL_SHIFT 0x01

#define MEDIA_CTRL_PLAY 0x10
#define MEDIA_CTRL_PLAY_FWD 0x10
#define MEDIA_CTRL_REVERSE 0x11
#define MEDIA_CTRL_PLAY_REV 0x11
#define MEDIA_CTRL_STOP 0x12
#define MEDIA_CTRL_PAUSE 0x13
#define MEDIA_CTRL_NEXT 0x14
#define MEDIA_CTRL_PREV 0x15
#define MEDIA_CTRL_RECORD 0x16
#define MEDIA_CTRL_FAST_FORWARD 0x17
#define MEDIA_CTRL_REWIND 0x18

#define MEDIA_CTRL_STICK_LEFT 0x20
#define MEDIA_CTRL_STICK_RIGHT 0x21
#define MEDIA_CTRL_STICK_UP 0x22
#define MEDIA_CTRL_STICK_DOWN 0x23

/* function keys, usually at top of device */
#define MEDIA_CTRL_F1 0x100
#define MEDIA_CTRL_F2 0x101
#define MEDIA_CTRL_F3 0x102
#define MEDIA_CTRL_F4 0x103
#define MEDIA_CTRL_F5 0x104
#define MEDIA_CTRL_F6 0x105
#define MEDIA_CTRL_F7 0x106
#define MEDIA_CTRL_F8 0x107
#define MEDIA_CTRL_F9 0x108
#define MEDIA_CTRL_F10 0x109
#define MEDIA_CTRL_F11 0x10a
#define MEDIA_CTRL_F12 0x10b
#define MEDIA_CTRL_F13 0x10c
#define MEDIA_CTRL_F14 0x10d
#define MEDIA_CTRL_F15 0x10e
#define MEDIA_CTRL_F16 0x10f

#define MEDIA_CTRL_B1 0x110
#define MEDIA_CTRL_B2 0x111
#define MEDIA_CTRL_B3 0x112
#define MEDIA_CTRL_B4 0x113
#define MEDIA_CTRL_B5 0x114
#define MEDIA_CTRL_B6 0x115
#define MEDIA_CTRL_B7 0x116
#define MEDIA_CTRL_B8 0x117
#define MEDIA_CTRL_B9 0x118
#define MEDIA_CTRL_B10 0x119
#define MEDIA_CTRL_B11 0x11a
#define MEDIA_CTRL_B12 0x11b
#define MEDIA_CTRL_B13 0x11c
#define MEDIA_CTRL_B14 0x11d
#define MEDIA_CTRL_B15 0x11e
#define MEDIA_CTRL_B16 0x11f

#ifdef __cplusplus
extern "C" {
#endif

struct media_ctrl_device;

struct media_ctrl_key
{
    int key; // internal keycode - do not use
    const char *name;
    int code; // eventcode
    //	int action;
};

struct media_ctrl_event
{
    struct timeval time;
    unsigned short type;
    unsigned short code;
    char *name;
    int value;
    unsigned short index;
};

struct media_ctrl
{

    int fd;
    int eventno;

    int status;

    struct media_ctrl_device *device;

    long jogpos;
    int shuttlepos;

    int lastval;
    int lastshu;

    int jogrel;                  // accumulate relative values if events come too fast
    unsigned long last_jog_time; // last jog event
};

struct media_ctrl_device
{
    int vendor;
    int product;
    const char *name;
    struct media_ctrl_key *keys;
    void (*translate)(struct media_ctrl *ctrl, struct input_event *ev, struct media_ctrl_event *me);
};

void media_ctrl_open_dev(struct media_ctrl *, const char *devname);
void media_ctrl_close(struct media_ctrl *);
void media_ctrl_read_event(struct media_ctrl *, struct media_ctrl_event *);

struct media_ctrl_key *media_ctrl_get_keys(struct media_ctrl *);
int media_ctrl_get_keys_count(struct media_ctrl *);

#ifdef __cplusplus
}
#endif

#endif
