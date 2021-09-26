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

#include "assetfinder.h"
#include "info.h"
#include "prefs.h"


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
	QPixmap getIcon(const info::machine &machine, std::optional<bool> showAuditAdornment = false);

private:
	typedef std::tuple<std::u8string, std::u8string> IconMapKey;

	struct IconMapHash
	{
		std::size_t operator()(const IconMapKey &x) const noexcept;
	};

	struct IconMapEquals
	{
		bool operator()(const IconMapKey &lhs, const IconMapKey &rhs) const;
	};

	typedef std::unordered_map<IconMapKey, std::optional<QPixmap>, IconMapHash, IconMapEquals> IconMap;
	
	const Preferences &	m_prefs;
	IconMap				m_iconMap;
	AssetFinder			m_assetFinder;
	QPixmap				m_blankIcon;

	std::optional<QPixmap> loadIcon(std::u8string_view iconName);
	static void adornIcon(QPixmap &basePixmap, const QPixmap &adornmentPixmap);
	static std::u8string_view getAdornmentForAuditStatus(AuditStatus machineAuditStatus);
};


#endif // ICONLOADER_H
