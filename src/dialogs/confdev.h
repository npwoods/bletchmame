/***************************************************************************

    dialogs/confdev.h

	Configurable Devices dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_CONFDEV_H
#define DIALOGS_CONFDEV_H

#include <vector>
#include <memory>
#include <observable/observable.hpp>
#include <QDialog>
#include <QStyle>

#include "confdevmodel.h"
#include "softwarelist.h"

// pesky Win32 declaration
#ifdef LoadImage
#undef LoadImage
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class ConfigurableDevicesDialog; }
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
QT_END_NAMESPACE

class Preferences;

class IConfigurableDevicesDialogHost
{
public:
	virtual Preferences &getPreferences() = 0;
	virtual info::machine getMachine() = 0;
	virtual observable::value<std::vector<status::image>> &getImages() = 0;
	virtual observable::value<std::vector<status::slot>> &getSlots() = 0;
	virtual const QString &getWorkingDirectory() const = 0;
	virtual void setWorkingDirectory(QString &&dir) = 0;
	virtual const std::vector<QString> &getRecentFiles(const QString &tag) const = 0;
	virtual std::vector<QString> getExtensions(const QString &tag) const = 0;
	virtual void createImage(const QString &tag, QString &&path) = 0;
	virtual void loadImage(const QString &tag, QString &&path) = 0;
	virtual void unloadImage(const QString &tag) = 0;
	virtual bool startedWithHashPaths() const = 0;
	virtual void changeSlots(std::map<QString, QString> &&changes) = 0;
};


// ======================> ConfigurableDevicesDialog

class ConfigurableDevicesDialog : public QDialog
{
	Q_OBJECT
public:
	ConfigurableDevicesDialog(QWidget &parent, IConfigurableDevicesDialogHost &host, bool cancellable);
	~ConfigurableDevicesDialog();

public slots:
	virtual void accept() override;

private slots:
	void on_applyChangesButton_clicked();

private:
	class ConfigurableDevicesItemDelegate;

	IConfigurableDevicesDialogHost &				m_host;
	std::unique_ptr<Ui::ConfigurableDevicesDialog>	m_ui;
	observable::unique_subscription					m_imagesEventSubscription;
	observable::unique_subscription					m_slotsEventSubscription;
	bool											m_canChangeSlotOptions;

	ConfigurableDevicesModel &model();
	const ConfigurableDevicesModel &model() const;
	void onModelReset();
	static void setupWarningIcons(std::initializer_list<std::reference_wrapper<QLabel>> iconLabels);

	void updateSlots();
	void updateImages();
	void deviceMenu(QPushButton &button, const QModelIndex &index);
	void buildDeviceMenuSlotItems(QMenu &popupMenu, const QString &tag, const info::slot &slot, const QString &currentSlotOption);
	void buildImageMenuSlotItems(QMenu &popupMenu, const QString &tag, const DeviceImage &image);
	bool createImage(const QString &tag);
	bool loadImage(const QString &tag);
	bool loadSoftwareListPart(const software_list_collection &software_col, const QString &tag, const QString &dev_interface);
	void unloadImage(const QString &tag);
	QString getWildcardString(const QString &tag, bool support_zip) const;
	void updateWorkingDirectory(const QString &path);
};


#endif // DIALOGS_CONFDEV_H
