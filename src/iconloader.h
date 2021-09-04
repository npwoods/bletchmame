/***************************************************************************

	iconloader.h

	Icon management

***************************************************************************/

#pragma once

#ifndef ICONLOADER_H
#define ICONLOADER_H

#include <QPixmap>

#include <optional>
#include <memory>
#include <unordered_map>

#include "info.h"
#include "assetfinder.h"

class Preferences;

// ======================> IIconLoader

class IIconLoader
{
public:
	virtual const QPixmap &getIcon(const info::machine &machine) = 0;
};


// ======================> IconLoader

class IconLoader : public IIconLoader
{
public:
	// ctor / dtor
	IconLoader(Preferences &prefs);
	~IconLoader();

	// methods
	void refreshIcons();
	virtual const QPixmap &getIcon(const info::machine &machine) final;

private:
	Preferences &										m_prefs;
	std::unordered_map<QString, std::optional<QPixmap>>	m_icon_map;
	AssetFinder											m_assetFinder;
	QPixmap												m_blankIcon;

	const QPixmap *getIconByName(const QString &iconName);
	static std::optional<QPixmap> loadIcon(const QByteArray &byteArray);
	static void setProperDevicePixelRatio(QPixmap &pixmap);
};

#endif // ICONLOADER_H
