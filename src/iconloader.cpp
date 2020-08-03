/***************************************************************************

	iconloader.cpp

	Icon management

***************************************************************************/

#include "iconloader.h"
#include "prefs.h"
#include "quazip/quazip.h"
#include "quazip/quazipfile.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define ICON_SIZE_X 16
#define ICON_SIZE_Y 16


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> IconLoader::IconFinder
class IconLoader::IconFinder
{
public:
	IconFinder() = default;
	IconFinder(const IconFinder &) = delete;
	IconFinder(IconFinder &&) = delete;

	virtual ~IconFinder()
	{
	}

	virtual std::unique_ptr<QIODevice> getIconStream(const QString &filename) = 0;
};


// ======================> IconLoader::DirectoryIconFinder
class IconLoader::DirectoryIconFinder : public IconLoader::IconFinder
{
public:
	DirectoryIconFinder(QString &&path)
		: m_path(std::move(path))
	{
	}

	virtual std::unique_ptr<QIODevice> getIconStream(const QString &filename) override
	{
		return std::make_unique<QFile>(m_path + "/" + filename);
	}

private:
	QString		m_path;
};

// ======================> IconLoader::DirectoryIconFinder
class IconLoader::ZipIconFinder : public IconLoader::IconFinder
{
public:
	ZipIconFinder(const QString &path)
		: m_zip(path)
	{
	}

	bool openZip()
	{
		return m_zip.open(QuaZip::Mode::mdUnzip);
	}

	virtual std::unique_ptr<QIODevice> getIconStream(const QString &filename) override
	{
		// find the file
		if (!m_zip.setCurrentFile(filename))
			return { };

		// and return a QuaZipFile
		return std::make_unique<QuaZipFile>(&m_zip);
	}

private:
	QuaZip	m_zip;
};


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
	// clear ourselves out
	m_icon_map.clear();
	m_finders.clear();

	// loop through all icon paths
	QStringList paths = m_prefs.GetSplitPaths(Preferences::global_path_type::ICONS);
	for (QString &path : paths)
	{
		// try to create an appropiate path entry
		std::unique_ptr<IconFinder> iconFinder;
		QFileInfo fi(path);
		if (fi.isDir())
		{
			iconFinder = std::make_unique<DirectoryIconFinder>(std::move(path));
		}
		else if (fi.isFile())
		{
			auto zipIconFinder = std::make_unique<ZipIconFinder>(path);
			if (zipIconFinder->openZip())
				iconFinder = std::move(zipIconFinder);
		}

		// if successful, add it
		if (iconFinder)
			m_finders.push_back(std::move(iconFinder));
	}
}


//-------------------------------------------------
//  getIcon
//-------------------------------------------------

const QPixmap &IconLoader::getIcon(const info::machine &machine)
{
	const QPixmap *result = getIconByName(machine.name());
	if (!result && !machine.clone_of().isEmpty())
		result = getIconByName(machine.clone_of());	
	return result ? *result : m_blankIcon;
}


//-------------------------------------------------
//  getIconByName
//-------------------------------------------------

const QPixmap *IconLoader::getIconByName(const QString &icon_name)
{
	// look up the result in the icon map
	auto iter = m_icon_map.find(icon_name);

	// did we come up empty?
	if (iter == m_icon_map.end())
	{
		// we have not tried to load this icon - first determine the real file name
		QString icon_file_name = icon_name + ".ico";

		// and try to load it from each ZIP file
		for (const auto &finder : m_finders)
		{
			// try to get a stream and open it
			std::unique_ptr<QIODevice> stream = finder->getIconStream(icon_file_name);
			if (stream && stream->open(QIODevice::ReadOnly))
			{
				// read the bytes
				QByteArray byteArray = stream->readAll();

				// we've found an entry - try to load the icon; note that while this can
				// fail, we want to memoize the failure
				std::optional<QPixmap> icon = LoadIcon(std::move(icon_file_name), byteArray);

				// record the result in the icon map
				iter = m_icon_map.emplace(icon_name, std::move(icon)).first;
				break;
			}
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
