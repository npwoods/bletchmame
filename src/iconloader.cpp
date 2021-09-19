/***************************************************************************

	iconloader.cpp

	Icon management

***************************************************************************/

#include <QPainter>

#include "iconloader.h"
#include "prefs.h"


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

	// load the other icons
	loadBuiltinIcon(m_missingRomIcon,			":/resources/roms_missing.ico");
	loadBuiltinIcon(m_optionalMissingRomIcon,	":/resources/roms_missing_optional.ico");
	loadBuiltinIcon(m_unknownRomStatusIcon,		":/resources/roms_unknown.ico");

	// and refresh
	refreshIcons();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

IconLoader::~IconLoader()
{
}


//-------------------------------------------------
//  loadBuiltinIcon
//-------------------------------------------------

void IconLoader::loadBuiltinIcon(QPixmap &pixmap, const QString &fileName)
{
	// load the icon
	bool success = pixmap.load(fileName);
	setPixmapDevicePixelRatioToFit(pixmap, ICON_SIZE);

	// treat failure to load as a catastrophic error; its probably a build problem
	if (!success)
		throw false;
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
	// get the AuditStatus for display purposes
	AuditStatus status;
	if (m_prefs.getAuditingState() != Preferences::AuditingState::Disabled)
		status = m_prefs.getMachineAuditStatus(machine.name());
	else
		status = AuditStatus::Found;	// auditing disabled?  treat things as if they were found

	// and retrieve the icon
	const QPixmap *result = getIconByName(machine.name(), status);
	if (!result && machine.clone_of())
		result = getIconByName(machine.clone_of()->name(), status);
	return result ? *result : defaultIcon(status);
}


//-------------------------------------------------
//  defaultIcon
//-------------------------------------------------

const QPixmap &IconLoader::defaultIcon(AuditStatus status) const
{
	switch (status)
	{
	case AuditStatus::Unknown:
		return m_unknownRomStatusIcon;
	case AuditStatus::Found:
		return m_blankIcon;
	case AuditStatus::Missing:
		return m_missingRomIcon;
	case AuditStatus::MissingOptional:
		return m_optionalMissingRomIcon;
	default:
		throw false;
	}
}


//-------------------------------------------------
//  getIconByName
//-------------------------------------------------

const QPixmap *IconLoader::getIconByName(const QString &machineName, AuditStatus status)
{
	// set up a key that captures both the machine name and the status
	auto key = std::make_tuple(machineName, status);

	// look up the result in the icon map
	auto iter = m_icon_map.find(key);

	// did we come up empty?
	if (iter == m_icon_map.end())
	{
		// we have not tried to load this icon - first determine the real file name
		QString iconFileName = machineName + ".ico";

		// and try to load the icon
		std::optional<QByteArray> byteArray = m_assetFinder.findAssetBytes(iconFileName);
		if (byteArray)
		{
			// we've found an entry - try to load the icon; note that while this can
			// fail, we want to memoize the failure
			std::optional<QPixmap> icon = loadIcon(*byteArray, status);

			// record the result in the icon map
			iter = m_icon_map.emplace(std::move(key), std::move(icon)).first;
		}
	}

	// if we found (or added) something, return it
	const QPixmap *result = (iter != m_icon_map.end() && iter->second.has_value())
		? &*iter->second
		: nullptr;
	return result;
}


//-------------------------------------------------
//  loadIcon
//-------------------------------------------------

std::optional<QPixmap> IconLoader::loadIcon(const QByteArray &byteArray, AuditStatus status)
{
	std::optional<QPixmap> result;

	// load the icon into a file
	QPixmap pixmap;
	if (pixmap.loadFromData(byteArray))
	{
		// we've load the icon; normalize it
		setPixmapDevicePixelRatioToFit(pixmap, ICON_SIZE);

		// and composite the audit status icon (if appropriate)
		if (status != AuditStatus::Found)
			adornIcon(pixmap, defaultIcon(status));

		// and return it
		result = std::move(pixmap);
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
//  IconMapHash::operator()
//-------------------------------------------------

std::size_t IconLoader::IconMapHash::operator()(const std::tuple<QString, AuditStatus> &x) const noexcept
{
	return std::hash<QString>{}(std::get<0>(x))
		^ std::hash<AuditStatus>{}(std::get<1>(x));
}


//-------------------------------------------------
//  IconMapEquals::operator()
//-------------------------------------------------

bool IconLoader::IconMapEquals::operator()(const std::tuple<QString, AuditStatus> &lhs, const std::tuple<QString, AuditStatus> &rhs) const
{
	return std::get<0>(lhs) == std::get<0>(rhs)
		&& std::get<1>(lhs) == std::get<1>(rhs);
}
