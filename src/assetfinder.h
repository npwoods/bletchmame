/***************************************************************************

	assetfinder.h

	Logic for finding assets (icons/snaps) by path

***************************************************************************/

#ifndef ASSETFINDER_H
#define ASSETFINDER_H

// bletchmame headers
#include "prefs.h"

// Qt headers
#include <QStringList>
#include <QIODevice>

// standard headers
#include <vector>


// ======================> AssetFinder

class AssetFinder
{
public:
	// ctor/dtor
	AssetFinder();
	AssetFinder(QStringList &&paths);
	AssetFinder(const Preferences &prefs, Preferences::global_path_type pathType);
	AssetFinder(const AssetFinder &) = delete;
	AssetFinder(AssetFinder &&) = delete;
	~AssetFinder();

	// methods
	void setPaths(QStringList &&paths);
	void setPaths(const Preferences &prefs, Preferences::global_path_type pathType);
	std::unique_ptr<QIODevice> findAsset(const QString &fileName, std::optional<std::uint32_t> crc32 = { }) const;
	std::optional<QByteArray> findAssetBytes(const QString &fileName, std::optional<std::uint32_t> crc32 = { }) const;

	// statics
	static bool isValidArchive(const QString &path);

private:
	class Lookup;
	class DirectoryLookup;
	class ZipFileLookup;
	class SevenZipFileLookup;

	// members
	std::vector<std::unique_ptr<Lookup>> m_lookups;
};


#endif // ASSETFINDER_H
