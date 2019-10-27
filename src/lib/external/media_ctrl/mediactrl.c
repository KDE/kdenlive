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

#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#if defined(Q_OS_LINUX)
#include <asm/types.h>
#endif
#include <fcntl.h>
#include <unistd.h>

#include "mediactrl.h"

static char *_shuttle_name = (char *)"Shuttle";
static char *_jog_name = (char *)"Jog";

/*
        ShuttlePro v2 keys
*/
static struct media_ctrl_key mc_shuttle_pro_v2_keys[] = {
    {0x100, "Button 1", MEDIA_CTRL_F1},  {0x101, "Button 2", MEDIA_CTRL_F2},   {0x102, "Button 3", MEDIA_CTRL_F3},   {0x103, "Button 4", MEDIA_CTRL_F4},
    {0x104, "Button 5", MEDIA_CTRL_B1},  {0x105, "Button 6", MEDIA_CTRL_B2},   {0x106, "Button 7", MEDIA_CTRL_B3},   {0x107, "Button 8", MEDIA_CTRL_B4},
    {0x108, "Button 9", MEDIA_CTRL_B5},  {0x109, "Button 10", MEDIA_CTRL_B6},  {0x10a, "Button 11", MEDIA_CTRL_B7},  {0x10b, "Button 12", MEDIA_CTRL_B8},
    {0x10c, "Button 13", MEDIA_CTRL_B9}, {0x10d, "Button 14", MEDIA_CTRL_B10}, {0x10e, "Button 15", MEDIA_CTRL_B11}, {0, NULL, 0}};

/*
    ShuttlePro keys
*/
static struct media_ctrl_key mc_shuttle_pro_keys[] = {{0x100, "Button 1", MEDIA_CTRL_F1},  {0x101, "Button 2", MEDIA_CTRL_F2},
                                                      {0x102, "Button 3", MEDIA_CTRL_F3},  {0x103, "Button 4", MEDIA_CTRL_F4},
                                                      {0x104, "Button 5", MEDIA_CTRL_B4},  {0x105, "Button 6", MEDIA_CTRL_B2},
                                                      {0x106, "Button 7", MEDIA_CTRL_B1},  {0x107, "Button 8", MEDIA_CTRL_B3},
                                                      {0x108, "Button 9", MEDIA_CTRL_B5},  {0x109, "Button 10", MEDIA_CTRL_B6},
                                                      {0x10a, "Button 11", MEDIA_CTRL_B7}, {0x10b, "Button 12", MEDIA_CTRL_B8},
                                                      {0x10c, "Button 13", MEDIA_CTRL_B9}, {0, NULL, 0}};

/*
        ShuttleXPress keys
*/
static struct media_ctrl_key mc_shuttle_xpress_keys[] = {{0x104, "Button B1", MEDIA_CTRL_B1}, {0x105, "Button B2", MEDIA_CTRL_B2},
                                                         {0x106, "Button B3", MEDIA_CTRL_B3}, {0x107, "Button B4", MEDIA_CTRL_B4},
                                                         {0x108, "Button B5", MEDIA_CTRL_B5}, {0, NULL, 0}};

