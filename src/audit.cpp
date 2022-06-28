/***************************************************************************

    audit.cpp

    ROM/Asset auditing

***************************************************************************/

// bletchmame headers
#include "audit.h"
#include "assetfinder.h"
#include "chd.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class Audit::Session
{
public:
	// ctor
	Session(ICallback &callback);

	// accessors
	AuditStatus status() const { return m_status; }
	bool hasAborted() const { return m_hasAborted; }

	// methods
	bool reportProgress(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t total);
	void verdictReached(AuditStatus newStatus, int entryIndex, const Verdict &verdict);

private:
	ICallback &		m_callback;
	AuditStatus		m_status;
	bool			m_hasAborted;
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
//  calculateHashForFile
//-------------------------------------------------

static Audit::Entry::CalculateHashStatus calculateHashForFile(QIODevice &stream, const Hash::CalculateCallback &callback, Hash &result)
{
	// calculate the hash
	std::optional<Hash> hash = Hash::calculate(stream, callback);

	// return the result
	result = hash.value_or(Hash());

	// and return the proper status
	return hash
		? Audit::Entry::CalculateHashStatus::Success
		: Audit::Entry::CalculateHashStatus::Cancelled;
}


//-------------------------------------------------
//  calculateHashForChd
//-------------------------------------------------

static Audit::Entry::CalculateHashStatus calculateHashForChd(QIODevice &stream, const Hash::CalculateCallback &callback, Hash &result)
{
	// calculate the hash
	std::optional<Hash> hash = getHashForChd(stream);

	// return the result
	result = hash.value_or(Hash());

	// and return the proper status
	return hash
		? Audit::Entry::CalculateHashStatus::Success
		: Audit::Entry::CalculateHashStatus::CantProcess;
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
		m_entries.emplace_back(Entry::Type::Rom, rom.name(), romsPathsPos, calculateHashForFile, rom.status(), rom.size(), Hash(rom.crc32(), rom.sha1()), rom.optional());

	// audit disks
	for (info::disk disk : machine.disks())
		m_entries.emplace_back(Entry::Type::Disk, disk.name() + ".chd", romsPathsPos, calculateHashForChd, disk.status(), std::optional<std::uint32_t>(), Hash(disk.sha1()), disk.optional());

	// audit samples
	for (info::sample sample : machine.samples())
		m_entries.emplace_back(Entry::Type::Sample, sample.name() + ".wav", samplesPathsPos, calculateHashForFile, info::rom::dump_status_t::GOOD, std::optional<std::uint32_t>(), Hash(), true);
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
	for (auto machineIter1 = machine; machineIter1; machineIter1 = machineIter1->clone_of())
	{
		for (auto machineIter2 = machine; machineIter2; machineIter2 = machineIter2->rom_of())
		{
			for (const QString &path : basePaths)
			{
				results.push_back(path + "/" + machineIter2->name());
				results.push_back(path + "/" + machineIter2->name() + ".zip");
			}
		}
	}
	return results;
}


//-------------------------------------------------
//  addMediaForSoftware
//-------------------------------------------------

void Audit::addMediaForSoftware(const Preferences &prefs, const software_list::software &software)
{
	// get base paths from preferences
	QStringList basePaths = prefs.getSplitPaths(Preferences::global_path_type::ROMS);

	// build the actual paths
	QStringList paths;
	const QString &softwareListName = software.parent().name();
	const QString &softwareName = software.name();
	for (const QString &basePath : basePaths)
	{
		paths.push_back(basePath + "/" + softwareListName + "/" + softwareName);
		paths.push_back(basePath + "/" + softwareListName + "/" + softwareName + ".zip");
	};

	// and append them
	int pathsPos = appendPaths(std::move(paths));

	for (const software_list::part &part : software.parts())
	{
		for (const software_list::dataarea &dataarea : part.dataareas())
		{
			for (const software_list::rom &rom : dataarea.roms())
			{
				m_entries.emplace_back(Entry::Type::Rom, rom.name(), pathsPos, calculateHashForFile, info::rom::dump_status_t::GOOD, rom.size(), Hash(rom.crc32(), rom.sha1()), false);
			}
		}
	}
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

std::optional<AuditStatus> Audit::run(ICallback &callback) const
{
	Session session(callback);
	std::vector<std::unique_ptr<AssetFinder>> assetFinders;

	// loop through all entries
	int i = 0;
	for (i = 0; !session.hasAborted() && i < m_entries.size(); i++)
		auditSingleMedia(session, i, assetFinders);

	// report the results accordingly - note that hypothetically we could have been
	// aborted after we completed, in which case we want to report complete results
	return i == m_entries.size()
		? session.status()
		: std::optional<AuditStatus>();
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
	std::optional<std::uint64_t> actualSize;
	Hash actualHash;

	// and time to get a verdict
	std::optional<Verdict::Type> verdictType;
	if (!stream)
	{
		// this entry was not found at all
		verdictType = Verdict::Type::NotFound;
		actualSize = 0;
	}
	else
	{
		// we're going to calculate the hash - prep a callback
		std::uint64_t streamSize = stream->size();
		auto calculateHashCallback = [&session, entryIndex, streamSize](std::uint64_t bytesProcessed)
		{
			return session.reportProgress(entryIndex, bytesProcessed, streamSize);
		};

		// calculate the hash
		switch (entry.calculateHashFunc()(*stream, calculateHashCallback, actualHash))
		{
		case Entry::CalculateHashStatus::Success:
			// we've successfully processed the hash - now evaluate them
			actualSize = streamSize;
			verdictType = evaluateHashes(entry.expectedSize(), entry.expectedHash(), *actualSize, actualHash, entry.dumpStatus());
			break;

		case Entry::CalculateHashStatus::Cancelled:
			// user cancelled; bail
			return;

		case Entry::CalculateHashStatus::CantProcess:
			// we couldn't process the asset (corrupt CHD?)
			actualSize = 0;
			verdictType = Verdict::Type::CouldntProcessAsset;
			break;
		}
	}

	// build the actual verdict
	Verdict verdict(*verdictType, *actualSize, actualHash);

	// determine the audit status
	AuditStatus status;
	if (isVerdictSuccessful(*verdictType))
		status = AuditStatus::Found;
	else if (entry.optional())
		status = AuditStatus::MissingOptional;
	else
		status = AuditStatus::Missing;

	// and report this stuff
	session.verdictReached(status, entryIndex, verdict);
}


//-------------------------------------------------
//  evaluateHashes
//-------------------------------------------------

Audit::Verdict::Type Audit::evaluateHashes(const std::optional<std::uint32_t> &expectedSize, const Hash &expectedHash,
	std::uint64_t actualSize, const Hash &actualHash, info::rom::dump_status_t dumpStatus)
{
	Verdict::Type result;
	if (expectedSize && actualSize != expectedSize)
	{
		// this entry has the wrong size
		result = Verdict::Type::IncorrectSize;
	}
	else if (dumpStatus == info::rom::dump_status_t::NODUMP)
	{
		// this entry has no good dump
		result = Verdict::Type::OkNoGoodDump;
	}
	else
	{
		// get a hash for comparison purposes
		Hash comparisonHash = actualHash.mask(
			bool(expectedHash.crc32()),
			bool(expectedHash.sha1()));

		// do they match?
		result = expectedHash == comparisonHash
			? Verdict::Type::Ok
			: Verdict::Type::Mismatch;
	}
	return result;
}


//-------------------------------------------------
//  isVerdictSuccessful
//-------------------------------------------------

bool Audit::isVerdictSuccessful(Audit::Verdict::Type verdictType)
{
	std::optional<bool> result;
	switch (verdictType)
	{
	case Audit::Verdict::Type::Ok:
	case Audit::Verdict::Type::OkNoGoodDump:
		result = true;
		break;

	case Audit::Verdict::Type::NotFound:
	case Audit::Verdict::Type::IncorrectSize:
	case Audit::Verdict::Type::Mismatch:
	case Audit::Verdict::Type::CouldntProcessAsset:
		result = false;
		break;
	}
	return *result;
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

Audit::Entry::Entry(Type type, const QString &name, int pathsPosition, CalculateHashFunc calculateHashFunc, info::rom::dump_status_t dumpStatus, std::optional<std::uint32_t> expectedSize, const Hash &expectedHash, bool optional)
	: m_type(type)
	, m_name(name)
	, m_pathsPosition(pathsPosition)
	, m_calculateHashFunc(calculateHashFunc)
	, m_dumpStatus(dumpStatus)
	, m_expectedSize(expectedSize)
	, m_expectedHash(expectedHash)
	, m_optional(optional)
{
}


//-------------------------------------------------
//  Session ctor
//-------------------------------------------------

Audit::Session::Session(ICallback &callback)
	: m_callback(callback)
	, m_status(AuditStatus::Found)
	, m_hasAborted(false)
{
}


//-------------------------------------------------
//  Session::reportProgress
//-------------------------------------------------

bool Audit::Session::reportProgress(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t total)
{
	// call the actual callback
	bool hasAborted = m_callback.reportProgress(entryIndex, bytesProcessed, total);

	// if the callback indicated that we aborted, record it
	if (hasAborted)
		m_hasAborted = true;

	// and we're done!
	return hasAborted;
}


//-------------------------------------------------
//  Session::verdictReached
//-------------------------------------------------

void Audit::Session::verdictReached(AuditStatus newStatus, int entryIndex, const Verdict &verdict)
{
	// aggregate the status
	if (newStatus > m_status)
		m_status = newStatus;

	// invoke the callback
	m_callback.reportVerdict(entryIndex, verdict);
}
