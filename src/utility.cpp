/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

#include <sstream>
#include <QDir>

#include "utility.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

const QString util::g_empty_string;


//-------------------------------------------------
//  wxFileName::IsPathSeparator
//-------------------------------------------------

bool wxFileName::IsPathSeparator(QChar ch)
{
	return ch == '/' || ch == QDir::separator();
}


//-------------------------------------------------
//  wxFileName::SplitPath
//-------------------------------------------------

void wxFileName::SplitPath(const QString &fullpath, QString *path, QString *name, QString *ext)
{
	QFileInfo fi(fullpath);
	if (path)
		*path = fi.dir().absolutePath();
	if (name)
		*name = fi.baseName();
	if (ext)
		*ext = fi.suffix();
}
