/***************************************************************************

	importmameinijob.cpp

	Business logic for importing MAME INI

***************************************************************************/

// bletchmame headers
#include "importmameinijob.h"
#include "iniparser.h"


//**************************************************************************
//  TYPES
//**************************************************************************

// ======================> GlobalPathEntry

class ImportMameIniJob::GlobalPathEntry : public ImportMameIniJob::Entry
{
public:
	GlobalPathEntry(Preferences &prefs, Preferences::global_path_type pathType, QString &&path, bool canReplace, ImportAction importAction);

	// virtuals
	virtual QString labelDisplayText() const override;
	virtual QString valueDisplayText() const override;
	virtual bool canAugment() const override;
	virtual bool canReplace() const override;
	virtual void doAugment() override;
	virtual void doReplace() override;

private:
	Preferences &					m_prefs;
	Preferences::global_path_type	m_pathType;
	QString							m_path;
	bool							m_canReplace;
};


// ======================> RawIniSettings

struct ImportMameIniJob::RawIniSettings
{
	std::array<QString, util::enum_count<Preferences::global_path_type>()>	m_paths;
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ImportMameIniJob::ImportMameIniJob(Preferences &prefs)
	: m_prefs(prefs)
{
}


//-------------------------------------------------
//  loadMameIni
//-------------------------------------------------

bool ImportMameIniJob::loadMameIni(const QString &fileName)
{
	// open up the file
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	// read the INI
	RawIniSettings rawSettings = extractRawIniSettings(file);

	// identify the base directory
	QDir baseDir = QFileInfo(fileName).absoluteDir();

	// copy the results into entries
	m_entries.clear();
	m_entries.reserve(rawSettings.m_paths.size());
	std::vector<QFileInfo> importPathFileInfos;
	for (Preferences::global_path_type pathType : util::all_enums<Preferences::global_path_type>())
	{
		// MAME's path separator is ';' even on non-Windows platforms
		QStringList importPaths = rawSettings.m_paths[(size_t)pathType].split(';');

		// get all fileinfos
		importPathFileInfos.clear();
		for (QString &importPath : importPaths)
		{
			// no whitespace please...
			importPath = importPath.trimmed();
			if (!importPath.isEmpty())
			{
				// register this file info
				importPathFileInfos.emplace_back(baseDir, importPath);

				// no more than one path when this is multipath
				if (!supportsMultiplePaths(pathType))
					break;
			}
		}

		// now go back and process the file infos
		for (const QFileInfo &importPathFileInfo : importPathFileInfos)
		{
			// determine the import action
			ImportAction importAction;
			if (isPathPresent(pathType, importPathFileInfo))
				importAction = ImportAction::AlreadyPresent;
			else
				importAction = supportsMultiplePaths(pathType) ? ImportAction::Augment : ImportAction::Replace;

			// we can only replace if there is a single path
			bool canReplace = importPathFileInfos.size() <= 1;

			// and add the entry
			Entry::ptr newEntry = std::make_unique<GlobalPathEntry>(
				m_prefs,
				pathType,
				importPathFileInfo.absoluteFilePath(),
				canReplace,
				importAction);
			m_entries.push_back(std::move(newEntry));
		}
	}

	return true;
}


//-------------------------------------------------
//  apply
//-------------------------------------------------

void ImportMameIniJob::apply()
{
	for (const Entry::ptr &entry : entries())
	{
		switch (entry->importAction())
		{
		case ImportAction::Augment:
			entry->doAugment();
			break;
		case ImportAction::Replace:
			entry->doReplace();
			break;
		case ImportAction::Ignore:
		case ImportAction::AlreadyPresent:
			// do nothing
			break;
		}
	}
}


//-------------------------------------------------
//  extractRawIniSettings
//-------------------------------------------------

ImportMameIniJob::RawIniSettings ImportMameIniJob::extractRawIniSettings(QIODevice &stream)
{
	RawIniSettings result;
	IniFileParser iniParser(stream);

	QString name, value;
	while (iniParser.next(name, value))
	{
		auto iter = std::find_if(Preferences::s_globalPathInfo.begin(), Preferences::s_globalPathInfo.end(), [&name](const auto &x)
		{
			return name == x.m_emuSettingName;
		});
		if (iter != Preferences::s_globalPathInfo.end())
		{
			size_t index = iter - Preferences::s_globalPathInfo.begin();
			result.m_paths[index] = std::move(value);
		}
	}
	return result;
}


//-------------------------------------------------
//  supportsMultiplePaths
//-------------------------------------------------

bool ImportMameIniJob::supportsMultiplePaths(Preferences::global_path_type pathType)
{
	bool result;
	switch (Preferences::getPathCategory(pathType))
	{
	case Preferences::PathCategory::SingleFile:
	case Preferences::PathCategory::SingleDirectory:
		result = false;
		break;

	case Preferences::PathCategory::MultipleDirectories:
	case Preferences::PathCategory::MultipleDirectoriesOrArchives:
		result = true;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  isPathPresent
//-------------------------------------------------

bool ImportMameIniJob::isPathPresent(Preferences::global_path_type pathType, const QFileInfo &fi) const
{
	// identify paths from our prefs
	QStringList prefsPaths = m_prefs.getSplitPaths(pathType);

	// find this file info
	auto iter = std::find_if(prefsPaths.begin(), prefsPaths.end(), [&fi](const QString &x)
	{
		return fi == QFileInfo(x);
	});
	return iter != prefsPaths.end();
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

ImportMameIniJob::Entry::Entry(ImportAction importAction)
	: m_importAction(importAction)
{
}


//-------------------------------------------------
//  GlobalPathEntry ctor
//-------------------------------------------------

ImportMameIniJob::GlobalPathEntry::GlobalPathEntry(Preferences &prefs, Preferences::global_path_type pathType, QString &&path, bool canReplace, ImportAction importAction)
	: Entry(importAction)
	, m_prefs(prefs)
	, m_pathType(pathType)
	, m_path(std::move(path))
	, m_canReplace(canReplace)
{
}


//-------------------------------------------------
//  GlobalPathEntry::labelDisplayText
//-------------------------------------------------

QString ImportMameIniJob::GlobalPathEntry::labelDisplayText() const
{
	return Preferences::s_globalPathInfo[(size_t)m_pathType].m_description;
}


//-------------------------------------------------
//  GlobalPathEntry::valueDisplayText
//-------------------------------------------------

QString ImportMameIniJob::GlobalPathEntry::valueDisplayText() const
{
	return QDir::toNativeSeparators(m_path);
}


//-------------------------------------------------
//  GlobalPathEntry::canAugment
//-------------------------------------------------

bool ImportMameIniJob::GlobalPathEntry::canAugment() const
{
	return supportsMultiplePaths(m_pathType);
}


//-------------------------------------------------
//  GlobalPathEntry::canReplace
//-------------------------------------------------

bool ImportMameIniJob::GlobalPathEntry::canReplace() const
{
	return m_canReplace;
}


//-------------------------------------------------
//  GlobalPathEntry::doAugment
//-------------------------------------------------

void ImportMameIniJob::GlobalPathEntry::doAugment()
{
	QStringList paths = m_prefs.getSplitPaths(m_pathType);
	paths << m_path;
	m_prefs.setGlobalPath(m_pathType, paths.join(";"));
}


//-------------------------------------------------
//  GlobalPathEntry::doReplace
//-------------------------------------------------

void ImportMameIniJob::GlobalPathEntry::doReplace()
{
	m_prefs.setGlobalPath(m_pathType, m_path);
}
