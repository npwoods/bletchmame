/***************************************************************************

	assetfinder.cpp

	Logic for finding assets (icons/snaps) by path

***************************************************************************/

// bletchmame headers
#include "assetfinder.h"
#include "perfprofiler.h"
#include "7zip.h"

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


// ======================> AssetFinder::ZipFileLookup
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

	static Lookup::ptr tryOpen(const QString &path)
	{
		auto lookup = std::make_unique<ZipFileLookup>(path);
		return lookup->openZip()
			? std::move(lookup)
			: nullptr;
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


// ======================> AssetFinder::SevenZipFileLookup
class AssetFinder::SevenZipFileLookup : public AssetFinder::Lookup
{
public:
	bool open(const QString &path)
	{
		return m_7zipFile.open(path);
	}

	virtual std::unique_ptr<QIODevice> getAsset(const QString &fileName, std::optional<std::uint32_t> crc32) override
	{
		// try the CRC-32 if present
		std::unique_ptr<QIODevice> file;
		if (crc32)
			file = m_7zipFile.get(*crc32);

		// otherwise try the file name
		if (!file)
			file = m_7zipFile.get(fileName);
		return file;
	}

	static Lookup::ptr tryOpen(const QString &path)
	{
		auto lookup = std::make_unique<SevenZipFileLookup>();
		return lookup->open(path)
			? std::move(lookup)
			: nullptr;
	}

private:
	SevenZipFile	m_7zipFile;
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
			// is this an archive (ZIP or 7-Zip) file?
			lookup = ZipFileLookup::tryOpen(path);
			if (!lookup)
				lookup = SevenZipFileLookup::tryOpen(path);
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
	return ZipFileLookup::tryOpen(path) || SevenZipFileLookup::tryOpen(path);
}
