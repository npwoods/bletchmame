/***************************************************************************

    audit.h

    ROM/Asset auditing

***************************************************************************/

#ifndef AUDIT_H
#define AUDIT_H

#include "info.h"
#include "prefs.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class AssetFinder;

// ======================> Audit

class Audit
{
public:
	class Test;

	// ctor
	Audit();

	// methods
	void addMediaForMachine(const Preferences &prefs, const info::machine &machine);
	AuditStatus run(const std::function<void(QString &&)> &callback = { }) const;

private:
	class Session;

	class Entry
	{
	public:
		Entry(const QString &name, int pathsPosition, info::rom::dump_status_t dumpStatus, std::optional<std::uint32_t> expectedSize,
			std::optional<std::uint32_t> expectedCrc32, const std::array<uint8_t, 20> &expectedSha1, bool optional);
		Entry(const Entry &) = default;
		Entry(Entry &&) = default;

		// accessors
		const QString &name() const							{ return m_name; }
		int pathsPosition() const							{ return m_pathsPosition; }
		info::rom::dump_status_t dumpStatus() const			{ return m_dumpStatus; }
		std::optional<std::uint32_t> expectedSize() const	{ return m_expectedSize; }
		std::optional<std::uint32_t> expectedCrc32() const	{ return m_expectedCrc32; }
		const std::array<uint8_t, 20> &expectedSha1() const	{ return m_expectedSha1; }
		bool optional() const								{ return m_optional; }

	private:
		QString							m_name;
		int								m_pathsPosition;
		info::rom::dump_status_t		m_dumpStatus;
		std::optional<std::uint32_t>	m_expectedSize;
		std::optional<std::uint32_t>	m_expectedCrc32;
		std::array<uint8_t, 20>			m_expectedSha1;
		bool							m_optional;
	};

	// static variables
	static std::array<quint32, 256>         s_crcTable;

	// variables
	std::vector<QStringList>				m_pathList;
	std::vector<Entry>						m_entries;

	// methods
	int setupPaths(const Preferences &prefs, std::optional<info::machine> machine, Preferences::global_path_type pathType);
	void auditSingleMedia(Session &session, const Entry &entry, std::vector<std::unique_ptr<AssetFinder>> &assetFinders) const;
	static std::array<quint32, 256> calculateCrc32Table();
	static void calculateHashes(QIODevice &stream, std::uint32_t *crc32, QByteArray *sha1);
	static QString hashString(std::optional<std::uint32_t> crc32, const QByteArray &sha1);
	template<std::size_t N> static QString hashString(std::optional<std::uint32_t> crc32, const std::array<uint8_t, N> &sha1);
	template<std::size_t N> static bool byteArrayEquals(const QByteArray &byteArray, const std::array<std::uint8_t, N> &stdArray);
	static QString hexString(const void *ptr, size_t sz);
};


#endif // AUDIT_H
