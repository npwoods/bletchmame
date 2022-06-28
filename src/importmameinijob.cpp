/***************************************************************************

	importmameinijob.cpp

	Business logic for importing MAME INI

***************************************************************************/

// bletchmame headers
#include "importmameinijob.h"
#include "iniparser.h"

// windows headers
#ifdef Q_OS_WINDOWS
#include <windows.h>
#endif // Q_OS_WINDOWS


//**************************************************************************
//  TYPES
//**************************************************************************

// ======================> GlobalPathEntry

class ImportMameIniJob::GlobalPathEntry : public ImportMameIniJob::Entry
{
public:
	GlobalPathEntry(Preferences &prefs, Preferences::global_path_type pathType, QString &&path, bool canReplace, bool pathAlreadyPresent);

	// virtuals
	virtual QString labelDisplayText() const override;
	virtual QString valueDisplayText() const override;
	virtual QString explanationDisplayText() const override;
	virtual bool canSupplement() const override;
	virtual bool canReplace() const override;
	virtual void doSupplement() override;
	virtual void doReplace() override;
	virtual void persistPreference(Preferences &prefs) override;

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
		QStringList importPaths = splitPathList(rawSettings.m_paths[(size_t)pathType]);

		// get all fileinfos
		importPathFileInfos.clear();
		for (QString &importPath : importPaths)
		{
			// expand environment variable
			importPath = expandEnvironmentVariables(importPath);

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
			// we can only replace if there is a single path
			bool canReplace = importPathFileInfos.size() <= 1;

			// is this path already present?
			bool pathAlreadyPresent = isPathPresent(pathType, importPathFileInfo);

			// create the entry
			Entry::ptr newEntry = std::make_unique<GlobalPathEntry>(
				m_prefs,
				pathType,
				importPathFileInfo.absoluteFilePath(),
				canReplace,
				pathAlreadyPresent);

			// check the preference
			if (!pathAlreadyPresent)
			{
				std::optional<MameIniImportActionPreference> importActionPref = m_prefs.getMameIniImportActionPreference(pathType);
				if (importActionPref)
				{
					switch (*importActionPref)
					{
					case MameIniImportActionPreference::Ignore:
						newEntry->setImportAction(ImportAction::Ignore);
						break;
					case MameIniImportActionPreference::Supplement:
						if (newEntry->canSupplement())
							newEntry->setImportAction(ImportAction::Supplement);
						break;
					case MameIniImportActionPreference::Replace:
						if (newEntry->canReplace())
							newEntry->setImportAction(ImportAction::Replace);
						break;
					}
				}
			}

			// and add the entry
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
		case ImportAction::Supplement:
			entry->doSupplement();
			break;
		case ImportAction::Replace:
			entry->doReplace();
			break;
		case ImportAction::Ignore:
		case ImportAction::AlreadyPresent:
			// do nothing
			break;
		}

