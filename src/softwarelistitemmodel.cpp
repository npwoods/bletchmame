/***************************************************************************

	softwarelistitemmodel.cpp

	QAbstractItemModel implementation for the software list

***************************************************************************/

// bletchmame headers
#include "softwarelistitemmodel.h"
#include "iconloader.h"
#include "perfprofiler.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

SoftwareListItemModel::SoftwareListItemModel(IconLoader *iconLoader, std::function<void(const software_list::software &)> &&softwareIconAccessedCallback, QObject *parent)
    : AuditableListItemModel(parent)
	, m_iconLoader(iconLoader)
	, m_softwareIconAccessedCallback(std::move(softwareIconAccessedCallback))
{
}


//-------------------------------------------------
//  load
//-------------------------------------------------

void SoftwareListItemModel::load(const software_list_collection &software_col, bool load_parts, const QString &dev_interface)
{
	beginResetModel();

	// clear things out
	internalReset();

	// now enumerate through each list and build the m_parts vector
	for (const software_list::ptr &softlist : software_col.software_lists())
	{
		// if the name of this software list is not in m_softlist_names, add it
		if (std::find(m_softlist_names.begin(), m_softlist_names.end(), softlist->name()) == m_softlist_names.end())
			m_softlist_names.push_back(softlist->name());

		// now enumerate through all software
		for (const software_list::software &software : softlist->get_software())
		{
			if (load_parts)
			{
				// we're loading individual parts; enumerate through them and add them
				for (const software_list::part &part : software.parts())
				{
					if (dev_interface.isEmpty() || dev_interface == part.interface())
						m_parts.emplace_back(software, &part);
				}
			}
			else
			{
				// we're not loading individual parts
				assert(dev_interface.isEmpty());

				// add this to the software index map (needed for auditing)
				SoftwareAuditIdentifier auditIdentifier(software.parent().name(), software.name());
				m_softwareIndexMap.emplace(std::move(auditIdentifier), util::safe_static_cast<int>(m_parts.size()));

				// and add it to the parts list
				m_parts.emplace_back(software, nullptr);
			}
		}
	}

	endResetModel();
}


//-------------------------------------------------
//  reset
//-------------------------------------------------

void SoftwareListItemModel::reset()
{
    beginResetModel();
    internalReset();
    endResetModel();
}


//-------------------------------------------------
//  internalReset
//-------------------------------------------------

void SoftwareListItemModel::internalReset()
{
    m_parts.clear();
    m_softlist_names.clear();
	m_softwareIndexMap.clear();
}


//-------------------------------------------------
//  auditStatusChanged
//-------------------------------------------------

void SoftwareListItemModel::auditStatusChanged(const SoftwareAuditIdentifier &identifier)
{
	ProfilerScope prof(CURRENT_FUNCTION);
	auto iter = m_softwareIndexMap.find(identifier);
	if (iter != m_softwareIndexMap.end())
		iconsChanged(iter->second, iter->second);
}


//-------------------------------------------------
//  allAuditStatusesChanged
//-------------------------------------------------

void SoftwareListItemModel::allAuditStatusesChanged()
{
	ProfilerScope prof(CURRENT_FUNCTION);
	if (!m_parts.empty())
		iconsChanged(0, util::safe_static_cast<int>(m_parts.size()) - 1);
}


//-------------------------------------------------
//  iconsChanged
//-------------------------------------------------

void SoftwareListItemModel::iconsChanged(int startIndex, int endIndex)
{
	// sanity checks
	assert(startIndex >= 0 && startIndex < m_parts.size());
	assert(endIndex >= 0 && endIndex < m_parts.size());
	assert(startIndex <= endIndex);

	// emit a dataChanged event for decorations int he proper range
	QModelIndex topLeft = createIndex(startIndex, (int)Column::Name);
	QModelIndex bottomRight = createIndex(endIndex, (int)Column::Name);
	QVector<int> roles = { Qt::DecorationRole };
	emit dataChanged(topLeft, bottomRight, roles);
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex SoftwareListItemModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex SoftwareListItemModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int SoftwareListItemModel::rowCount(const QModelIndex &parent) const
{
    return util::safe_static_cast<int>(m_parts.size());
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int SoftwareListItemModel::columnCount(const QModelIndex &parent) const
{
	return util::enum_count<Column>();
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant SoftwareListItemModel::data(const QModelIndex &index, int role) const
{
	ProfilerScope prof(CURRENT_FUNCTION);
	QVariant result;
    if (index.isValid()
        && index.row() >= 0
        && index.row() < m_parts.size())
    {
        const software_list::software &sw = m_parts[index.row()].software();
        Column column = (Column)index.column();

		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case Column::Name:
				result = sw.name();
				break;
			case Column::Description:
				result = sw.description();
				break;
			case Column::Year:
				result = sw.year();
				break;
			case Column::Manufacturer:
				result = sw.publisher();
				break;
			case Column::SoftwareListFile:
				result = sw.parent().name() + ".xml";
				break;
			}
			break;

		case Qt::DecorationRole:
			if (column == Column::Name && m_iconLoader)
			{
				std::optional<QPixmap> icon = m_iconLoader->getIcon(sw);
				if (icon.has_value())
					result = std::move(icon.value());

				// invoke the "accessed" callback, which can trigger an audit when autoauditing
				if (m_softwareIconAccessedCallback)
					m_softwareIconAccessedCallback(sw);
			}
			break;
		}
    }
    return result;
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant SoftwareListItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant result;
    if (orientation == Qt::Orientation::Horizontal)
    {
        Column column = (Column)section;
        switch (role)
        {
        case Qt::DisplayRole:
            switch (column)
            {
            case Column::Name:
                result = "Name";
                break;
            case Column::Description:
                result = "Description";
                break;
            case Column::Year:
                result = "Year";
                break;
            case Column::Manufacturer:
                result = "Manufacturer";
                break;
			case Column::SoftwareListFile:
				result = "Software List File";
				break;
			}
            break;
        }
    }
    return result;
}


//-------------------------------------------------
//  getAuditIdentifier
//-------------------------------------------------

AuditIdentifier SoftwareListItemModel::getAuditIdentifier(int row) const
{
	const software_list::software &software = m_parts[row].software();
	return SoftwareAuditIdentifier(software.parent().name(), software.name());
}


//-------------------------------------------------
//  isAuditIdentifierPresent
//-------------------------------------------------

bool SoftwareListItemModel::isAuditIdentifierPresent(const AuditIdentifier &identifier) const
{
	const SoftwareAuditIdentifier *softwareAuditIdentifier = std::get_if<SoftwareAuditIdentifier>(&identifier);
	if (!softwareAuditIdentifier)
		return false;

	auto iter = m_softwareIndexMap.find(*softwareAuditIdentifier);
	return iter != m_softwareIndexMap.end();
}


//-------------------------------------------------
//  SoftwareAndPart ctor
//-------------------------------------------------

SoftwareListItemModel::SoftwareAndPart::SoftwareAndPart(const software_list::software &sw, const software_list::part *p)
    : m_software(sw)
    , m_part(p)
{
}
