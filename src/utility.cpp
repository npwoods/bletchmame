/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

#include <sstream>
#include <QDir>
#include <QGridLayout>

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


//-------------------------------------------------
//  globalPositionBelowWidget
//-------------------------------------------------

QPoint globalPositionBelowWidget(const QWidget &widget)
{
	QPoint localPos(0, widget.height());
	QPoint globalPos = widget.mapToGlobal(localPos);
	return globalPos;
}


//-------------------------------------------------
//  truncateGridLayout
//-------------------------------------------------

void truncateGridLayout(QGridLayout &gridLayout, int rows)
{
	for (int row = gridLayout.rowCount() - 1; row >= 0 && row >= rows; row--)
	{
		for (int col = 0; col < gridLayout.columnCount(); col++)
		{
			QLayoutItem *layoutItem = gridLayout.itemAtPosition(row, col);
			if (layoutItem && layoutItem->widget())
				delete layoutItem->widget();
		}
	}
}
