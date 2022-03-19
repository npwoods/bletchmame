/***************************************************************************

	dialogs/importmameini.h

	Dialog for importing MAME.ini files

***************************************************************************/

#ifndef DIALOGS_IMPORTMAMEINI_H
#define DIALOGS_IMPORTMAMEINI_H

// bletchmame headers
#include "prefs.h"

// Qt headers
#include <QDialog>
#include <QAbstractListModel>


QT_BEGIN_NAMESPACE
namespace Ui { class ImportMameIniDialog; }
QT_END_NAMESPACE

// ======================> ImportMameIniDialog

class ImportMameIniDialog : public QDialog
{
	Q_OBJECT

public:
	class Test;

	// ctor/dtor
	ImportMameIniDialog(Preferences &prefs, QWidget *parent = nullptr);
	~ImportMameIniDialog();

	// methods
	bool loadMameIni(const QString &fileName);
	void apply();
	bool hasImportables() const;

private:
	enum class ImportAction
	{
		Ignore,
		Supplement,
		Replace,
		AlreadyPresent
	};

	class TableModel;
	class ActionColumnItemDelegate;

	// variables
	std::unique_ptr<Ui::ImportMameIniDialog>    m_ui;

	// methods
	TableModel &tableModel();
	const TableModel &tableModel() const;
	void updateButtonsEnabled();
	void tableViewCurrentChanged(const QModelIndex &current);

	// statics
	static QString importActionString(ImportAction importAction);
};

#endif // DIALOGS_IMPORTMAMEINI_H
