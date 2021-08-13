/***************************************************************************

    mameversion.h

    Logic for parsing and comparing MAME version strings

***************************************************************************/

#pragma once

#ifndef MAMEVERSION_H
#define MAMEVERSION_H

#include <QString>
#include <optional>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> MameVersion

class MameVersion
{
public:
	class Test;

	// ctor
	MameVersion(const QString &version);
	constexpr MameVersion(int major, int minor, bool dirty)
		: m_major(major)
		, m_minor(minor)
		, m_dirty(dirty)
	{
	}

	// accessors
	int major() const	{ return m_major; }
	int minor() const	{ return m_minor; }
	bool dirty() const	{ return m_dirty; }

	// methods
	bool isAtLeast(const MameVersion &that) const;
	MameVersion nextCleanVersion() const;
	QString toString() const;

	// MAME versions
	struct Capabilities;

private:
	int	m_major;
	int m_minor;
	bool m_dirty;

	static void parse(const QString &versionString, int &major, int &minor, bool &dirty);
};



// ======================> MameVersion::Capabilities

struct MameVersion::Capabilities
{
	// -attach_window support
#if defined(Q_OS_WIN32)
	static constexpr std::optional<MameVersion> HAS_ATTACH_WINDOW = MameVersion(0, 213, false);
	static constexpr std::optional<MameVersion> HAS_ATTACH_CHILD_WINDOW = MameVersion(0, 218, false);
#elif defined(Q_OS_UNIX)
	static constexpr std::optional<MameVersion> HAS_ATTACH_WINDOW = MameVersion(0, 232, false);
	static constexpr std::optional<MameVersion> HAS_ATTACH_CHILD_WINDOW = MameVersion(0, 232, false);
#else
	static constexpr std::optional<MameVersion> HAS_ATTACH_WINDOW = std::nullopt;
	static constexpr std::optional<MameVersion> HAS_ATTACH_CHILD_WINDOW = std::nullopt;
#endif

	// minimum MAME version
	static constexpr MameVersion MINIMUM_SUPPORTED = MameVersion(0, 213, false);

	// recording movies by specifying absolute paths was introduced in MAME 0.221
	static constexpr MameVersion HAS_TOGGLE_MOVIE = MameVersion(0, 221, false);
};


#endif // MAMEVERSION_H
