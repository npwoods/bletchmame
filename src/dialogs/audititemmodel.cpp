/***************************************************************************

	dialogs/audititemmodel.cpp

	Auditing Dialog Item Model

***************************************************************************/

#include "dialogs/audititemmodel.h"
#include "utility.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditItemModel::AuditItemModel(const Audit &audit, QObject *parent)
	: QAbstractItemModel(parent)
	, m_audit(audit)
{
	m_verdicts.resize(audit.entries().size());
}


//-------------------------------------------------
//  singleMediaAudited
//-------------------------------------------------

void AuditItemModel::singleMediaAudited(int entryIndex, const Audit::Verdict &verdict)
{
	// store the verdict
	m_verdicts[entryIndex].emplace(verdict);

	// indicate we changed
	QModelIndex topLeft = createIndex(entryIndex, 0);
	QModelIndex bottomRight = createIndex(entryIndex, (int)Column::Max);
	dataChanged(topLeft, bottomRight);
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex AuditItemModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex AuditItemModel::parent(const QModelIndex &child) const
{
	return QModelIndex();
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int AuditItemModel::rowCount(const QModelIndex &parent) const
{
	return util::safe_static_cast<int>(m_audit.entries().size());
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int AuditItemModel::columnCount(const QModelIndex &parent) const
{
	return util::enum_count<Column>();
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant AuditItemModel::data(const QModelIndex &index, int role) const
{
	QVariant result;
	if (index.isValid()
		&& index.row() >= 0
		&& index.row() < m_audit.entries().size())
	{
		const Audit::Entry &entry = m_audit.entries()[index.row()];
		const std::optional<Audit::Verdict> &verdict = m_verdicts[index.row()];
		Column column = (Column)index.column();

		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case Column::Media:
				result = entry.name();
				break;

			case Column::Size:
				if (entry.expectedSize())
					result = QString::number(*entry.expectedSize());
				break;

			case Column::Necessity:
				result = entry.optional() ? "optional" : "required";
				break;

			case Column::Status:
				if (verdict.has_value())
				{
					switch (verdict->type())
					{
					case Audit::Verdict::Type::Ok:
						result = "Ok";
						break;

					case Audit::Verdict::Type::OkNoGoodDump:
						result = "No Good Dump Known";
						break;

					case Audit::Verdict::Type::NotFound:
						result = "Not Found";
						break;

					case Audit::Verdict::Type::IncorrectSize:
						result = "Incorrect Size";
						break;

					case Audit::Verdict::Type::Mismatch:
						result = "Mismatch";
						break;
					}
				}
				else
				{
					result = "???";
				}
				break;
			}
			break;
		}
	}
	return result;
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant AuditItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant result;
	if (orientation == Qt::Orientation::Horizontal && section >= 0 && section < util::enum_count<Column>())
	{
		Column column = (Column)section;
		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case Column::Media:
				result = "Media";
				break;

			case Column::Size:
				result = "Size";
				break;

			case Column::Necessity:
				result = "Necessity";
				break;

			case Column::Status:
				result = "Status";
				break;
			}
		}
	}
	return result;
}
