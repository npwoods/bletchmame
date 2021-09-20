/***************************************************************************

    audit.cpp

    ROM/Asset auditing

***************************************************************************/

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

//-------------------------------------------------
//  ctor
//-------------------------------------------------

Audit::Audit()
{
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
		m_entries.emplace_back(rom.name(), romsPathsPos, rom.status(), rom.size(), Hash(rom.crc32(), rom.sha1()), rom.optional());

	// audit disks
	for (info::disk disk : machine.disks())
		m_entries.emplace_back(disk.name(), romsPathsPos, disk.status(), std::optional<std::uint32_t>(), Hash(disk.sha1()), disk.optional());

	// audit samples
	for (info::sample sample : machine.samples())
		m_entries.emplace_back(sample.name(), samplesPathsPos, info::rom::dump_status_t::NODUMP, std::optional<std::uint32_t>(), Hash(), true);
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
	position = util::safe_static_cast<int>(m_pathList.size());

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
		// calculate the hash
		Hash actualHash = Hash::calculate(*stream);

		// get a hash for comparison purposes
		Hash comparisonHash = actualHash.mask(
			entry.expectedHash().crc32().has_value(),
			entry.expectedHash().sha1().has_value());

		// do they match?
		if (entry.expectedHash() != comparisonHash)
		{
			QString msg = QString("%1 WRONG CHECKSUMS\nEXPECTED %2\nFOUND %3").arg(
				entry.name(),
				entry.expectedHash().toString(),
				actualHash.toString());
			session.message(entry.optional() ? AuditStatus::MissingOptional : AuditStatus::Missing, std::move(msg));
		}
	}
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

Audit::Entry::Entry(const QString &name, int pathsPosition, info::rom::dump_status_t dumpStatus, std::optional<std::uint32_t> expectedSize, const Hash &expectedHash, bool optional)
	: m_name(name)
	, m_pathsPosition(pathsPosition)
	, m_dumpStatus(dumpStatus)
	, m_expectedSize(expectedSize)
	, m_expectedHash(expectedHash)
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
