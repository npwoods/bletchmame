/***************************************************************************

	hash.h

	CRC32/SHA-1 hash handling

***************************************************************************/

// bletchmame headers
#include "hash.h"

// Qt headers
#include <QCryptographicHash>
#include <QIODevice>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

std::array<quint32, 256> Hash::s_crcTable = calculateCrc32Table();

//-------------------------------------------------
//  ctor
//-------------------------------------------------

Hash::Hash(std::optional<std::uint32_t> crc32, const QByteArray &sha1)
	: m_crc32(crc32)
{
	m_sha1.emplace();
	assert(sha1.size() == m_sha1->size());
	for (size_t i = 0; i < std::min((size_t)sha1.size(), m_sha1->size()); i++)
		(*m_sha1)[i] = sha1[i];
}


//-------------------------------------------------
//  calculate - calculates CRC32 and SHA-1
//-------------------------------------------------

std::optional<Hash> Hash::calculate(QIODevice &stream, const CalculateCallback &callback)
{
	// sanity check
	if (!callback)
		throw false;

	// setup
	std::uint32_t crc32 = ~0;
	std::uint64_t bytesProcessed = 0;

	// Qt has SHA-1 functionality
	QCryptographicHash cryptographicHash(QCryptographicHash::Algorithm::Sha1);

	while (!stream.atEnd())
	{
		// read into a buffer
		char buffer[15000];
		int len = (int)stream.read(buffer, std::size(buffer));
		if (len <= 0)
			break;
		bytesProcessed += len;

		// CRC32 processing
		for (int i = 0; i < len; i++)
			crc32 = s_crcTable[(crc32 ^ buffer[i]) & 0xFF] ^ (crc32 >> 8);

		// SHA-1 processing
#if QT_VERSION < 0x060300
		cryptographicHash.addData(buffer, len);
#else // !QT_VERSION < 0x060300
		cryptographicHash.addData(QByteArrayView(buffer, len));
#endif // QT_VERSION < 0x060300

		// invoke callback if appropriate
		if (callback(bytesProcessed))
			return { };
	}

	// we're done; return the right results
	crc32 ^= ~0;
	return Hash(crc32, cryptographicHash.result());
}


//-------------------------------------------------
//  toString
//-------------------------------------------------

QString Hash::toString() const
{
	QString result;
	if (m_crc32)
		result += QString("CRC(%1) ").arg(QString::number(*m_crc32, 16).rightJustified(8, '0'));

	if (m_sha1)
		result += QString("SHA1(%1)").arg(hexString(&(*m_sha1)[0], std::size(*m_sha1)));

	return result.trimmed();
}


//-------------------------------------------------
//  mask
//-------------------------------------------------

Hash Hash::mask(bool useCrc32, bool useSha1) const
{
	return Hash(
		useCrc32 ? crc32() : std::nullopt,
		useSha1 ? sha1() : std::nullopt);
}


//-------------------------------------------------
//  hexString
//-------------------------------------------------

QString Hash::hexString(const void *ptr, size_t sz)
{
	const std::uint8_t *bytePtr = (const std::uint8_t *)ptr;
	QString result;
	for (size_t i = 0; i < sz; i++)
		result += QString::number(bytePtr[i], 16).rightJustified(2, '0');
	return result;
}


//-------------------------------------------------
//  calculateCrc32Table
//-------------------------------------------------

std::array<quint32, 256> Hash::calculateCrc32Table()
{
	std::array<quint32, 256> result;

	// initialize CRC table
	for (int i = 0; i < std::size(result); i++)
	{
		quint32 crc = i;
		for (int j = 0; j < 8; j++)
			crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;

		result[i] = crc;
	}

	return result;
}


