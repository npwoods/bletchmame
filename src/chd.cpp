/***************************************************************************

	chd.cpp

	Limited support for MAME's CHD file format (nothing more than extracting
	SHA-1 hashes)

***************************************************************************/

// bletchmame headers
#include "chd.h"

// Qt headers
#include <QIODevice>
#include <QtEndian>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	struct ChdHeader
	{
		std::array<char, 8>		tag;			// 'MComprHD'
		quint32_be				length;			// length of header (including tag and length fields)
		quint32_be				version;		// drive format version
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  getHashForChd
//-------------------------------------------------

std::optional<Hash> getHashForChd(QIODevice &stream)
{
	union
	{
		ChdHeader	hdr;
		char		buffer[256];
	} u;

	// try to read headers
	qint64 total = stream.read((char *)&u, sizeof(u));
	if (total < sizeof(u.hdr))
		return { };

	// check the tag
	std::array<char, 8> magic = { 'M', 'C', 'o', 'm', 'p', 'r', 'H', 'D' };
	if (u.hdr.tag != magic)
		return { };

	// check the length, and ensure that we read enough
	size_t sha1Offset;
	switch (u.hdr.version)
	{
	case 3:
		// V3 header:
		// ...
		// [80] uint8_t  sha1[20];      // SHA1 checksum of raw data
		sha1Offset = 80;
		break;

	case 4:
		// V4 header:
		// ...
		// [ 48] uint8_t  sha1[20];      // combined raw+meta SHA1
		sha1Offset = 48;
		break;

	case 5:
		// V5 header:
		// ...
		// [ 84] uint8_t  sha1[20];      // combined raw+meta SHA1
		sha1Offset = 84;
		break;

	default:
		return { };
	}

	// pluck the pertinent data out if we can
	std::array<uint8_t, 20> sha1;
	if (sha1Offset + sizeof(sha1) > (size_t)total)
		return { };
	memcpy(&sha1, &u.buffer[sha1Offset], sizeof(sha1));

	// and return the hash
	return Hash(sha1);
}
