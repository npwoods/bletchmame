/***************************************************************************

    audit.h

    ROM/Asset auditing

***************************************************************************/

#ifndef AUDIT_H
#define AUDIT_H

#include "info.h"
#include "prefs.h"
#include "hash.h"
#include "softwarelist.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class AssetFinder;


// ======================> Audit

class Audit
{
public:
	class Test;

	// ======================> Audit::Verdict

	class Verdict
	{
	public:
		enum class Type
		{
			// successful verdicts
			Ok,
			OkNoGoodDump,

			// error conditions
			NotFound,
			IncorrectSize,
			Mismatch,
			CouldntProcessAsset
		};

		// ctor
		Verdict(Type type, std::uint64_t actualSize, Hash actualHash);
		Verdict(const Verdict &) = default;
		Verdict(Verdict &&) = default;

		// accessors
		Type type() const					{ return m_type; }
		std::uint64_t actualSize() const	{ return m_actualSize; }
		const Hash &actualHash() const		{ return m_actualHash; }

	private:
		Type			m_type;
		std::uint64_t	m_actualSize;
		Hash			m_actualHash;
	};


	// ======================> Audit::Entry

	class Entry
	{
		friend class Audit;

	public:
		enum class Type
		{
			Rom,
			Disk,
			Sample
		};

		typedef std::optional<Hash>(*CalcHashFunc)(QIODevice &);

		// ctor
		Entry(Type type, const QString &name, int pathsPosition, CalcHashFunc calcHashFunc, info::rom::dump_status_t dumpStatus, std::optional<std::uint32_t> expectedSize, const Hash &expectedHash, bool optional);
		Entry(const Entry &) = default;
		Entry(Entry &&) = default;

		// accessors
		Type type() const									{ return m_type; }
		const QString &name() const							{ return m_name; }
		int pathsPosition() const							{ return m_pathsPosition; }
		CalcHashFunc calculateHashFunc() const				{ return m_calcHashFunc; }
		info::rom::dump_status_t dumpStatus() const			{ return m_dumpStatus; }
		std::optional<std::uint32_t> expectedSize() const	{ return m_expectedSize; }
		const Hash &expectedHash() const					{ return m_expectedHash; }
		bool optional() const								{ return m_optional; }

	private:
		Type							m_type;
		QString							m_name;
		int								m_pathsPosition;
		CalcHashFunc					m_calcHashFunc;
		info::rom::dump_status_t		m_dumpStatus;
		std::optional<std::uint32_t>	m_expectedSize;
		Hash							m_expectedHash;
		bool							m_optional;
	};

	typedef std::function<void(int entryIndex, const Verdict &verdict)> Callback;

	// ctor
	Audit();

	// accessors
	const std::vector<Entry> &entries() const	{ return m_entries; }

	// methods
	void addMediaForMachine(const Preferences &prefs, const info::machine &machine);
	void addMediaForSoftware(const Preferences &prefs, const software_list::software &software);
	AuditStatus run(const Callback &callback = { }) const;

	// statics
	static bool isVerdictSuccessful(Audit::Verdict::Type verdictType);

private:
	class Session;

	// variables
	std::vector<QStringList>				m_pathList;
	std::vector<Entry>						m_entries;

	// methods
	QStringList buildMachinePaths(const Preferences &prefs, Preferences::global_path_type pathType, std::optional<info::machine> machine);
	int appendPaths(QStringList &&paths);
	void auditSingleMedia(Session &session, int entryIndex, std::vector<std::unique_ptr<AssetFinder>> &assetFinders) const;
};


#endif // AUDIT_H
