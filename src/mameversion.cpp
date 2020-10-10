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
