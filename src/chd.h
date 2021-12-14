/***************************************************************************

	chd.h

	Limited support for MAME's CHD file format (nothing more than extracting
	SHA-1 hashes)

***************************************************************************/

#ifndef CHD_H
#define CHD_H

#include "hash.h"


//**************************************************************************
//  INTERFACE
//**************************************************************************

std::optional<Hash> getHashForChd(QIODevice &stzream);


#endif // CHD_H
