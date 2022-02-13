/***************************************************************************

	iconloader.cpp

	Icon management

***************************************************************************/

// bletchmame headers
#include "iconloader.h"
#include "perfprofiler.h"
#include "prefs.h"

// Qt headers
#include <QPainter>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define ICON_SIZE	16


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

IconLoader::IconLoader(Preferences &prefs)
	: m_prefs(prefs)
	, m_blankIcon(ICON_SIZE, ICON_SIZE)
{
	// QPixmap starts with uninitialized data; make "blank" be blank
	m_blankIcon.fill(Qt::transparent);
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
	m_iconMap.clear();

	// set up the asset finder
	m_assetFinder.setPaths(m_prefs, Preferences::global_path_type::ICONS);
}


//-------------------------------------------------
//  getIcon
//-------------------------------------------------

std::optional<QPixmap> IconLoader::getIcon(std::u8string_view iconName, std::u8string_view adornment)
{
	ProfilerScope prof(CURRENT_FUNCTION);
	std::optional<QPixmap> result;

	// set up a key that captures both the icon name and adornment
	auto key = std::make_tuple(std::u8string(iconName), std::u8string(adornment));

	// look up the result in the icon map
	auto iter = m_iconMap.find(key);

	// did we find something?
	if (iter != m_iconMap.end())
	{
		// we found something
		result = iter->second;
	}
	else if (iconName.empty() && adornment.empty())
	{
		// blank icon; this is easy
		result = m_blankIcon;
	}
	else
	{
		// we have to load something that ends up in the map
		if (adornment.empty())
		{
			// load this unadorned icon
			result = loadIcon(iconName);
		}
		else
		{
			// this is an icon requiring adornment; first try to get the base icon because
			// that is more likely to fail
			std::optional<QPixmap> baseIcon = getIcon(iconName);
			if (baseIcon)
			{
				std::optional<QPixmap> adornmentIcon = getIcon(adornment);
				if (adornmentIcon)
				{
					result = *baseIcon;
					adornIcon(*result, *adornmentIcon);
				}
			}
		}

		// be slightly careful regarding what we put into the map
		if (result || adornment.empty())
			m_iconMap.emplace(std::move(key), result);
	}
	return result;
}


//-------------------------------------------------
//  getIcon
//-------------------------------------------------

std::optional<QPixmap> IconLoader::getIcon(const QString &iconName, std::u8string_view adornment)
{
	std::u8string iconNameU8 = util::toU8String(iconName);
	return getIcon(iconNameU8, adornment);
}


//-------------------------------------------------
//  loadIcon
//-------------------------------------------------

std::optional<QPixmap> IconLoader::loadIcon(std::u8string_view iconName)
{
	// set up the filename
	QString iconFileName = util::toQString(iconName);
	if (!iconFileName.endsWith(".ico"))
		iconFileName += ".ico";

	// get the byte array
	std::optional<QByteArray> byteArray;
	if (iconName[0] == ':')
	{
		// it is a resource; we expect this to succeed
		QFile iconFile(iconFileName);
		if (!iconFile.open(QIODevice::ReadOnly))
			throw false;
		byteArray = iconFile.readAll();
	}
	else
	{
		// not a resource, use the asset finder
		byteArray = m_assetFinder.findAssetBytes(iconFileName);
	}

	// we tried to get a byte array, now try for a pixmap
	std::optional<QPixmap> result;
	if (byteArray)
	{
		QPixmap pixmap;
		if (pixmap.loadFromData(*byteArray))
		{
			// we've load the icon; normalize it
			setPixmapDevicePixelRatioToFit(pixmap, ICON_SIZE);

			// and return it
			result = std::move(pixmap);
		}
	}
	return result;
}


//-------------------------------------------------
//  adornIcon
//-------------------------------------------------