		entry->persistPreference(m_prefs);
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
		auto iter = std::ranges::find_if(Preferences::s_globalPathInfo, [&name](const auto &x)
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
	auto iter = std::ranges::find_if(prefsPaths, [&fi](const QString &x)
	{
		return areFileInfosEquivalent(fi, QFileInfo(x));
	});
	return iter != prefsPaths.end();
}


//-------------------------------------------------
//  expandEnvironmentVariables
//-------------------------------------------------

QString ImportMameIniJob::expandEnvironmentVariables(const QString &s)
{
	QString r;
#ifdef Q_OS_WINDOWS
	{
		bool done = false;
		DWORD tryLength = 4;

		while (!done)
		{
			// resize the string to what we think the length should be
			r.resize(tryLength);

			// call ExpandEnvironmentStringsW
			DWORD actualLength = ::ExpandEnvironmentStringsW(
				(LPCWSTR)s.constData(),
				(LPWSTR)r.data(),
				tryLength + 1);

			// if it errors; just return what we were passed
			if (actualLength == 0)
				return s;

			// was our buffer sufficient?
			if (actualLength - 1 > tryLength)
			{
				// if not, try again
				tryLength = actualLength - 1;
			}
			else
			{
				// it was - resize the result and we're done
				done = true;
				r.resize(actualLength - 1);
			}
		}
	}
#else // !Q_OS_WINDOWS
	{
		QRegExp env_var("\\$([A-Za-z0-9_]+)");
		int i;
		r = s;

		while ((i = env_var.indexIn(r)) != -1)
		{
			QByteArray value(qgetenv(env_var.cap(1).toLatin1().data()));
			if (value.size() > 0)
			{
				r.remove(i, env_var.matchedLength());
				r.insert(i, value);
			}
			else
				break;
		}
	}
#endif // Q_OS_WINDOWS
	return r;
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

ImportMameIniJob::GlobalPathEntry::GlobalPathEntry(Preferences &prefs, Preferences::global_path_type pathType, QString &&path, bool canReplace, bool pathAlreadyPresent)
	: Entry(ImportAction::Ignore)
	, m_prefs(prefs)
	, m_pathType(pathType)
	, m_path(std::move(path))
	, m_canReplace(canReplace)
{
	if (pathAlreadyPresent)
		setImportAction(ImportAction::AlreadyPresent);
	else if (canSupplement())
		setImportAction(ImportAction::Supplement);
	else
		setImportAction(ImportAction::Replace);
}


//-------------------------------------------------
//  GlobalPathEntry::labelDisplayText
//-------------------------------------------------

QString ImportMameIniJob::GlobalPathEntry::labelDisplayText() const
{
	return Preferences::s_globalPathInfo[(size_t)m_pathType].m_description;
}


//-------------------------------------------------
//  GlobalPathEntry::explanationDisplayText
//-------------------------------------------------

QString ImportMameIniJob::GlobalPathEntry::explanationDisplayText() const
{
	// get the current setting; this can drive the text
	const QString &currentValue = m_prefs.getGlobalPath(m_pathType);

	// build the description
	QString text = "The MAME.ini contains the path <b>%1</b> for %2.  ";
	if (currentValue.isEmpty())
		text += "BletchMAME has no path of this type.  ";
	else
		text += "BletchMAME has paths <b>%3</b> for this setting.  ";
	switch (importAction())
	{
	case ImportAction::Ignore:
		text += "Choosing <b>Ignore</b> will ignore this path from MAME.ini.";
		break;
	case ImportAction::Supplement:
		text += "Choosing <b>Supplement</b> will add this path to BletchMAME's existing path.";
		break;
	case ImportAction::Replace:
		text += "Choosing <b>Replace</b> will replace BletchMAME's path with the MAME.INI path.";
		break;
	case ImportAction::AlreadyPresent:
		text += "The path from MAME.ini is already present.";
		break;
	default:
		throw false;
	}

	// return the description
	return text.arg(
		QDir::toNativeSeparators(m_path),
		Preferences::s_globalPathInfo[(size_t)m_pathType].m_description,
		QDir::toNativeSeparators(currentValue));
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

bool ImportMameIniJob::GlobalPathEntry::canSupplement() const
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
//  GlobalPathEntry::doSupplement
//-------------------------------------------------

void ImportMameIniJob::GlobalPathEntry::doSupplement()
{
	QStringList paths = m_prefs.getSplitPaths(m_pathType);
	paths << m_path;
	m_prefs.setGlobalPath(m_pathType, joinPathList(paths));
}


//-------------------------------------------------
//  GlobalPathEntry::doReplace
//-------------------------------------------------

void ImportMameIniJob::GlobalPathEntry::doReplace()
{
	m_prefs.setGlobalPath(m_pathType, m_path);
}


//-------------------------------------------------
//  GlobalPathEntry::doReplace
//-------------------------------------------------

void ImportMameIniJob::GlobalPathEntry::persistPreference(Preferences &prefs)
{
	std::optional<MameIniImportActionPreference> importPref;
	switch (importAction())
	{
	case ImportAction::Ignore:
		importPref = MameIniImportActionPreference::Ignore;
		break;
	case ImportAction::Replace:
		importPref = MameIniImportActionPreference::Replace;
		break;
	case ImportAction::Supplement:
		importPref = MameIniImportActionPreference::Supplement;
		break;
	default:
		// do nothing
		break;
	}

	if (importPref)
		m_prefs.setMameIniImportActionPreference(m_pathType, importPref);
}
