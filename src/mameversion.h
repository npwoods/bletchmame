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

// ======================> SimpleMameVersion

class MameVersion;

class SimpleMameVersion
{
public:
	// ctor
	constexpr SimpleMameVersion(int major, int minor)
		: m_major(major)
		, m_minor(minor)
	{
	}
	SimpleMameVersion(const SimpleMameVersion &) = default;
	SimpleMameVersion(SimpleMameVersion &&) = default;

	// accessors
	int major() const { return m_major; }
	int minor() const { return m_minor; }

	// operators
	SimpleMameVersion &operator=(const SimpleMameVersion &) = default;
	SimpleMameVersion &operator=(SimpleMameVersion &&) = default;
	bool operator==(const SimpleMameVersion &) const = default;

	// methods
	operator MameVersion() const;
	QString toString() const;
	QString toPrettyString() const;

private:
	int						m_major;
	int						m_minor;
};

// ======================> MameVersion

class MameVersion
{
public:
	class Test;

	// ctors
	MameVersion(int major, int minor);
	MameVersion(const QString &version);
	MameVersion(const MameVersion &) = default;
	MameVersion(MameVersion &&) = default;

	// accessors
	int major() const		{ return m_major; }
	int minor() const		{ return m_minor; }
	bool isDirty() const	{ return (bool)m_rawVersionString; }

	// operators
	MameVersion &operator=(const MameVersion &) = default;
	MameVersion &operator=(MameVersion &&) = default;
	bool operator==(const MameVersion &) const = default;

	// methods
	bool isAtLeast(const SimpleMameVersion &that) const;
	SimpleMameVersion nextCleanVersion() const;
	QString toString() const;
	QString toPrettyString() const;

	// MAME versions
	struct Capabilities
	{
		// -attach_window support
		static const std::optional<SimpleMameVersion> HAS_ATTACH_WINDOW;
		static const std::optional<SimpleMameVersion> HAS_ATTACH_CHILD_WINDOW;

		// minimum MAME version
		static const SimpleMameVersion MINIMUM_SUPPORTED;

		// recording movies by specifying absolute paths was introduced in MAME 0.221
		static const SimpleMameVersion HAS_TOGGLE_MOVIE;
	};

private:
	int						m_major;
	int						m_minor;
	std::optional<QString>	m_rawVersionString;	// only set for dirty versions

	SimpleMameVersion toSimpleMameVersion() const;
};


#endif // MAMEVERSION_H
