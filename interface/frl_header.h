/*
 * frl_header.h
 *
 * $Header: /afs/cern.ch/project/cvs/reps/tridas/TriDAS/daq/interface/shared/include/interface/shared/frl_header.h,v 1.4 2007/11/23 10:40:08 lorsini Exp $
 *
 * This file contains the struct definition of the FRL-header.
 * The FRL-header is inserted by the FRL as it cuts the
 * variable size event record from the FED into fixed size
 * blocks (aka segments).
 * The same header definition is subsequently used by the
 *  FEDbuilder as it builts super-fragments.
 *
 * Note that the FRL fills the header fields in big-endian.
 *
 *
 * mods:
 * 18-May-2004 FM initial version into TriDAS/daq/interface/shared/include
 * 26-Sep-2007 S. Murray: Replaced integer types with those from stdint.h
 */

#ifndef _FRL_HEADER_H
#define _FRL_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

// this include for types uint_32t definition
#include <stdint.h>

/*************************************************************************
 *
 * data structures and associated typedefs
 *
 *************************************************************************/

/*
 * FRL header - in front of each segment
 */

typedef struct frlh_struct {
	uint32_t source;
	uint32_t trigno; /* better name is event number */
	uint32_t segno; /* starts counting from 0 onwards */
	uint32_t reserved;
	uint32_t segsize; /* segment size in bytes - not including the segment header itself
	 * higher order bits are used as flags */
	uint32_t reserved_2;
} frlh_t;

#define FRL_SEGSIZE_MASK  0x00FFFFFF

#define FRL_FLUSHED_BLOCK             0x20000000
#define FRL_LAST_SEGM                 0x80000000
#define FRL_LAST_SEGM_CRC_INVALID     0x40000000
#define FRL_LAST_SEGM_FED_CRC_INVALID 0x10000000

#ifdef __cplusplus
}
#endif

#endif  /* _FRL_HEADER_H */