/*
        JLCooper MCS3 Keys
*/
static struct media_ctrl_key mc_jlcooper_mcs3_keys[] = {{0x107, "F1", MEDIA_CTRL_F1},
                                                        {0x101, "F2", MEDIA_CTRL_F2},
                                                        {0x105, "F3", MEDIA_CTRL_F3},
                                                        {0x102, "F4", MEDIA_CTRL_F4},
                                                        {0x103, "F5", MEDIA_CTRL_F5},
                                                        {0x104, "F6", MEDIA_CTRL_F6},
                                                        {0x10d, "W1", MEDIA_CTRL_B6},
                                                        {0x10e, "W2", MEDIA_CTRL_B4},
                                                        {0x100, "W3", MEDIA_CTRL_B2},
                                                        {0x106, "W4", MEDIA_CTRL_B1},
                                                        {0x110, "W5", MEDIA_CTRL_B3},
                                                        {0x111, "W6", MEDIA_CTRL_B5},
                                                        {0x115, "W7", MEDIA_CTRL_B7},
                                                        {0x116, "STICK_LEFT", MEDIA_CTRL_STICK_LEFT},
                                                        {0x113, "STICK_RIGHT", MEDIA_CTRL_STICK_RIGHT},
                                                        {0x114, "STICK_UP", MEDIA_CTRL_STICK_UP},
                                                        {0x112, "STICK_DOWN", MEDIA_CTRL_STICK_DOWN},
                                                        {0x10f, "Rewind", MEDIA_CTRL_REWIND},
                                                        {0x108, "Fast Forward", MEDIA_CTRL_FAST_FORWARD},
                                                        {0x109, "Stop", MEDIA_CTRL_STOP},
                                                        {0x10a, "Play", MEDIA_CTRL_PLAY},
                                                        {0x10b, "Record", MEDIA_CTRL_RECORD},
                                                        {0, NULL, 0}};

/*
        Griffin PowerMate
*/
static struct media_ctrl_key mc_powermate_keys[] = {{BTN_0, "Button", MEDIA_CTRL_B1}, {0, NULL, 0}};

/*
        X-Keys Jog/Shuttle
*/
static struct media_ctrl_key mc_x_keys[] = {{0x102, "Button L1", MEDIA_CTRL_F1},
                                            {0x103, "Button L2", MEDIA_CTRL_F9},
                                            {0x104, "Button L3", MEDIA_CTRL_B1},
                                            {0x105, "Button L4", MEDIA_CTRL_B3},
                                            {0x106, "Button L5", MEDIA_CTRL_B5},
                                            {0x10a, "Button L6", MEDIA_CTRL_F2},
                                            {0x10b, "Button L7", MEDIA_CTRL_F10},
                                            {0x10c, "Button L8", MEDIA_CTRL_B2},
                                            {0x10d, "Button L9", MEDIA_CTRL_B4},
                                            {0x10e, "Button L10", MEDIA_CTRL_B6},
                                            {0x112, "Button C1", MEDIA_CTRL_F3},
                                            {0x11a, "Button C2", MEDIA_CTRL_F4},
                                            {0x122, "Button C3", MEDIA_CTRL_F5},
                                            {0x12a, "Button C4", MEDIA_CTRL_F6},
                                            {0x113, "Button C5", MEDIA_CTRL_F11},
                                            {0x11b, "Button C6", MEDIA_CTRL_F12},
                                            {0x123, "Button C7", MEDIA_CTRL_F13},
                                            {0x12b, "Button C8", MEDIA_CTRL_F14},
                                            {0x132, "Button R1", MEDIA_CTRL_F7},
                                            {0x133, "Button R2", MEDIA_CTRL_F15},
                                            {0x134, "Button R3", MEDIA_CTRL_B7},
                                            {0x135, "Button R4", MEDIA_CTRL_B9},
                                            {0x136, "Button R5", MEDIA_CTRL_B11},
                                            {0x13a, "Button R6", MEDIA_CTRL_F8},
                                            {0x13b, "Button R7", MEDIA_CTRL_F16},
                                            {0x13c, "Button R8", MEDIA_CTRL_B8},
                                            {0x13d, "Button R9", MEDIA_CTRL_B10},
                                            {0x13e, "Button R10", MEDIA_CTRL_B12},
                                            {0, NULL, 0}};

struct media_ctrl_key *media_ctrl_get_key(struct media_ctrl *ctrl, int code, int *index)
{
    int i = 0;
    struct media_ctrl_key *keys = ctrl->device->keys;

    while (keys[i].key != 0) {
        if (keys[i].key == code) {
            if (index != NULL) *index = i;
            return &keys[i];
        }
        i++;
    }

    return NULL;
}

int media_ctrl_get_keys_count(struct media_ctrl *ctrl)
{
    int i = 0;
    struct media_ctrl_key *keys = ctrl->device->keys;

    while (keys[i].key != 0) {
        i++;
    }

    return i;
}

