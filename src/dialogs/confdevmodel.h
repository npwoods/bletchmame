/***************************************************************************

	dialogs/confdevmodel.h

	Configurable Devices model for QTreeView

***************************************************************************/

#ifndef DIALOGS_CONFDEVMODEL_H
#define DIALOGS_CONFDEVMODEL_H

// bletchmame headers
#include "info.h"
#include "softwarelist.h"
#include "status.h"

// Qt headers
#include <QAbstractItemModel>

// standard headers
#include <map>

#define TEXT_NONE				"<<none>>"


// ======================> DeviceImage

struct DeviceImage
{
	QString				m_instanceName;
	QString				m_fileName;
	bool				m_isReadable;
	bool				m_isWriteable;
	bool				m_isCreatable;
	bool				m_mustBeLoaded;
};


// ======================> ConfigurableDevicesModel

class ConfigurableDevicesModel : public QAbstractItemModel
{
	Q_OBJECT
public:
	class Test;

	ConfigurableDevicesModel(QObject *parent, info::machine machine, software_list_collection &&softwareListCollection);
	~ConfigurableDevicesModel();

	// device info
	class DeviceInfo
	{
	public:
		DeviceInfo(const QString &tag, std::optional<info::slot> slot, const QString &slotOption, const std::optional<DeviceImage> &image);

		const auto &tag() const			{ return m_tag; }
		const auto &slot() const		{ return m_slot; }
		const auto &slotOption() const	{ return m_slotOption; }
		const auto &image() const		{ return m_image; }

	private:
		const QString &						m_tag;
		std::optional<info::slot>			m_slot;
		const QString &						m_slotOption;
		const std::optional<DeviceImage> &	m_image;
	};

	// accessors
	const software_list_collection &softwareListCollection() const { return m_softwareListCollection; }

	// methods
	void setSlots(const std::vector<status::slot> &devslots);
	void setImages(const std::vector<status::image> &images);
	DeviceInfo getDeviceInfo(const QModelIndex &index) const;
	void changeSlotOption(const QString &tag, const QString &slotOption);
	std::map<QString, QString> getChanges() const;

	// statics
	static std::optional<QString> getSlotOptionText(info::slot slot, const QString &slotOption);
	static std::optional<QString> getSlotOptionText(info::slot slot, std::optional<info::slot_option> slotOption);

	// QAbstractItemModel implementation
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	virtual QModelIndex parent(const QModelIndex &index) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
	class Node;
	class RootNode;
	class DeviceNode;

	enum class Column
	{
		Tag,
		Options,

		Max = Options
	};


	info::machine				m_machine;
	software_list_collection	m_softwareListCollection;
	std::unique_ptr<RootNode>	m_root;

	template<typename T> T &nodeFromModelIndex(const QModelIndex &index);
	template<typename T> const T &nodeFromModelIndex(const QModelIndex &index) const;

	// tag calculus
	static bool isSubTag(const QString &tag, const QString &candidateSubtag);
	static bool isTagOfSlotOptionDevice(info::machine machine, const QString &tag);
	static QString relativeTag(const QString &tag, const QString &baseTag);

	// methods for showing presentation
	QString deviceTagDisplayText(const DeviceNode &deviceNode) const;
	void deviceOptionsStatus(const ConfigurableDevicesModel::DeviceNode &deviceNode, std::optional<QString> &text, bool &mandatoryImageMissing) const;
	QString deviceOptionsDisplayText(const DeviceNode &deviceNode) const;
	QFont deviceOptionsDisplayFont(const DeviceNode &deviceNode) const;
	QColor deviceOptionsDisplayForeground(const DeviceNode &deviceNode) const;
};


#endif // DIALOGS_CONFDEVMODEL_H
