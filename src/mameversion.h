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
	MameVersion(const QString &version);
	MameVersion(int major, int minor, bool dirty);

	// accessors
	int major() const	{ return m_major; }
	int minor() const	{ return m_minor; }
	bool dirty() const	{ return m_dirty; }

	// methods
	bool isAtLeast(const MameVersion &that) const;
	MameVersion nextCleanVersion() const;
	QString toString() const;

private:
	int	m_major;
	int m_minor;
	bool m_dirty;

	static void parse(const QString &versionString, int &major, int &minor, bool &dirty);
};

#endif // MAMEVERSION_H
