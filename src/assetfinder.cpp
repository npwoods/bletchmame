/***************************************************************************

	assetfinder.cpp

	Logic for finding assets (icons/snaps) by path

***************************************************************************/

#include <quazip.h>
#include <quazipfile.h>

#include "assetfinder.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> AssetFinder::Lookup
class AssetFinder::Lookup
{
public:
	typedef std::unique_ptr<Lookup> ptr;

	Lookup() = default;
	Lookup(const Lookup &) = delete;
	Lookup(Lookup &&) = default;

	virtual ~Lookup() { }
	virtual std::unique_ptr<QIODevice> getAsset(const QString &fileName, std::optional<std::uint32_t> crc32) = 0;
};


// ======================> AssetFinder::DirectoryLookup
class AssetFinder::DirectoryLookup : public AssetFinder::Lookup
{
public:
	DirectoryLookup(QString &&path)
		: m_path(std::move(path))
	{
	}

	virtual std::unique_ptr<QIODevice> getAsset(const QString &fileName, std::optional<std::uint32_t> crc32) override
	{
		return std::make_unique<QFile>(m_path + "/" + fileName);
	}

private:
	QString		m_path;
};


// ======================> IconLoader::DirectoryIconFinder
class AssetFinder::ZipFileLookup : public AssetFinder::Lookup
{
public:
	ZipFileLookup(const QString &path)
		: m_zip(path)
	{
	}

	bool openZip()
	{
		return m_zip.open(QuaZip::Mode::mdUnzip);
	}

	virtual std::unique_ptr<QIODevice> getAsset(const QString &fileName, std::optional<std::uint32_t> crc32) override
	{
		// find the file
		if (!findFileInZip(fileName, crc32))
			return { };

		// and return a QuaZipFile
		return std::make_unique<QuaZipFile>(&m_zip);
	}

private:
	QuaZip	m_zip;

	bool findFileInZip(const QString &fileName, std::optional<std::uint32_t> crc32)
	{
		for (bool more = m_zip.goToFirstFile(); more; more = m_zip.goToNextFile())
		{
			// check the file name
			QString thisFileName = m_zip.getCurrentFileName();
			if (QString::compare(fileName, thisFileName, Qt::CaseInsensitive) == 0)
				return true;

			// were we passed a CRC32, and if so does it match?
			QuaZipFileInfo64 info;
			if (crc32.has_value() && m_zip.getCurrentFileInfo(&info) && info.crc == crc32.value())
				return true;
		}

		// couldn't find the file
		return false;
	}
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AssetFinder::AssetFinder()
{
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

AssetFinder::AssetFinder(QStringList &&paths)
{
	setPaths(std::move(paths));
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

AssetFinder::AssetFinder(const Preferences &prefs, Preferences::global_path_type pathType)
{
	setPaths(prefs, pathType);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

AssetFinder::~AssetFinder()
{
}


//-------------------------------------------------
//  setPaths
//-------------------------------------------------

void AssetFinder::setPaths(QStringList &&paths)
{
	// prepare the lookups vector
	m_lookups.clear();
	m_lookups.reserve(paths.size());

	// inspect each path
	for (QString &path : paths)
	{
		QFileInfo fi(path);
		Lookup::ptr lookup;

		// based on the file info, try to create an appropriate lookup
		if (fi.isDir())
		{
			// this path segment is a directory
			lookup = std::make_unique<DirectoryLookup>(std::move(path));
		}
		else if (fi.isFile())
		{
			auto zipLookup = std::make_unique<ZipFileLookup>(path);
			if (zipLookup->openZip())
				lookup = std::move(zipLookup);
		}

		// if successful, add it
		if (lookup)
			m_lookups.push_back(std::move(lookup));
	}
}


//-------------------------------------------------
//  setPaths
//-------------------------------------------------

void AssetFinder::setPaths(const Preferences &prefs, Preferences::global_path_type pathType)
{
	// get the paths
	QStringList paths = prefs.getSplitPaths(pathType);

	// and set them
	setPaths(std::move(paths));
}


//-------------------------------------------------
//  findAsset
//-------------------------------------------------

std::unique_ptr<QIODevice> AssetFinder::findAsset(const QString &fileName, std::optional<std::uint32_t> crc32) const
{
	for (const Lookup::ptr &lookup : m_lookups)
	{
		std::unique_ptr<QIODevice> stream = lookup->getAsset(fileName, crc32);
		if (stream && stream->open(QIODevice::ReadOnly))
			return stream;
	}
	return { };
}


//-------------------------------------------------
//  findAssetBytes
//-------------------------------------------------

std::optional<QByteArray> AssetFinder::findAssetBytes(const QString &fileName, std::optional<std::uint32_t> crc32) const
{
	std::optional<QByteArray> result;
	std::unique_ptr<QIODevice> stream = findAsset(fileName, crc32);
	if (stream)
		result = stream->readAll();
	return result;
}


//-------------------------------------------------
//  isValidArchive - utility method housed here
//	to insulate rest of app from QuaZip
//-------------------------------------------------

bool AssetFinder::isValidArchive(const QString &path)
{
	QuaZip zip(path);
	return zip.open(QuaZip::Mode::mdUnzip);
}
