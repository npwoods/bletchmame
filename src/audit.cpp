/***************************************************************************

    audit.cpp

    ROM/Asset auditing

***************************************************************************/

#include <QCryptographicHash>

#include "audit.h"
#include "assetfinder.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class Audit::Session
{
public:
	// ctor
	Session(const std::function<void(QString &&)> &callback);

	// accessors
	AuditStatus status() const { return m_status; }

	// methods
	void message(AuditStatus newStatus, QString &&message);

private:
	const std::function<void(QString &&)> &	m_callback;
	AuditStatus								m_status;
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

std::array<quint32, 256> Audit::s_crcTable = calculateCrc32Table();


//-------------------------------------------------
//  ctor
//-------------------------------------------------

Audit::Audit()
{
}


//-------------------------------------------------
//  calculateCrc32Table
//-------------------------------------------------

std::array<quint32, 256> Audit::calculateCrc32Table()
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


//-------------------------------------------------
//  calculateHashes - calculates CRC32 and SHA-1
//-------------------------------------------------

void Audit::calculateHashes(QIODevice &stream, std::uint32_t *crc32, QByteArray *sha1)
{
	// Qt has SHA-1 functionality
	std::optional<QCryptographicHash> cryptographicHash;
	if (sha1)
		cryptographicHash.emplace(QCryptographicHash::Algorithm::Sha1);

	if (crc32)
		*crc32 = ~0;
	while (!stream.atEnd())
	{
		// read into a buffer
		char buffer[15000];
		int len = (int) stream.read(buffer, std::size(buffer));

		// CRC32 processing
		if (crc32)
		{
			for (int i = 0; i < len; i++)
				*crc32 = s_crcTable[(*crc32 ^ buffer[i]) & 0xFF] ^ (*crc32 >> 8);
		}

		// SHA-1 processing
		if (cryptographicHash)
			cryptographicHash->addData(buffer, len);
	}

	// we're done; return the right results
	if (crc32)
		*crc32 ^= ~0;
	if (sha1 && cryptographicHash)
		*sha1 = cryptographicHash->result();
}


//-------------------------------------------------
//  byteArrayEquals
//-------------------------------------------------

template<std::size_t N>
bool Audit::byteArrayEquals(const QByteArray &byteArray, const std::array<std::uint8_t, N> &stdArray)
{
	return byteArray.size() == N
		&& !memcmp(byteArray.constData(), &stdArray[0], N);
}


//-------------------------------------------------
//  hexString
//-------------------------------------------------

QString Audit::hexString(const void *ptr, size_t sz)
{
	const std::uint8_t *bytePtr = (const std::uint8_t *)ptr;
	QString result;
	for (size_t i = 0; i < sz; i++)
		result += QString::number(bytePtr[i], 16);
	return result;
}


//-------------------------------------------------
//  hashString
//-------------------------------------------------

QString Audit::hashString(std::optional<std::uint32_t> crc32, const QByteArray &sha1)
{
	QString result;
	if (crc32)
		result = QString("CRC(%1) ").arg(QString::number(*crc32, 16));

	QString sha1Hex = hexString(sha1.constData(), sha1.size());
	result += QString("SHA1(%1)").arg(sha1Hex);
	return result;    
}


//-------------------------------------------------
//  addMediaForMachine
//-------------------------------------------------

template<std::size_t N>
QString Audit::hashString(std::optional<std::uint32_t> crc32, const std::array<uint8_t, N> &sha1)
{
	QByteArray sha1ByteArray((const char *)&sha1[0], std::size(sha1));
	return hashString(crc32, sha1ByteArray);
}


//-------------------------------------------------
//  addMediaForMachine
//-------------------------------------------------

void Audit::addMediaForMachine(const Preferences &prefs, const info::machine &machine)
{
	// set up paths
	int romsPathsPos = setupPaths(prefs, machine, Preferences::global_path_type::ROMS);
	int samplesPathsPos = setupPaths(prefs, machine, Preferences::global_path_type::SAMPLES);

	// audit ROMs
	for (info::rom rom : machine.roms())
		m_entries.emplace_back(rom.name(), romsPathsPos, rom.status(), rom.size(), rom.crc32(), rom.sha1(), rom.optional());

	// audit disks
	for (info::disk disk : machine.disks())
		m_entries.emplace_back(disk.name(), romsPathsPos, disk.status(), std::optional<std::uint32_t>(), std::optional<std::uint32_t>(), disk.sha1(), disk.optional());

	// audit samples
	for (info::sample sample : machine.samples())
		m_entries.emplace_back(sample.name(), samplesPathsPos, info::rom::dump_status_t::NODUMP, std::optional<std::uint32_t>(), std::optional<std::uint32_t>(), std::array<uint8_t, 20>(), true);
}


//-------------------------------------------------
//  setupPaths
//-------------------------------------------------

int Audit::setupPaths(const Preferences &prefs, std::optional<info::machine> machine, Preferences::global_path_type pathType)
{
	int position;

	// get base paths from preferences
	QStringList basePaths = prefs.getSplitPaths(pathType);

	// blow these out to machine paths
	QStringList paths;
	while (machine)
	{
		for (const QString &path : basePaths)
		{
			paths.push_back(path + "/" + machine->name());
			paths.push_back(path + "/" + machine->name() + ".zip");
		}
		machine = machine->clone_of();
	}

	// record our position; we want to record it
	position = m_pathList.size();

	// and put the results on our path list
	m_pathList.emplace_back(std::move(paths));
	return position;
}


//-------------------------------------------------
//  run
//-------------------------------------------------

AuditStatus Audit::run(const std::function<void(QString &&)> &callback) const
{
	Session session(callback);
	std::vector<std::unique_ptr<AssetFinder>> assetFinders;

	// loop through all entries
	for (const Entry &entry : m_entries)
		auditSingleMedia(session, entry, assetFinders);

	return session.status();
}


//-------------------------------------------------
//  auditSingleMedia
//-------------------------------------------------

void Audit::auditSingleMedia(Session &session, const Entry &entry, std::vector<std::unique_ptr<AssetFinder>> &assetFinders) const
{
	// ensure that an assetFinder is available
	if (entry.pathsPosition() >= assetFinders.size())
	{
		// we need to expand the asset finders; do so
		std::size_t oldSize = assetFinders.size();
		assetFinders.resize(entry.pathsPosition() + 1);

		// and set the paths
		for (std::size_t i = oldSize; i < assetFinders.size(); i++)
			assetFinders[i] = std::make_unique<AssetFinder>(QStringList(m_pathList[i]));
	}

	// identify the AssetFinder
	const AssetFinder &assetFinder = *assetFinders[entry.pathsPosition()];

	// try to find the asset
	std::unique_ptr<QIODevice> stream = assetFinder.findAsset(entry.name());

	// do we have anything at all
	if (!stream)
	{
		session.message(AuditStatus::Missing, QString("%1: NOT FOUND").arg(entry.name()));
		return;
	}

	// does the size match?
	if (entry.expectedSize() && stream->size() != entry.expectedSize())
	{
		session.message(AuditStatus::Missing, QString("%1: INCORRECT LENGTH: %2 bytes").arg(entry.name(), stream->size()));
		return;
	}

	// do we have good dumps?
	if (entry.dumpStatus() != info::rom::dump_status_t::NODUMP)
	{
		// calculate the hashes
		std::optional<std::uint32_t> actualCrc32;
		QByteArray actualSha1;
		if (entry.expectedCrc32())
			actualCrc32.emplace(0);
		calculateHashes(*stream, entry.expectedCrc32() ? &*actualCrc32 : nullptr, &actualSha1);

		// do they match?
		if (entry.expectedCrc32() != actualCrc32 || !byteArrayEquals(actualSha1, entry.expectedSha1()))
		{
			QString msg = QString("%1 WRONG CHECKSUMS\nEXPECTED %2\nFOUND %3").arg(
				entry.name(),
				hashString(entry.expectedCrc32(), entry.expectedSha1()),
				hashString(actualCrc32, actualSha1));
			session.message(entry.optional() ? AuditStatus::MissingOptional : AuditStatus::Missing, std::move(msg));
		}
	}
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

Audit::Entry::Entry(const QString &name, int pathsPosition, info::rom::dump_status_t dumpStatus, std::optional<std::uint32_t> expectedSize,
	std::optional<std::uint32_t> expectedCrc32, const std::array<uint8_t, 20> &expectedSha1, bool optional)
	: m_name(name)
	, m_pathsPosition(pathsPosition)
	, m_dumpStatus(dumpStatus)
	, m_expectedSize(expectedSize)
	, m_expectedCrc32(expectedCrc32)
	, m_expectedSha1(expectedSha1)
	, m_optional(optional)
{
}


//-------------------------------------------------
//  Session ctor
//-------------------------------------------------

Audit::Session::Session(const std::function<void(QString &&)> &callback)
	: m_callback(callback)
	, m_status(AuditStatus::Found)
{
}


//-------------------------------------------------
//  Session::message
//-------------------------------------------------

void Audit::Session::message(AuditStatus newStatus, QString &&message)
{
	if (newStatus > m_status)
		m_status = newStatus;
	if (m_callback)
		m_callback(std::move(message));
}
