/***************************************************************************

    mameversion.cpp

    Logic for parsing and comparing MAME version strings

***************************************************************************/

#include "mameversion.h"
#include "validity.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  Parse
//-------------------------------------------------

void MameVersion::Parse(const QString &version_string, int &major, int &minor, bool &dirty)
{
	if (sscanf(version_string.toStdString().c_str(), "%d.%d", &major, &minor) != 2)
	{
		// can't parse the string
		major = 0;
		minor = 0;
		dirty = true;
	}
	else
	{
		// we parsed the string; now check if we are dirty
		QString non_dirty_version_string = QString::asprintf("%d.%d (mame%d%d)", major, minor, major, minor);
		dirty = version_string != non_dirty_version_string;
	}
}


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

static void general(const char *version_string, int expected_major, int expected_minor, bool expected_dirty)
{
	auto version = MameVersion(version_string);
	assert(version.Major() == expected_major);
	assert(version.Minor() == expected_minor);
	assert(version.Dirty() == expected_dirty);
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	[]() { general("blah",								0, 0,	true); },
	[]() { general("0.217 (mame0217)",					0, 217,	false); },
	[]() { general("0.217 (mame0217-90-g0cb747353b)",	0, 217,	true); }
};
