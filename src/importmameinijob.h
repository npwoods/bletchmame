/***************************************************************************

	importmameinijob.h

	Business logic for importing MAME INI

***************************************************************************/

#ifndef IMPORTMAMEINIJOB_H
#define IMPORTMAMEINIJOB_H

// bletchmame headers
#include "prefs.h"

// Qt headers
#include <QString>
#include <QRegExp>

// standard headrs
#include <memory>


// ======================> ImportMameIniJob

class ImportMameIniJob
{
public:
	class Test;

	enum class ImportAction
	{
		Ignore,
		Supplement,
		Replace,
		AlreadyPresent
	};

	class Entry
	{
	public:
		typedef std::unique_ptr<Entry> ptr;

		// ctor/dtor
		Entry(ImportAction importAction);
		virtual ~Entry() = default;

		// accessors
		ImportAction importAction() const				{ return m_importAction; }
		void setImportAction(ImportAction importAction) { m_importAction = importAction; }
		bool isEditable() const							{ return m_importAction != ImportAction::AlreadyPresent; }

		// virtuals
		virtual QString labelDisplayText() const = 0;
		virtual QString valueDisplayText() const = 0;
		virtual QString explanationDisplayText() const = 0;
		virtual bool canSupplement() const = 0;
		virtual bool canReplace() const = 0;
		virtual void doSupplement() = 0;
		virtual void doReplace() = 0;

	private:
		ImportAction m_importAction;
	};

	// ctor
	ImportMameIniJob(Preferences &prefs);
	
	// methods
	bool loadMameIni(const QString &fileName);
	void apply();

	// accessors
	const std::vector<Entry::ptr> &entries() const		{ return m_entries; }
	std::vector<Entry::ptr> &entries()					{ return m_entries; }

private:
	class GlobalPathEntry;
	struct RawIniSettings;

	// variables
	Preferences &			m_prefs;
	std::vector<Entry::ptr> m_entries;

	// statics
	static RawIniSettings extractRawIniSettings(QIODevice &stream);
	static bool supportsMultiplePaths(Preferences::global_path_type pathType);
	static QString expandEnvironmentVariables(const QString &s);

	// methods
	bool isPathPresent(Preferences::global_path_type pathType, const QFileInfo &fi) const;
};

#endif // IMPORTMAMEINIJOB_H
