#ifndef DOMID_HEADER_INCLUDED
#define DOMID_HEADER_INCLUDED 1

#include <autoconf/trader_outside_kernel.h>

#define DOM_NEMESIS		0xD000000000000000ULL
#ifdef CONFIG_TRADER_OUTSIDE_KERNEL
#define DOM_TRADER		(DOM_NEMESIS + 1)
#define DOM_SERIAL		(DOM_NEMESIS + 2)
#define DOM_USER		(DOM_NEMESIS + 3)
#else
#define DOM_SERIAL		(DOM_NEMESIS + 1)
#define DOM_USER		(DOM_NEMESIS + 2)
#endif

#endif

