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

class Preferences;

class IconLoader
{
public:
	// ctor / dtor
	IconLoader(Preferences &prefs);
	~IconLoader();

	// methods
	void refreshIcons();
	const QPixmap &getIcon(const info::machine &machine);

private:
	class IconFinder;
	class DirectoryIconFinder;
	class ZipIconFinder;
	
	Preferences &													m_prefs;
	std::unordered_map<QString, std::optional<QPixmap>, util::Hash>	m_icon_map;
	std::vector<std::unique_ptr<IconFinder>>						m_finders;
	QPixmap															m_blankIcon;

	std::optional<QPixmap> LoadIcon(QString &&icon_name, const QByteArray &byteArray);
	const QPixmap *getIconByName(const QString &iconName);
};

#endif // ICONLOADER_H
