/***************************************************************************

    audit.h

    ROM/Asset auditing

***************************************************************************/

#ifndef AUDIT_H
#define AUDIT_H

#include "info.h"
#include "prefs.h"
#include "hash.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class AssetFinder;


// ======================> Audit

class Audit
{
public:
	class Entry
	{
		friend class Audit;
	public:
		// ctor
		Entry(const QString &name, int pathsPosition, info::rom::dump_status_t dumpStatus, std::optional<std::uint32_t> expectedSize, const Hash &expectedHash, bool optional);
		Entry(const Entry &) = default;
		Entry(Entry &&) = default;

		// accessors
		const QString &name() const							{ return m_name; }
		int pathsPosition() const							{ return m_pathsPosition; }
		info::rom::dump_status_t dumpStatus() const			{ return m_dumpStatus; }
		std::optional<std::uint32_t> expectedSize() const	{ return m_expectedSize; }
		const Hash &expectedHash() const					{ return m_expectedHash; }
		bool optional() const								{ return m_optional; }

	private:
		QString							m_name;
		int								m_pathsPosition;
		info::rom::dump_status_t		m_dumpStatus;
		std::optional<std::uint32_t>	m_expectedSize;
		Hash							m_expectedHash;
		bool							m_optional;
	};

	// ctor
	Audit();

	// accessors
	const std::vector<Entry> &entries() const { return m_entries; }

	// methods
	void addMediaForMachine(const Preferences &prefs, const info::machine &machine);
	AuditStatus run(const std::function<void(QString &&)> &callback = { }) const;

private:
	class Session;

	// variables
	std::vector<QStringList>				m_pathList;
	std::vector<Entry>						m_entries;

	// methods
	int setupPaths(const Preferences &prefs, std::optional<info::machine> machine, Preferences::global_path_type pathType);
	void auditSingleMedia(Session &session, const Entry &entry, std::vector<std::unique_ptr<AssetFinder>> &assetFinders) const;
};


#endif // AUDIT_H
