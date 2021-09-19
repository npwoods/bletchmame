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
#include "audit.h"

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
	struct IconMapHash
	{
		std::size_t operator()(const std::tuple<QString, AuditStatus> &x) const noexcept;
	};

	struct IconMapEquals
	{
		bool operator()(const std::tuple<QString, AuditStatus> &lhs, const std::tuple<QString, AuditStatus> &rhs) const;
	};

	typedef std::unordered_map<std::tuple<QString, AuditStatus>, std::optional<QPixmap>, IconMapHash, IconMapEquals> IconMap;
	
	Preferences &	m_prefs;
	IconMap			m_icon_map;
	AssetFinder		m_assetFinder;
	QPixmap			m_unknownRomStatusIcon;
	QPixmap			m_blankIcon;
	QPixmap			m_missingRomIcon;
	QPixmap			m_optionalMissingRomIcon;

	const QPixmap &defaultIcon(AuditStatus status) const;
	const QPixmap *getIconByName(const QString &machineName, AuditStatus status);
	std::optional<QPixmap> loadIcon(const QByteArray &byteArray, AuditStatus status);
	static void loadBuiltinIcon(QPixmap &pixmap, const QString &fileName);
	static void adornIcon(QPixmap &basePixmap, const QPixmap &ornamentPixmap);
};


#endif // ICONLOADER_H