void translate_contour_hid_event(struct media_ctrl *ctrl, struct input_event *ev, struct media_ctrl_event *me)
{
    me->type = 0;

    if (ev->type == EV_REL) {
        int cv;
        /* First check the outer dial */
        if (ev->code == REL_WHEEL) {

            cv = (signed int)ev->value;
            if (cv == 1 || cv == -1) cv = 0;

            if (cv == ctrl->lastshu) return;
            ctrl->lastshu = cv;

            /* TODO: review this change */
            if (cv > 0) cv -= 1;
            if (cv < 0) cv += 1;

            // printf("Shuttle: %d\n", cv);
            me->type = MEDIA_CTRL_EVENT_SHUTTLE;
            me->value = cv * 2;
            me->name = _shuttle_name;

        } else if (ev->code == REL_DIAL) {
            int lv;

            if (ctrl->lastval == -1) ctrl->lastval = ev->value;
            lv = ctrl->lastval;
            cv = ev->value;

            if (lv == cv) return;

            ctrl->lastval = cv;

            if (cv < 10 && lv > 0xF0) cv += 0x100;
            if (lv < 10 && cv > 0xF0) lv += 0x100;

            me->type = MEDIA_CTRL_EVENT_JOG;
            me->value = cv - lv;
            me->name = _jog_name;

            ctrl->jogpos += me->value;
            // printf("Jog: %06ld (%d)\n", ctrl->jogpos, me->value);
        }
        return;
    }
    if (ev->type == EV_KEY) {
        int index;
        struct media_ctrl_key *key = media_ctrl_get_key(ctrl, ev->code, &index);
        if (key == NULL) return;

        me->type = MEDIA_CTRL_EVENT_KEY;
        me->code = key->code;
        me->value = ev->value;
        me->name = (char *)key->name;
        me->index = index;

        // printf("Key: %04x %02x: %s\n", ev->code, ev->value, key->name);
    }
}

void translate_compliant(struct media_ctrl *ctrl, struct input_event *ev, struct media_ctrl_event *me)
{
    me->type = 0;

    // printf("Translate %02x %02x\n", ev->type, ev->code );

    if (ev->type == EV_REL) {
        if (ev->code == REL_DIAL) {

            me->type = MEDIA_CTRL_EVENT_JOG;
            me->value = (signed int)ev->value;
            me->name = _jog_name;

            ctrl->jogpos += me->value;
            // printf("Jog: %06ld (%d)\n", ctrl->jogpos, me->value);
        }
        return;
    }
    if (ev->type == EV_ABS) {
        // printf("ABS\n" );
        if (ev->code == 0x1c || ev->code == ABS_THROTTLE) {
            // printf("ABS_MISC\n" );
            me->type = MEDIA_CTRL_EVENT_SHUTTLE;
            me->value = (signed int)ev->value;
            me->name = _shuttle_name;

            ctrl->shuttlepos = me->value;
            // printf("Shuttle: %06d (%d)\n", ctrl->shuttlepos, me->value);
        }
    } else if (ev->type == EV_KEY) {
        int index;
        struct media_ctrl_key *key = media_ctrl_get_key(ctrl, ev->code, &index);
        if (key == NULL) return;

        me->type = MEDIA_CTRL_EVENT_KEY;
        me->code = key->code;
        me->value = ev->value;
        me->name = (char *)key->name;
        me->index = index;

        // printf("Key: %04x %02x: %s\n", ev->code, ev->value, key->name);
    }
}

