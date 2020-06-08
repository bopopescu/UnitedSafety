/* (c) Copyright 2014, Synapse Wireless Inc */

#ifndef _syn_types_h_
#define _syn_types_h_

/*
 * Data type definitions for SNAP
 * If stdint is not available on the target platform,
 * build with CUSTOM_PLATFORM_TYPES defined and provide
 * a custom_syn_types.h with the appropriate definitions.
 */

#ifndef CUSTOM_PLATFORM_TYPES

#include <stdint.h>

typedef uint8_t Snap_U8;
typedef int8_t Snap_S8;
typedef uint16_t Snap_U16;
typedef int16_t Snap_S16;
typedef uint32_t Snap_U32;
typedef int32_t Snap_S32;

#else

#include "custom_syn_types.h"

#endif

#endif /* _syn_types_h_ */