void IconLoader::adornIcon(QPixmap &basePixmap, const QPixmap &ornamentPixmap)
{
	// identify the target rectangle on basePixmap
	qreal basePixmapDevicePixelRatio = basePixmap.devicePixelRatio();
	qreal basePixmapWidth = basePixmap.width() / basePixmapDevicePixelRatio;
	qreal basePixmapHeight = basePixmap.height() / basePixmapDevicePixelRatio;
	QRectF targetRect(basePixmapWidth / 2, basePixmapHeight / 2, basePixmapWidth / 2, basePixmapHeight / 2);

	// and the sourceRect on ornamentPixmap
	QRectF sourceRect(0, 0, ornamentPixmap.width(), ornamentPixmap.height());

	// and draw the pixmap
	QPainter painter(&basePixmap);
	painter.drawPixmap(targetRect, ornamentPixmap, sourceRect);
}


//-------------------------------------------------
//  getIcon
//-------------------------------------------------

QPixmap IconLoader::getIcon(const info::machine &machine, std::optional<bool> showAuditAdornment)
{
	using namespace std::literals;

	ProfilerScope prof(CURRENT_FUNCTION);

	// do we need to get audit adornment from prefs?
	bool actualShowAuditAdornment = showAuditAdornment.has_value()
		? *showAuditAdornment
		: m_prefs.getAuditingState() != Preferences::AuditingState::Disabled;

	// identify the correct adornment
	std::u8string_view adornment = actualShowAuditAdornment
		? getAdornmentForAuditStatus(m_prefs.getMachineAuditStatus(machine.name()))
		: u8""sv;

	// look up the correct icon
	std::optional<QPixmap> result;
	std::optional<info::machine> thisMachine = machine;
	while (!result && thisMachine)
	{
		std::u8string name = util::toU8String(thisMachine->name().toUtf8());
		result = getIcon(name, adornment);
		thisMachine = thisMachine->clone_of();
	}

	// if we still have not got anything, just use the adornment
	if (!result)
		result = getIcon(adornment).value();

	return *result;
}


//-------------------------------------------------
//  getIcon
//-------------------------------------------------

std::optional<QPixmap> IconLoader::getIcon(const software_list::software &software)
{
	using namespace std::literals;

	ProfilerScope prof(CURRENT_FUNCTION);
	std::optional<QPixmap> result;

	if (m_prefs.getAuditingState() != Preferences::AuditingState::Disabled)
	{
		const QString &softwareListName = software.parent().name();
		AuditStatus status = m_prefs.getSoftwareAuditStatus(softwareListName, software.name());
		std::u8string_view adornment = getAdornmentForAuditStatus(status);

		// try to find an icon using a few techniques
		result = getIcon(QString("%1/%2").arg(softwareListName, softwareListName), adornment);
		if (!result)
			result = getIcon(QString("%1/%2").arg(softwareListName, softwareListName), adornment);
		if (!result)
			result = getIcon(adornment);
	}

	return result;
}


//-------------------------------------------------
//  getAdornmentForAuditStatus
//-------------------------------------------------

std::u8string_view IconLoader::getAdornmentForAuditStatus(AuditStatus machineAuditStatus)
{
	using namespace std::literals;

	std::u8string_view result;
	switch (machineAuditStatus)
	{
	case AuditStatus::Unknown:
		result = u8":/resources/rom_unknown.ico"sv;
		break;

	case AuditStatus::Found:
		result = u8""sv;
		break;

	case AuditStatus::Missing:
		result = u8":/resources/rom_missing.ico"sv;
		break;

	case AuditStatus::MissingOptional:
		result = u8":/resources/rom_missing_optional.ico"sv;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  IconMapHash::operator()
//-------------------------------------------------

std::size_t IconLoader::IconMapHash::operator()(const IconMapKey &x) const noexcept
{
	return std::hash<std::u8string>{}(std::get<0>(x))
		^ std::hash<std::u8string>{}(std::get<1>(x));
}
