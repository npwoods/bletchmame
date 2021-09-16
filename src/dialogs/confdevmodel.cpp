/***************************************************************************

	dialogs/confdevmodel.cpp

	Configurable Devices model for QTreeView

***************************************************************************/

#include <QDebug>
#include <QDir>

#include "confdevmodel.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_POPULATE_DEVICES	0
#define LOG_UNEXPECTED			0


//**************************************************************************
//  TYPES
//**************************************************************************

// ======================> ConfigurableDevicesModel::Node

class ConfigurableDevicesModel::Node
{
public:
	typedef std::unique_ptr<Node> ptr;

	// ctor
	Node() = default;
	Node(const Node &) = delete;
	Node(Node &&) = delete;

	// dtor
	virtual ~Node()
	{
	}

	// methods
	DeviceNode &addChild(std::unique_ptr<DeviceNode> &&child);
	const std::vector<std::unique_ptr<DeviceNode>> &children() const;
	DeviceNode *findNode(const QString &tag, bool create);
	void populateChildren(std::optional<info::machine> machine);

	// virtuals
	virtual const Node *parent() const = 0;
	virtual QString childTagBase() const = 0;
	virtual void getChanges(std::map<QString, QString> &results) const;

private:
	std::vector<std::unique_ptr<DeviceNode>>	m_children;
};


// ======================> ConfigurableDevicesModel::RootNode

class ConfigurableDevicesModel::RootNode : public ConfigurableDevicesModel::Node
{
public:
	virtual const Node *parent() const override
	{
		return nullptr;
	}

	virtual QString childTagBase() const override
	{
		return QString();
	}
};


// ======================> ConfigurableDevicesModel::DeviceNode

class ConfigurableDevicesModel::DeviceNode : public ConfigurableDevicesModel::Node
{
public:
	typedef std::unique_ptr<DeviceNode> ptr;

	DeviceNode(Node &parent, QString &&tag, std::optional<info::slot> slot)
		: m_parent(parent)
		, m_slot(slot)
		, m_tag(std::move(tag))
	{
	}

	virtual const Node *parent() const
	{
		return &m_parent;
	}

	virtual QString childTagBase() const override
	{
		return m_tag + ":" + m_currentOption;
	}

	const QString &tag() const
	{
		return m_tag;
	}

	const std::optional<info::slot> &slot() const
	{
		return m_slot;
	}

	const QString &currentOption() const
	{
		return m_currentOption;
	}

	const std::optional<DeviceImage> &image() const
	{
		return m_image;
	}

	void setCurrentOption(const QString &currentOption, bool inEmulation)
	{
		// if this is the current state in the emulation, take note
		if (inEmulation)
			m_currentOptionInEmulation = currentOption;

		// only do something if the value is being changed
		if (currentOption != m_currentOption)
		{
			// update our currentOption
			m_currentOption = currentOption;

			// try to find the machine
			std::optional<info::machine> deviceMachine = getMachineForSlotOption(currentOption);

			// and finally [re]populate our children
			populateChildren(deviceMachine);
		}
	}

	virtual void getChanges(std::map<QString, QString> &results) const override final
	{
		if (m_currentOption != m_currentOptionInEmulation)
			results[m_tag] = m_currentOption;
		Node::getChanges(results);
	}

	void setImage(const status::image &image)
	{
		m_image.emplace();
		m_image->m_instanceName	= image.m_instance_name;
		m_image->m_fileName		= image.m_file_name;
		m_image->m_isReadable	= image.m_is_readable;
		m_image->m_isWriteable	= image.m_is_writeable;
		m_image->m_isCreatable	= image.m_is_creatable;
		m_image->m_mustBeLoaded	= image.m_must_be_loaded;
	}

private:
	Node &							m_parent;
	std::optional<info::slot>		m_slot;
	std::optional<DeviceImage>		m_image;
	QString							m_tag;
	QString							m_currentOption;
	QString							m_currentOptionInEmulation;

