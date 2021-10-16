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
	Session(const Callback &callback);

	// accessors
	AuditStatus status() const { return m_status; }

	// methods
	void message(AuditStatus newStatus, int entryIndex, const Verdict &verdict);

private:
	const Callback &	m_callback;
	AuditStatus				m_status;
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
	// set up ROM paths
	QStringList romsPaths = buildMachinePaths(prefs, Preferences::global_path_type::ROMS, machine);
	int romsPathsPos = appendPaths(std::move(romsPaths));

	// set up Sample paths
	QStringList samplesPaths = buildMachinePaths(prefs, Preferences::global_path_type::SAMPLES, machine);
	int samplesPathsPos = appendPaths(std::move(samplesPaths));

	// audit ROMs
	for (info::rom rom : machine.roms())
		m_entries.emplace_back(Entry::Type::Rom, rom.name(), romsPathsPos, rom.status(), rom.size(), Hash(rom.crc32(), rom.sha1()), rom.optional());

	// audit disks
	for (info::disk disk : machine.disks())
		m_entries.emplace_back(Entry::Type::Disk, disk.name(), romsPathsPos, disk.status(), std::optional<std::uint32_t>(), Hash(disk.sha1()), disk.optional());

	// audit samples
	for (info::sample sample : machine.samples())
		m_entries.emplace_back(Entry::Type::Sample, sample.name(), samplesPathsPos, info::rom::dump_status_t::NODUMP, std::optional<std::uint32_t>(), Hash(), true);
}


//-------------------------------------------------
//  buildMachinePaths
//-------------------------------------------------

QStringList Audit::buildMachinePaths(const Preferences &prefs, Preferences::global_path_type pathType, std::optional<info::machine> machine)
{
	// get base paths from preferences
	QStringList basePaths = prefs.getSplitPaths(pathType);

	// blow these out to machine paths
	QStringList results;
	while (machine)
	{
		for (const QString &path : basePaths)
		{
			results.push_back(path + "/" + machine->name());
			results.push_back(path + "/" + machine->name() + ".zip");
		}
		machine = machine->clone_of();
	}
	return results;
}


//-------------------------------------------------
//  appendPaths
//-------------------------------------------------

int Audit::appendPaths(QStringList &&paths)
{
	// record our position; we want to record it
	int position = util::safe_static_cast<int>(m_pathList.size());

	// and put the results on our path list
	m_pathList.emplace_back(std::move(paths));
	return position;
}


//-------------------------------------------------
//  run
//-------------------------------------------------

AuditStatus Audit::run(const Callback &callback) const
{
	Session session(callback);
	std::vector<std::unique_ptr<AssetFinder>> assetFinders;

	// loop through all entries
	for (int i = 0; i < m_entries.size(); i++)
		auditSingleMedia(session, i, assetFinders);

	return session.status();
}


//-------------------------------------------------
//  auditSingleMedia
//-------------------------------------------------

void Audit::auditSingleMedia(Session &session, int entryIndex, std::vector<std::unique_ptr<AssetFinder>> &assetFinders) const
{
	// find the entry
	const Entry &entry = m_entries[entryIndex];

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
	std::unique_ptr<QIODevice> stream = assetFinder.findAsset(
		entry.name(),
		entry.expectedHash().crc32());

	// get critical information
	std::uint64_t actualSize;
	Hash actualHash;

	// and time to get a verdict
	Verdict::Type verdictType;
	if (!stream)
	{
		// this entry was not found at all
		verdictType = Verdict::Type::NotFound;
		actualSize = 0;
	}
	else
	{
		// we have something; process it
		actualSize = stream->size();
		actualHash = Hash::calculate(*stream);

		if (entry.expectedSize() && actualSize != entry.expectedSize())
		{
			// this entry has the wrong tisze
			verdictType = Verdict::Type::IncorrectSize;
		}
		else if (entry.dumpStatus() == info::rom::dump_status_t::NODUMP)
		{
			// this entry has no good dump
			verdictType = Verdict::Type::OkNoGoodDump;
		}
		else
		{
			// get a hash for comparison purposes
			Hash comparisonHash = actualHash.mask(
				entry.expectedHash().crc32().has_value(),
				entry.expectedHash().sha1().has_value());

			// do they match?
			verdictType = entry.expectedHash() == comparisonHash
				? Verdict::Type::Ok
				: Verdict::Type::Mismatch;
		}
	}

	// build the actual verdict
	Verdict verdict(verdictType, actualSize, actualHash);

	// determine the audit status
	AuditStatus status;
	if (isVerdictSuccessful(verdictType))
		status = AuditStatus::Found;
	else if (entry.optional())
		status = AuditStatus::MissingOptional;
	else
		status = AuditStatus::Missing;

	// and report this stuff
	session.message(status, entryIndex, verdict);
}


//-------------------------------------------------
//  isVerdictSuccessful
//-------------------------------------------------

bool Audit::isVerdictSuccessful(Audit::Verdict::Type verdictType)
{
	bool result;
	switch (verdictType)
	{
	case Audit::Verdict::Type::Ok:
	case Audit::Verdict::Type::OkNoGoodDump:
		result = true;
		break;

	case Audit::Verdict::Type::NotFound:
	case Audit::Verdict::Type::IncorrectSize:
	case Audit::Verdict::Type::Mismatch:
		result = false;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  Verdict ctor
//-------------------------------------------------

Audit::Verdict::Verdict(Type type, std::uint64_t actualSize, Hash actualHash)
	: m_type(type)
	, m_actualSize(actualSize)
	, m_actualHash(actualHash)
{
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

Audit::Entry::Entry(Type type, const QString &name, int pathsPosition, info::rom::dump_status_t dumpStatus, std::optional<std::uint32_t> expectedSize, const Hash &expectedHash, bool optional)
	: m_type(type)
	, m_name(name)
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

Audit::Session::Session(const Callback &callback)
	: m_callback(callback)
	, m_status(AuditStatus::Found)
{
}


//-------------------------------------------------
//  Session::message
//-------------------------------------------------

void Audit::Session::message(AuditStatus newStatus, int entryIndex, const Verdict &verdict)
{
	if (newStatus > m_status)
		m_status = newStatus;
	if (m_callback)
		m_callback(entryIndex, verdict);
}
