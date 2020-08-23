/***************************************************************************

    dialogs/paths.h

    Paths dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_PATHS_H
#define DIALOGS_PATHS_H

#include <QDialog>
#include <QWidget>
#include <QString>
#include <memory>
#include <vector>

#include "prefs.h"

QT_BEGIN_NAMESPACE
namespace Ui { class PathsDialog; }
QT_END_NAMESPACE

// ======================> PathsDialog

class PathsDialog : public QDialog
{
	Q_OBJECT
public:
	class Test;

	PathsDialog(QWidget &parent, Preferences &prefs);
	~PathsDialog();

	std::vector<Preferences::global_path_type> persist();

	static QString browseForPathDialog(QWidget &parent, Preferences::global_path_type type, const QString &default_path);

private slots:
	void on_comboBox_currentIndexChanged(int index);
	void on_browseButton_clicked();
	void on_insertButton_clicked();
	void on_deleteButton_clicked();
	void on_listView_activated(const QModelIndex &index);

private:
	class PathListModel;

	static const size_t PATH_COUNT = (size_t)Preferences::global_path_type::COUNT;
	static const QStringList s_combo_box_strings;

	std::unique_ptr<Ui::PathsDialog>				m_ui;
	Preferences &									m_prefs;

	std::array<QString, PATH_COUNT>					m_pathLists;
	std::optional<Preferences::global_path_type>	m_listViewModelCurrentPath;

	// methods
	void updateCurrentPathList();
	void extractPathsFromListView();
	void updateButtonsEnabled();
	Preferences::global_path_type getCurrentPath() const;
	bool browseForPath(int item);
	PathListModel &listModel();

	// static methods
	static QStringList buildComboBoxStrings();
	static bool isFilePathType(Preferences::global_path_type type);
	static bool isDirPathType(Preferences::global_path_type type);
};


#endif // DIALOGS_PATHS_H
