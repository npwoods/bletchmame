/***************************************************************************

	assetfinder.cpp

	Logic for finding assets (icons/snaps) by path

***************************************************************************/

// bletchmame headers
#include "assetfinder.h"
#include "perfprofiler.h"

// dependency headers
#include <quazip.h>
#include <quazipfile.h>


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
	QuaZip														m_zip;
	std::optional<std::unordered_map<std::uint32_t, QString>>	m_crc32Map;

	bool findFileInZip(const QString &fileName, std::optional<std::uint32_t> crc32)
	{
		// if we have a crc32, we have an opportunity to lookup by CRC-32
		if (crc32)
		{
			// did we build the CRC-32 map?
			if (!m_crc32Map)
				buildCrc32Map();

			// perform a file lookup by CRC-32
			auto iter = m_crc32Map->find(*crc32);
			if (iter != m_crc32Map->end() && setCurrentFile(iter->second, QuaZip::CaseSensitivity::csSensitive))
				return true;
		}

		// perform a file lookup by name
		return setCurrentFile(fileName, QuaZip::CaseSensitivity::csInsensitive);
	}

	void buildCrc32Map()
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		m_crc32Map.emplace();
		QList<QuaZipFileInfo64> allFiles = m_zip.getFileInfoList64();
		for (QuaZipFileInfo64 &fi : allFiles)
			m_crc32Map->emplace(fi.crc, std::move(fi.name));
	}

	bool setCurrentFile(const QString &fileName, QuaZip::CaseSensitivity cs)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		return m_zip.setCurrentFile(fileName, cs);
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
	ProfilerScope prof(CURRENT_FUNCTION);
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
