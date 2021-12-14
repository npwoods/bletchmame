/***************************************************************************

	dialogs/audititemmodel.cpp

	Auditing Dialog Item Model

***************************************************************************/

#include "dialogs/audititemmodel.h"
#include "iconloader.h"
#include "utility.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditItemModel::AuditItemModel(const Audit &audit, IconLoader &iconLoader, QObject *parent)
	: QAbstractItemModel(parent)
	, m_audit(audit)
	, m_iconLoader(iconLoader)
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
//  iconFromAuditEntryType
//-------------------------------------------------

std::u8string_view AuditItemModel::iconFromAuditEntryType(Audit::Entry::Type entryType)
{
	using namespace std::literals;

	std::u8string_view result;
	switch (entryType)
	{
	case Audit::Entry::Type::Rom:
		result = u8":/resources/rom.ico"sv;
		break;

	case Audit::Entry::Type::Disk:
		result = u8":/resources/harddisk.ico"sv;
		break;

	case Audit::Entry::Type::Sample:
		result = u8":/resources/sound.ico"sv;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  auditStatusFromVerdict
//-------------------------------------------------

AuditStatus AuditItemModel::auditStatusFromVerdict(const std::optional<Audit::Verdict> &verdict, bool optional)
{
	std::optional<AuditStatus> result;
	if (verdict.has_value())
	{
		switch (verdict->type())
		{
		case Audit::Verdict::Type::Ok:
		case Audit::Verdict::Type::OkNoGoodDump:
			result = AuditStatus::Found;
			break;

		case Audit::Verdict::Type::NotFound:
			result = optional ? AuditStatus::MissingOptional : AuditStatus::Missing;
			break;

		case Audit::Verdict::Type::IncorrectSize:
		case Audit::Verdict::Type::Mismatch:
		case Audit::Verdict::Type::CouldntProcessAsset:
			result = AuditStatus::Missing;
			break;
		}
	}
	else
	{
		result = AuditStatus::Unknown;
	}
	return result.value();
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
		bool verdictFailed = verdict.has_value() && !Audit::isVerdictSuccessful(verdict->type());
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

					case Audit::Verdict::Type::CouldntProcessAsset:
						result = "Could Not Process Asset";
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

		case Qt::DecorationRole:
			if (column == Column::Media)
			{
				// determine the icon
				std::u8string_view icon = iconFromAuditEntryType(entry.type());

				// determine the adornment
				AuditStatus auditStatus = auditStatusFromVerdict(verdict, entry.optional());
				std::u8string_view adornment = IconLoader::getAdornmentForAuditStatus(auditStatus);

				// and get the icon
				std::optional<QPixmap> pixmap = m_iconLoader.getIcon(icon, adornment);
				if (pixmap)
					result = std::move(*pixmap);
			}
			break;

		case Qt::FontRole:
			if (column == Column::Status && verdictFailed)
			{
				QFont font;
				font.setBold(true);
				result = std::move(font);
			}
			break;

		case Qt::ForegroundRole:
			if (column == Column::Status && verdictFailed)
			{
				// the audit failed, show a color
				result = entry.optional()
					? QColorConstants::DarkYellow
					: QColorConstants::Red;
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
