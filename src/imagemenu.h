/***************************************************************************

	imagemenu.h

	Implements functionality for image selection from a menu

***************************************************************************/

#ifndef IMAGEMENU_H
#define IMAGEMENU_H

// bletchmame headers
#include "info.h"
#include "status.h"

// Qt headers
#include <QMenu>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

#define TEXT_NONE				"<<none>>"

QT_BEGIN_NAMESPACE
class QMenu;
QT_END_NAMESPACE

class Preferences;
class software_list_collection;

// ======================> IImageMenuHost

class IImageMenuHost
{
public:
	virtual Preferences &getPreferences() = 0;
	virtual info::machine getRunningMachine() const = 0;
	virtual const software_list_collection &getRunningSoftwareListCollection() const = 0;
	virtual status::state &getRunningState() = 0;
	virtual void createImage(const QString &tag, QString &&path) = 0;
	virtual void loadImage(const QString &tag, QString &&path) = 0;
	virtual void unloadImage(const QString &tag) = 0;
};


//**************************************************************************
//  FUNCTIONS
//**************************************************************************

void appendImageMenuItems(IImageMenuHost &host, QMenu &popupMenu, const QString &tag);
QString prettifyImageFileName(const info::machine &machine, const software_list_collection &software_col,
	const QString &deviceTag, const QString &fileName, bool fullPath);

#endif // IMAGEMENU_H
