/***************************************************************************

    mameversion.h

    Logic for parsing and comparing MAME version strings

***************************************************************************/

#pragma once

#ifndef MAMEVERSION_H
#define MAMEVERSION_H

#include <QString>

// ======================> MameVersion

class MameVersion
{
public:
	class Test;

	// ctor
	MameVersion(const QString &version)
	{
		parse(version, m_major, m_minor, m_dirty);
	}

	MameVersion(int major, int minor, bool dirty)
	{
		m_major = major;
		m_minor = minor;
		m_dirty = dirty;
	}

	// accessors
	int major() const	{ return m_major; }
	int minor() const	{ return m_minor; }
	bool dirty() const	{ return m_dirty; }

	bool isAtLeast(const MameVersion &that) const
	{
		bool result;
		if (major() > that.major())
			result = true;
		else if (major() < that.major())
			result = false;
		else if (minor() > that.minor())
			result = true;
		else if (minor() < that.minor())
			result = false;
		else
			result = dirty() || !that.dirty();
		return result;		
	}

	MameVersion nextCleanVersion() const
	{
		return MameVersion(major(), minor() + (dirty() ? 1 : 0), false);
	}

	QString toString() const
	{
		return QString(dirty() ? "%1.%2 (dirty)" : "%1.%2").arg(
			QString::number(major()),
			QString::number(minor()));
	}

private:
	int	m_major;
	int m_minor;
	bool m_dirty;

	static void parse(const QString &versionString, int &major, int &minor, bool &dirty);
};

#endif // MAMEVERSION_H
