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

	virtual QByteArray OpenFile(const QString &filename) = 0;
};


// ======================> IconLoader::DirectoryIconFinder
class IconLoader::DirectoryIconFinder : public IconLoader::IconFinder
{
public:
	DirectoryIconFinder(QString &&path)
		: m_path(std::move(path))
	{
	}

	virtual QByteArray OpenFile(const QString &filename) override
	{
		// does the file exist?  if so, open it
		QFile file(m_path + "/" + filename);
		return file.open(QIODevice::ReadOnly)
			? file.readAll()
			: QByteArray();
	}

private:
	QString		m_path;
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
		std::unique_ptr<IconFinder> finder;
		QFileInfo fi(path);
		if (fi.isDir())
			finder = std::make_unique<DirectoryIconFinder>(std::move(path));

		// if successful, add it
		if (finder)
			m_finders.push_back(std::move(finder));
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
	const QPixmap *result = nullptr;
	auto iter = m_icon_map.find(icon_name);
	if (iter != m_icon_map.end())
	{
		// we've previously loaded (or tried to load) this icon; use our old result
		if (iter->second.has_value())
			result = &*iter->second;
	}
	else
	{
		// we have not tried to load this icon - first determine the real file name
		QString icon_file_name = icon_name + ".ico";

		// and try to load it from each ZIP file
		for (const auto &finder : m_finders)
		{
			// get the byte array
			QByteArray byteArray = finder->OpenFile(icon_file_name);
			if (byteArray.size() > 0)
			{
				// we've found an entry - try to load the icon; note that while this can
				// fail, we want to memoize the failure
				std::optional<QPixmap> icon = LoadIcon(std::move(icon_file_name), byteArray);

				// record the result in the icon map
				m_icon_map.emplace(icon_name, std::move(icon));
				break;
			}
		}
	}
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
