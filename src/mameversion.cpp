/***************************************************************************

    mameversion.cpp

    Logic for parsing and comparing MAME version strings

***************************************************************************/

#include "mameversion.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif // _MSC_VER


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameVersion::MameVersion(const QString &version)
{
	parse(version, m_major, m_minor, m_dirty);
}


//-------------------------------------------------
//  isAtLeast
//-------------------------------------------------

bool MameVersion::isAtLeast(const MameVersion &that) const
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


//-------------------------------------------------
//  nextCleanVersion
//-------------------------------------------------

MameVersion MameVersion::nextCleanVersion() const
{
	return MameVersion(major(), minor() + (dirty() ? 1 : 0), false);
}


//-------------------------------------------------
//  toString
//-------------------------------------------------

QString MameVersion::toString() const
{
	return QString(dirty() ? "%1.%2 (dirty)" : "%1.%2").arg(
		QString::number(major()),
		QString::number(minor()));
}


//-------------------------------------------------
//  parse
//-------------------------------------------------

void MameVersion::parse(const QString &versionString, int &major, int &minor, bool &dirty)
{
	if (sscanf(versionString.toStdString().c_str(), "%d.%d", &major, &minor) != 2)
	{
		// can't parse the string
		major = 0;
		minor = 0;
		dirty = true;
	}
	else
	{
		// we parsed the string; now check if we are dirty
		QString non_dirty_versionString = QString("%1.%2 (mame%3%4)").arg(
			QString::number(major),
			QString::number(minor),
			QString::number(major),
			QString::number(minor));
		dirty = versionString != non_dirty_versionString;
	}
}
