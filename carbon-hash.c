/*
 *  This file is part of carbon-c-relay.
 *
 *  dnspq is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  dnspq is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with dnspq.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>

#include "carbon-hash.h"

/**
 * Computes the hash position for key in a 16-bit unsigned integer
 * space.  Returns a number between 0 and 65535 based on the highest 2
 * bytes of the MD5 sum of key.
 */
unsigned short carbon_hashpos(const char *key) {
	unsigned char md5[MD5_DIGEST_LENGTH];

	MD5((unsigned char *)key, strlen(key), md5);

	return ((md5[0] << 8) + md5[1]);
}

//int (*compar)(const void *, const void *)
static int entrycmp(const void *l, const void *r) {
	return ((carbon_ring *)l)->pos - ((carbon_ring *)r)->pos;
}

/**
 * Computes the hash positions for the server name given.  This is based
 * on the hashpos function.  The server name usually is the IPv4
 * address.  The port component is just stored and not used in the hash
 * calculation.  Returns an updated ring, or a new ring if none given.
 */
carbon_ring *carbon_addnode(
		carbon_ring *ring,
		const char *host,
		const unsigned short port)
{
	int i;
	char buf[256];
	carbon_ring *entries = (carbon_ring *)malloc(sizeof(carbon_ring) * CARBON_REPLICAS);

	if (entries == NULL)
		return NULL;

	for (i = 0; i < CARBON_REPLICAS; i++) {
		/* this format is actually Python's tuple format that is used in
		 * serialised form as input for the hash */
		snprintf(buf, sizeof(buf), "('%s', None):%d", host, i);
		entries[i].pos = carbon_hashpos(buf);
		entries[i].server = host;
		entries[i].port = port;
		entries[i].next = NULL;
	}

	qsort(entries, CARBON_REPLICAS, sizeof(carbon_ring), *entrycmp);

	if (ring == NULL) {
		for (i = 1; i < CARBON_REPLICAS; i++)
			entries[i - 1].next = &entries[i];
		ring = entries;
	} else {
		/* merge-join the two rings */
		carbon_ring *w, *last;
		i = 0;
		for (w = ring, last = NULL; w != NULL && i < CARBON_REPLICAS; ) {
			if (w->pos < entries[i].pos) {
				last = w;
				w = w->next;
			} else {
				entries[i].next = w;
				if (last == NULL) {
					ring = &entries[i];
				} else {
					last->next = &entries[i];
				}
				last = &entries[i];
				i++;
			}
		}
		if (w != NULL) {
			last->next = w;
		} else {
			last->next = &entries[i];
			for (i = i + 1; i < CARBON_REPLICAS; i++)
				entries[i - 1].next = &entries[i];
		}
	}

	return ring;
}
