/*
 * uuid.h -- private header file for uuids
 * 
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

typedef unsigned char uuid_t[16];

/*
 * Offset between 15-Oct-1582 and 1-Jan-70
 */
#define TIME_OFFSET_HIGH 0x01B21DD2
#define TIME_OFFSET_LOW  0x13814000

struct uuid {
    uint32_t   time_low;
    uint16_t   time_mid;
    uint16_t   time_hi_and_version;
    uint16_t   clock_seq;
    uint8_t    node[6];
};

extern void uuid_unparse(uuid_t uu, char *out);
