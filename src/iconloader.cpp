/***************************************************************************

	iconloader.cpp

	Icon management

***************************************************************************/

#include "iconloader.h"
#include "prefs.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define ICON_SIZE_X 16
#define ICON_SIZE_Y 16


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

IconLoader::IconLoader(Preferences &prefs)
	: m_prefs(prefs)
	, m_blankIcon(ICON_SIZE_X, ICON_SIZE_Y)
{
	// QPixmap starts with uninitialized data; make "blank" be blank
	m_blankIcon.fill(Qt::transparent);

	refreshIcons();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

IconLoader::~IconLoader()
{
}


//-------------------------------------------------
//  refreshIcons
//-------------------------------------------------

void IconLoader::refreshIcons()
{
	// clear out the icon map
	m_icon_map.clear();

	// set up the asset finder
	m_assetFinder.setPaths(m_prefs, Preferences::global_path_type::ICONS);
}


//-------------------------------------------------
//  getIcon
//-------------------------------------------------

const QPixmap &IconLoader::getIcon(const info::machine &machine)
{
	const QPixmap *result = getIconByName(machine.name());
	if (!result && machine.clone_of())
		result = getIconByName(machine.clone_of()->name());	
	return result ? *result : m_blankIcon;
}


//-------------------------------------------------
//  getIconByName
//-------------------------------------------------

const QPixmap *IconLoader::getIconByName(const QString &iconName)
{
	// look up the result in the icon map
	auto iter = m_icon_map.find(iconName);

	// did we come up empty?
	if (iter == m_icon_map.end())
	{
		// we have not tried to load this icon - first determine the real file name
		QString iconFileName = iconName + ".ico";

		// and try to load the icon
		std::unique_ptr<QIODevice> stream = m_assetFinder.findAsset(iconFileName);
		if (stream)
		{
			// read the bytes
			QByteArray byteArray = stream->readAll();

			// we've found an entry - try to load the icon; note that while this can
			// fail, we want to memoize the failure
			std::optional<QPixmap> icon = LoadIcon(std::move(iconFileName), byteArray);

			// record the result in the icon map
			iter = m_icon_map.emplace(iconName, std::move(icon)).first;
		}
	}

	// if we found (or added) something, return it
	const QPixmap *result = (iter != m_icon_map.end() && iter->second.has_value())
		? &*iter->second
		: nullptr;
	return result;
}


//-------------------------------------------------
//  LoadIcon
//-------------------------------------------------

std::optional<QPixmap> IconLoader::LoadIcon(QString &&icon_name, const QByteArray &byteArray)
{
	// load the icon into a file
	QPixmap image;
	return image.loadFromData(byteArray)
		? image.scaled(ICON_SIZE_X, ICON_SIZE_Y)
		: std::optional<QPixmap>();
}