	std::optional<info::machine> getMachineForSlotOption(const QString &slotOption)
	{
		if (!slotOption.isEmpty() && m_slot.has_value())
		{
			auto iter = std::find_if(
				m_slot.value().options().begin(),
				m_slot.value().options().end(),
				[&slotOption](const info::slot_option &x)
				{
					return x.name() == slotOption;
				});
			if (iter != m_slot.value().options().end())
				return iter->machine();
		}
		return std::nullopt;
	}
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ConfigurableDevicesModel::ConfigurableDevicesModel(QObject *parent, info::machine machine, software_list_collection &&softwareListCollection)
	: QAbstractItemModel(parent)
	, m_machine(machine)
	, m_softwareListCollection(std::move(softwareListCollection))
{
	// create the root, and populate it
	m_root = std::make_unique<RootNode>();
	m_root->populateChildren(m_machine);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ConfigurableDevicesModel::~ConfigurableDevicesModel()
{
}


//-------------------------------------------------
//  setSlots
//-------------------------------------------------

void ConfigurableDevicesModel::setSlots(const std::vector<status::slot> &devslots)
{
	beginResetModel();
	for (const status::slot &slot : devslots)
	{
		DeviceNode *node = m_root->findNode(slot.m_name, false);
		if (!node)
		{
			// this should not really happen unless there is an infodb mismatch
			if (LOG_UNEXPECTED)
				qInfo("ConfigurableDevicesModel::setSlots(): Unable to locate slot '%s'", qUtf8Printable(slot.m_name));
			continue;
		}

		node->setCurrentOption(slot.m_current_option, true);
	}
	endResetModel();
}


//-------------------------------------------------
//  setImages
//-------------------------------------------------

void ConfigurableDevicesModel::setImages(const std::vector<status::image> &images)
{
	for (const status::image &image : images)
	{
		DeviceNode &node = *m_root->findNode(image.m_tag, true);
		node.setImage(image);
	}
}


//-------------------------------------------------
//  getDeviceInfo
//-------------------------------------------------

ConfigurableDevicesModel::DeviceInfo ConfigurableDevicesModel::getDeviceInfo(const QModelIndex &index) const
{
	const DeviceNode &node = nodeFromModelIndex<DeviceNode>(index);
	return DeviceInfo(node.tag(), node.slot(), node.currentOption(), node.image());
}


//-------------------------------------------------
//  changeSlotOption
//-------------------------------------------------

void ConfigurableDevicesModel::changeSlotOption(const QString &tag, const QString &slotOption)
{
	beginResetModel();

	DeviceNode *node = m_root->findNode(tag, false);
	if (node)
		node->setCurrentOption(slotOption, false);
	else if (LOG_UNEXPECTED)
		qInfo("ConfigurableDevicesModel::changeSlotOption(): Unable to locate slot '%s'", qUtf8Printable(tag));

	endResetModel();
}


//-------------------------------------------------
//  nodeFromModelIndex
//-------------------------------------------------

std::map<QString, QString> ConfigurableDevicesModel::getChanges() const
{
	std::map<QString, QString> result;
	m_root->getChanges(result);
	return result;
}


//-------------------------------------------------
//  prettifyImageFileName
//-------------------------------------------------

QString ConfigurableDevicesModel::prettifyImageFileName(const QString &deviceTag, const QString &fileName, bool fullPath) const
{
	QString result;

	// try to find the software
	std::optional<info::device> device = m_machine.find_device(deviceTag);
	const software_list::software *software = device.has_value()
		? m_softwareListCollection.find_software_by_name(fileName, device->devinterface())
		: nullptr;

	// did we find the software?
	if (software)
	{
		// if so, the pretty name is the description
		result = software->m_description;
	}
	else if (!fullPath && !fileName.isEmpty())
	{
		// we want to show the base name plus the extension
		QString normalizedFileName = QDir::fromNativeSeparators(fileName);
		QFileInfo fileInfo(normalizedFileName);
		result = fileInfo.baseName();
		if (!fileInfo.suffix().isEmpty())
		{
			result += ".";
			result += fileInfo.suffix();
		}
	}
	else
	{
		// we want to show the full filename; our job is easy
		result = fileName;
	}
	return result;
}


//-------------------------------------------------
//  nodeFromModelIndex
//-------------------------------------------------

template<typename T>
T &ConfigurableDevicesModel::nodeFromModelIndex(const QModelIndex &index)
{
	Node *node = index.isValid()
		? static_cast<Node *>(index.internalPointer())
		: &*m_root;
	return *dynamic_cast<T *>(node);
}

template<typename T>
const T &ConfigurableDevicesModel::nodeFromModelIndex(const QModelIndex &index) const
{
	const Node *node = index.isValid()
		? static_cast<Node *>(index.internalPointer())
		: &*m_root;
	return *dynamic_cast<const T *>(node);
}


//-------------------------------------------------
//  isSubTag
//-------------------------------------------------

bool ConfigurableDevicesModel::isSubTag(const QString &tag, const QString &candidateSubtag)
{
	return candidateSubtag.startsWith(tag + ":");
}


//-------------------------------------------------
//  isTagOfSlotOptionDevice
//-------------------------------------------------

bool ConfigurableDevicesModel::isTagOfSlotOptionDevice(info::machine machine, const QString &tag)
{
	for (info::slot slot : machine.devslots())
	{
		for (info::slot_option opt : slot.options())
		{
			QString optionTag = slot.name() + ":" + opt.name();
			if (isSubTag(optionTag, tag))
				return true;
		}
	}
	return false;
}


//-------------------------------------------------
//  relativeTag
//-------------------------------------------------

QString ConfigurableDevicesModel::relativeTag(const QString &tag, const QString &baseTag)
{
	QString result;
	if (tag == baseTag)
	{
		// same tag; pluck the end
		int colonPos = tag.lastIndexOf(':');
		result = colonPos >= 0
			? tag.right(tag.size() - colonPos - 1)
			: tag;
	}
	else if (tag.startsWith(baseTag + ":"))
	{
		// tag is a child of base
		result = tag.right(tag.size() - baseTag.size() - 1);
	}
	else
	{
		// tag is not a child of base
		result = tag;
	}
	return result;
}


//-------------------------------------------------
//  getSlotOptionText
//-------------------------------------------------

std::optional<QString> ConfigurableDevicesModel::getSlotOptionText(info::slot slot, const QString &slotOption)
{
	std::optional<QString> result;
	if (!slotOption.isEmpty())
	{
		auto iter = std::find_if(
			slot.options().begin(),
			slot.options().end(),
			[&slotOption](const info::slot_option &x)
			{
				return x.name() == slotOption;
			});

		if (iter != slot.options().end())
		{
			// found the info::slot_option; call the other function
			result = getSlotOptionText(slot, *iter);
		}
		else
		{
			// very special case - this is an option that is not present in the infodb; return it verbatim
			result = QString("%1 (\?\?\?)").arg(slotOption);
		}
	}
	else
	{
		// empty option - call the other function
		result = getSlotOptionText(slot, std::nullopt);
	}
	return result;
}


//-------------------------------------------------
//  getSlotOptionText
//-------------------------------------------------

std::optional<QString> ConfigurableDevicesModel::getSlotOptionText(info::slot slot, std::optional<info::slot_option> slotOption)
{
	std::optional<QString> result;
	if (slotOption.has_value())
	{
		std::optional<info::machine> slotOptionMachine = slotOption->machine();
		result = slotOptionMachine.has_value()
			? QString("%1 (%2)").arg(slotOptionMachine->description(), slotOption->name())
			: slotOption->name();
	}
	else
	{
		result = std::nullopt;
	}
	return result;
}


//-------------------------------------------------
//  deviceTagDisplayText
//-------------------------------------------------

QString ConfigurableDevicesModel::deviceTagDisplayText(const ConfigurableDevicesModel::DeviceNode &deviceNode) const
{
	return relativeTag(deviceNode.tag(), deviceNode.parent()->childTagBase());
}


//-------------------------------------------------
//  deviceOptionsStatus
//-------------------------------------------------

void ConfigurableDevicesModel::deviceOptionsStatus(const ConfigurableDevicesModel::DeviceNode &deviceNode, std::optional<QString> &text, bool &mandatoryImageMissing) const
{
	// clear out outputs
	text = std::nullopt;
	mandatoryImageMissing = false;

	// do we have an image?
	if (deviceNode.image().has_value())
	{
		// this is an image
		const DeviceImage &image = deviceNode.image().value();
		QString prettyFileName = prettifyImageFileName(deviceNode.tag(), image.m_fileName, false);
		if (!prettyFileName.isEmpty())
			text = std::move(prettyFileName);
		else if (image.m_mustBeLoaded)
			mandatoryImageMissing = true;
	}

	// if we don't have image text, try for a slot
	if (!text.has_value() && deviceNode.slot().has_value() && !mandatoryImageMissing)
	{
		// yes this is a slot
		text = getSlotOptionText(deviceNode.slot().value(), deviceNode.currentOption());
	}
}


//-------------------------------------------------
//  deviceOptionsDisplayText
//-------------------------------------------------

QString ConfigurableDevicesModel::deviceOptionsDisplayText(const ConfigurableDevicesModel::DeviceNode &deviceNode) const
{
	// do the heavy lifting
	std::optional<QString> text;
	bool mandatoryImageMissing;
	deviceOptionsStatus(deviceNode, text, mandatoryImageMissing);

	// and return
	return text.has_value()
		? std::move(text.value())
		: TEXT_NONE;
}


//-------------------------------------------------
//  deviceOptionsDisplayFont
//-------------------------------------------------
 
QFont ConfigurableDevicesModel::deviceOptionsDisplayFont(const ConfigurableDevicesModel::DeviceNode &deviceNode) const
{
	// do the heavy lifting
	std::optional<QString> text;
	bool mandatoryImageMissing;
	deviceOptionsStatus(deviceNode, text, mandatoryImageMissing);

	// and format it
	QFont font;
	font.setItalic(!text.has_value());
	font.setBold(mandatoryImageMissing);
	return font;
}


//-------------------------------------------------
//  deviceOptionsDisplayForeground
//-------------------------------------------------

QColor ConfigurableDevicesModel::deviceOptionsDisplayForeground(const DeviceNode &deviceNode) const
{
	// do the heavy lifting
	std::optional<QString> text;
	bool mandatoryImageMissing;
	deviceOptionsStatus(deviceNode, text, mandatoryImageMissing);

	// and return
	return mandatoryImageMissing
		? QColorConstants::Red
		: QColorConstants::Black;
}


//**************************************************************************
//  QABSTRACTITEMMODEL IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant ConfigurableDevicesModel::data(const QModelIndex &index, int role) const
{
	QVariant result;
	if (index.isValid())
	{
		Column column = static_cast<Column>(index.column());
		const DeviceNode &deviceNode = nodeFromModelIndex<DeviceNode>(index);

		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case Column::Tag:
				result = deviceTagDisplayText(deviceNode);
				break;

			case Column::Options:
				result = deviceOptionsDisplayText(deviceNode);
				break;
			}
			break;

		case Qt::FontRole:
			switch (column)
			{
			case Column::Options:
				result = deviceOptionsDisplayFont(deviceNode);
				break;

			case Column::Tag:
				// do nothing
				break;
			}
			break;

		case Qt::ForegroundRole:
			switch (column)
			{
			case Column::Options:
				result = deviceOptionsDisplayForeground(deviceNode);
				break;

			case Column::Tag:
				// do nothing
				break;
			}
			break;
		}
	}
	return result;
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex ConfigurableDevicesModel::index(int row, int column, const QModelIndex &parent) const
{
	const Node &node = nodeFromModelIndex<Node>(parent);
	const Node &childNode = *node.children()[row];
	return createIndex(row, column, (void *) &childNode);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex ConfigurableDevicesModel::parent(const QModelIndex &index) const
{
	QModelIndex result;
	const Node &node = nodeFromModelIndex<Node>(index);
	const Node *parentNode = node.parent();
	if (parentNode)
	{
		auto iter = std::find_if(
			parentNode->children().begin(),
			parentNode->children().end(),
			[&node](const auto &x) { return &*x == &node; });
		result = createIndex(iter - parentNode->children().begin(), 0, (void *)parentNode);
	}
	return result;
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int ConfigurableDevicesModel::rowCount(const QModelIndex &parent) const
{
	return util::safe_static_cast<int>(nodeFromModelIndex<Node>(parent).children().size());
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int ConfigurableDevicesModel::columnCount(const QModelIndex &parent) const
{
	return util::enum_count<Column>();
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant ConfigurableDevicesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant result;
	if (orientation == Qt::Orientation::Horizontal)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			switch ((Column)section)
			{
			case Column::Tag:
				result = "Tag";
				break;
				
			case Column::Options:
				result = "Options";
				break;
			}
			break;
		}
	}
	return result;
}


//-------------------------------------------------
//  flags
//-------------------------------------------------

Qt::ItemFlags ConfigurableDevicesModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags result = QAbstractItemModel::flags(index);
	if (index.isValid() && index.column() == (int)Column::Options)
		result |= Qt::ItemIsEditable;
	return result;
}


//**************************************************************************
//  NODES IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  Node::addChild
//-------------------------------------------------

ConfigurableDevicesModel::DeviceNode &ConfigurableDevicesModel::Node::addChild(ConfigurableDevicesModel::DeviceNode::ptr &&child)
{
	// prepare the result
	DeviceNode &result = *child;

	// insert the child
	auto iter = std::lower_bound(
		m_children.begin(),
		m_children.end(),
		child->tag(),
		[](const DeviceNode::ptr &a, const QString &b) { return a->tag() < b; });
	m_children.insert(iter, std::move(child));

	// and we're done
	return result;
}


//-------------------------------------------------
//  Node::children
//-------------------------------------------------

const std::vector<std::unique_ptr<ConfigurableDevicesModel::DeviceNode>> &ConfigurableDevicesModel::Node::children() const
{
	return m_children;
}


//-------------------------------------------------
//  Node::findNode
//-------------------------------------------------

ConfigurableDevicesModel::DeviceNode *ConfigurableDevicesModel::Node::findNode(const QString &tag, bool create)
{
	DeviceNode *result = nullptr;

	for (DeviceNode::ptr &node : m_children)
	{
		// did we find the target tag?
		const QString &nodeTag = node->tag();
		if (nodeTag == tag)
		{
			// we found the tag we're looking for
			result = &*node;
			break;
		}
		else if (isSubTag(nodeTag, tag))
		{
			// we found an ancestor; find inside it
			result = node->findNode(tag, create);
			break;
		}
	}

	// do we need to create the node?
	if (!result && create)
	{
		DeviceNode::ptr node = std::make_unique<DeviceNode>(*this, QString(tag), std::nullopt);
		result = &addChild(std::move(node));
	}

	return result;
}


//-------------------------------------------------
//  Node::populateChildren
//-------------------------------------------------

void ConfigurableDevicesModel::Node::populateChildren(std::optional<info::machine> machine)
{
	m_children.clear();
	if (machine)
	{
		for (info::slot slot : machine->devslots())
		{
			if (!isTagOfSlotOptionDevice(*machine, slot.name()))
			{
				if (LOG_POPULATE_DEVICES)
					qInfo("ConfigurableDevicesModel::Node::populateChildren(): Adding slot '%s'", qUtf8Printable(slot.name()));

				// create the child node
				DeviceNode::ptr child = std::make_unique<DeviceNode>(*this, QString(childTagBase() + slot.name()), slot);

				// set its option to the default
				auto iter = std::find_if(
					slot.options().begin(),
					slot.options().end(),
					[](const info::slot_option &opt) { return opt.is_default(); });
				if (iter != slot.options().end())
				{
					child->setCurrentOption(iter->name(), true);
				}

				// and add it
				addChild(std::move(child));
			}
		}
	}
}


//-------------------------------------------------
//  Node::getChanges
//-------------------------------------------------

void ConfigurableDevicesModel::Node::getChanges(std::map<QString, QString> &results) const
{
	for (const DeviceNode::ptr &ptr : children())
		ptr->getChanges(results);
}


//**************************************************************************
//  OTHER
//**************************************************************************

//-------------------------------------------------
//  DeviceInfo ctor
//-------------------------------------------------

ConfigurableDevicesModel::DeviceInfo::DeviceInfo(const QString &tag, std::optional<info::slot> slot, const QString &slotOption, const std::optional<DeviceImage> &image)
	: m_tag(tag)
	, m_slot(slot)
	, m_slotOption(slotOption)
	, m_image(image)
{

}