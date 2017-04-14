/*
 * unparse.c -- convert a UUID to string
 * 
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>

#include "uuid.h"

static void uuid_unpack(uuid_t in, struct uuid *uu)
{
        uint8_t    *ptr = in;
        uint32_t   tmp;

        tmp = *ptr++;
        tmp = (tmp << 8) | *ptr++;
        tmp = (tmp << 8) | *ptr++;
        tmp = (tmp << 8) | *ptr++;
        uu->time_low = tmp;

        tmp = *ptr++;
        tmp = (tmp << 8) | *ptr++;
        uu->time_mid = tmp;
        
        tmp = *ptr++;
        tmp = (tmp << 8) | *ptr++;
        uu->time_hi_and_version = tmp;

        tmp = *ptr++;
        tmp = (tmp << 8) | *ptr++;
        uu->clock_seq = tmp;

        memcpy(uu->node, ptr, 6);
}

void uuid_unparse(uuid_t uu, char *out)
{
        struct uuid uuid;

        uuid_unpack(uu, &uuid);
        sprintf(out,
                "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                uuid.time_low, uuid.time_mid, uuid.time_hi_and_version,
                uuid.clock_seq >> 8, uuid.clock_seq & 0xFF,
                uuid.node[0], uuid.node[1], uuid.node[2],
                uuid.node[3], uuid.node[4], uuid.node[5]);
}
