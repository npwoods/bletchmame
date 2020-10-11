/***************************************************************************

    dialogs/images.h

    Images (File Manager) dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_IMAGES_H
#define DIALOGS_IMAGES_H

#include <vector>
#include <memory>
#include <observable/observable.hpp>
#include <QDialog>

#include "status.h"
#include "softwarelist.h"
#include "info.h"


// pesky Win32 declaration
#ifdef LoadImage
#undef LoadImage
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class ImagesDialog; }
class QLabel;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

class Preferences;

class IImagesHost
{
public:
	virtual Preferences &getPreferences() = 0;
	virtual info::machine getMachine() = 0;
	virtual observable::value<std::vector<status::image>> &getImages() = 0;
	virtual const QString &getWorkingDirectory() const = 0;
	virtual void setWorkingDirectory(QString &&dir) = 0;
	virtual const std::vector<QString> &getRecentFiles(const QString &tag) const = 0;
	virtual std::vector<QString> getExtensions(const QString &tag) const = 0;
	virtual void createImage(const QString &tag, QString &&path) = 0;
	virtual void loadImage(const QString &tag, QString &&path) = 0;
	virtual void unloadImage(const QString &tag) = 0;
};


// ======================> ImagesDialog

class ImagesDialog : public QDialog
{
	Q_OBJECT
public:
	ImagesDialog(QWidget &parent, IImagesHost &host, bool cancellable);
	~ImagesDialog();

private:
	IImagesHost &						m_host;
	std::unique_ptr<Ui::ImagesDialog>	m_ui;
	observable::unique_subscription		m_imagesEventSubscription;

	void updateImageGrid();
	const QString &prettifyImageFileName(const software_list_collection &software_col, const QString &tag, const QString &file_name, QString &buffer, bool full_path);
	const QString *deviceInterfaceFromTag(const QString &tag);
	software_list_collection buildSoftwareListCollection() const;
	bool imageMenu(const QPushButton &button, int row);
	bool createImage(const QString &tag);
	bool loadImage(const QString &tag);
	bool loadSoftwareListPart(const software_list_collection &software_col, const QString &tag, const QString &dev_interface);
	bool unloadImage(const QString &tag);
	QString getWildcardString(const QString &tag, bool support_zip) const;
	void updateWorkingDirectory(const QString &path);

	template<class T>
	T &getOrCreateGridWidget(int row, int column, void (ImagesDialog::*setupFunc)(T &, int) = nullptr);

	void setupGridLineEdit(QLineEdit &lineEdit, int row);
	void setupGridButton(QPushButton &button, int row);
};


#endif // DIALOGS_IMAGES_H