struct media_ctrl_device supported_devices[] = {
    {0x0b33, 0x0030, "Contour Design ShuttlePRO v2", mc_shuttle_pro_v2_keys, translate_contour_hid_event},
    {0x0b33, 0x0020, "Contour Design ShuttleXpress", mc_shuttle_xpress_keys, translate_contour_hid_event},
    {0x0b33, 0x0010, "Contour Design ShuttlePro", mc_shuttle_pro_keys, translate_contour_hid_event},
    {0x0b33, 0x0011, "Contour Design ShuttlePro", mc_shuttle_pro_keys, translate_contour_hid_event}, /* Hercules OEM */
    {0x05f3, 0x0240, "Contour Design ShuttlePro", mc_shuttle_pro_keys, translate_contour_hid_event},
    {0x0760, 0x0001, "JLCooper MCS3", mc_jlcooper_mcs3_keys, translate_compliant},
    {0x077d, 0x0410, "Griffin PowerMate", mc_powermate_keys, translate_compliant},
    {0x05f3, 0x0241, "X-Keys Editor", mc_x_keys, translate_contour_hid_event},
    {0, 0, 0, 0, 0}};

void media_ctrl_read_event(struct media_ctrl *ctrl, struct media_ctrl_event *me)
{
    ssize_t n;
    struct input_event ev;

    // struct media_ctrl_event me;

    if (ctrl->fd > 0) {
        n = read(ctrl->fd, &ev, sizeof(ev));
    } else {
        return;
    }

    if (n != sizeof(ev)) {
        // printf("JogShuttle::inputCallback: read: (%d) %s\n", errno, strerror(errno));
        close(ctrl->fd);
        ctrl->fd = -1;
        return;
    }

    if (ctrl->device && ctrl->device->translate)
        ctrl->device->translate(ctrl, &ev, me);
    else
        me->type = 0;

    if (me->type == MEDIA_CTRL_EVENT_JOG) {
        struct timeval timev;
        gettimeofday(&timev, NULL);
        unsigned long now = (unsigned long)timev.tv_usec + (1000000 * (unsigned long)timev.tv_sec);
        if (now < ctrl->last_jog_time + 40000) {
            // printf("*** Fast Jog %02d %05d ***\n", me->value, now - ctrl->last_jog_time);
            ctrl->jogrel = me->value;
            me->type = MEDIA_CTRL_EVENT_NONE;
        } else {
            me->value += ctrl->jogrel;
            ctrl->jogrel = 0;
            ctrl->last_jog_time = now;
            // printf("*** Jog %02d ***\n", me->value);
        }
    }
}

int probe_device(struct media_ctrl *mc)
{
    short devinfo[4];
    int i = 0;

    if (ioctl(mc->fd, EVIOCGID, &devinfo)) {
        perror("evdev ioctl");
        return 0;
    }

    do {
        if (supported_devices[i].vendor == devinfo[1] && supported_devices[i].product == devinfo[2]) {

            mc->device = &supported_devices[i];
            // printf("Success on /dev/input/event%d: %s\n", mc->eventno, mc->device->name);
            // mc->fd = fd;
            // mc->translate = mc->device.translate_function;
            // mc = malloc(sizeof(struct media_ctrl));
            mc->jogpos = 0;
            mc->lastval = -1;
            mc->last_jog_time = 0;
            return 1;
        }
        // mc->device = NULL;

    } while (supported_devices[++i].vendor != 0);

    return 0;
}

void find_first_device(struct media_ctrl *mc)
{
    char buf[256];
    int i;

    for (i = 0; i < 32; i++) {
        sprintf(buf, "/dev/input/event%d", i);
        int fd = open(buf, O_RDONLY);
        if (fd < 0) {
            perror(buf);
        } else {
            mc->fd = fd;
            mc->eventno = i;
            if (probe_device(mc)) {
                return;
            }
            close(fd);
            mc->fd = -1;
        }
    }
}

void media_ctrl_close(struct media_ctrl *mc)
{
    if (mc->fd > 0) close(mc->fd);
    memset(mc, 0, sizeof(struct media_ctrl));
}

void media_ctrl_open(struct media_ctrl *mc)
{
    find_first_device(mc);
}

void media_ctrl_open_dev(struct media_ctrl *mc, const char *devname)
{
    int fd;

    fd = open(devname, O_RDONLY);
    if (fd < 0) {
        perror(devname);
        mc->fd = -1;
    } else {
        mc->fd = fd;
        // mc->eventno = i;
        if (probe_device(mc)) {
            return;
        }
        close(fd);
        mc->fd = -1;
    }
}
