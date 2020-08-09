/***************************************************************************

    mameversion.h

    Logic for parsing and comparing MAME version strings

***************************************************************************/

#pragma once

#ifndef MAMEVERSION_H
#define MAMEVERSION_H

#include <QString>

class MameVersion
{
public:
	class Test;

	// ctor
	MameVersion(const QString &version)
	{
		Parse(version, m_major, m_minor, m_dirty);
	}

	MameVersion(int major, int minor, bool dirty)
	{
		m_major = major;
		m_minor = minor;
		m_dirty = dirty;
	}

	// accessors
	int Major() const	{ return m_major; }
	int Minor() const	{ return m_minor; }
	bool Dirty() const	{ return m_dirty; }

	bool IsAtLeast(const MameVersion &that) const
	{
		bool result;
		if (Major() > that.Major())
			result = true;
		else if (Major() < that.Major())
			result = false;
		else if (Minor() > that.Minor())
			result = true;
		else if (Minor() < that.Minor())
			result = false;
		else
			result = Dirty() || !that.Dirty();
		return result;		
	}

	MameVersion NextCleanVersion() const
	{
		return MameVersion(Major(), Minor() + (Dirty() ? 1 : 0), false);
	}

	QString ToString() const
	{
		return QString(Dirty() ? "%1.%2 (dirty)" : "%1.%2").arg(
			QString::number(Major()),
			QString::number(Minor()));
	}

private:
	int	m_major;
	int m_minor;
	bool m_dirty;

	static void Parse(const QString &version_string, int &major, int &minor, bool &dirty);
};

#endif // MAMEVERSION_H
