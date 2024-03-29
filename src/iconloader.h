/***************************************************************************

	iconloader.h

	Icon management

***************************************************************************/

#pragma once

#ifndef ICONLOADER_H
#define ICONLOADER_H

// bletchmame headers
#include "assetfinder.h"
#include "info.h"
#include "prefs.h"
#include "softwarelist.h"

// Qt headers
#include <QPixmap>

// standard headers
#include <optional>
#include <memory>
#include <unordered_map>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> IconLoader

class IconLoader
{
public:
	// ctor / dtor
	IconLoader(Preferences &prefs);
	~IconLoader();

	// methods
	void refreshIcons();
	std::optional<QPixmap> getIcon(std::u8string_view iconName, std::u8string_view adornment = { });
	std::optional<QPixmap> getIcon(const QString &iconName, std::u8string_view adornment = { });
	QPixmap getIcon(const info::machine &machine, std::optional<bool> showAuditAdornment = false);
	std::optional<QPixmap> getIcon(const software_list::software &software);

	// statics
	static std::u8string_view getAdornmentForAuditStatus(AuditStatus machineAuditStatus);

private:
	typedef std::tuple<std::u8string, std::u8string> IconMapKey;

	struct IconMapHash
	{
		std::size_t operator()(const IconMapKey &x) const noexcept;
	};

	typedef std::unordered_map<IconMapKey, std::optional<QPixmap>, IconMapHash> IconMap;
	
	const Preferences &	m_prefs;
	IconMap				m_iconMap;
	AssetFinder			m_assetFinder;
	QPixmap				m_blankIcon;

	std::optional<QPixmap> loadIcon(std::u8string_view iconName);
	static void adornIcon(QPixmap &basePixmap, const QPixmap &adornmentPixmap);
};


#endif // ICONLOADER_H
