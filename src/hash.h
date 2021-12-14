/***************************************************************************

	hash.h

	CRC32/SHA-1 hash handling

***************************************************************************/

#ifndef HASH_H
#define HASH_H

#include <array>
#include <cstdint>
#include <QByteArray>


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class QIODevice;

// ======================> Hash

class Hash
{
public:
	class Test;

	Hash(std::optional<std::uint32_t> crc32 = { }, const std::optional<std::array<uint8_t, 20>> &sha1 = { })
		: m_crc32(crc32)
		, m_sha1(sha1)
	{
	}

	Hash(const std::optional<std::array<uint8_t, 20>> &sha1)
		: m_sha1(sha1)
	{
	}

	Hash(std::optional<std::uint32_t> crc32, const QByteArray &sha1);
	Hash(const Hash &) = default;
	Hash(Hash &&) = default;

	// operators
	Hash &operator=(const Hash &) = default;
	bool operator==(const Hash &) const = default;

	// methods
	QString toString() const;
	Hash mask(bool useCrc32, bool useSha1) const;

	// statics
	static Hash calculate(QIODevice &stream);

	// accessors
	const std::optional<std::uint32_t> &crc32() const			{ return m_crc32; }
	const std::optional<std::array<uint8_t, 20>> &sha1() const	{ return m_sha1; }

private:
	// static variables
	static std::array<quint32, 256>         s_crcTable;

	std::optional<std::uint32_t>			m_crc32;
	std::optional<std::array<uint8_t, 20>>	m_sha1;

	static std::array<quint32, 256> calculateCrc32Table();
	static QString hexString(const void *ptr, size_t sz);
};


#endif // HASH_H
