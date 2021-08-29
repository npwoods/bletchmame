/***************************************************************************

	assetfinder.h

	Logic for finding assets (icons/snaps) by path

***************************************************************************/

#ifndef ASSETFINDER_H
#define ASSETFINDER_H

#include <QStringList>
#include <QIODevice>

#include <vector>

#include "prefs.h"

// ======================> AssetFinder

class AssetFinder
{
public:
	// ctor/dtor
	AssetFinder();
	AssetFinder(const Preferences &prefs, Preferences::global_path_type pathType);
	AssetFinder(const AssetFinder &) = delete;
	AssetFinder(AssetFinder &&) = delete;
	~AssetFinder();

	// methods
	void setPaths(QStringList &&paths);
	void setPaths(const Preferences &prefs, Preferences::global_path_type pathType);
	std::unique_ptr<QIODevice> findAsset(const QString &fileName) const;
	QByteArray findAssetBytes(const QString &fileName) const;

	// statics
	static bool isValidArchive(const QString &path);

private:
	class Lookup;
	class DirectoryLookup;
	class ZipFileLookup;

	// members
	std::vector<std::unique_ptr<Lookup>> m_lookups;
};


#endif // ASSETFINDER_H
