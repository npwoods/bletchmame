/***************************************************************************

	machinelistitemmodel.h

	QAbstractItemModel implementation for the machine list

***************************************************************************/

#ifndef MACHINELISTITEMMODEL_H
#define MACHINELISTITEMMODEL_H

// bletchmame headers
#include "auditablelistitemmodel.h"
#include "info.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class IconLoader;
class MachineAuditIdentifier;

// ======================> MachineListItemModel

class MachineListItemModel : public AuditableListItemModel
{
public:
	enum class Column
	{
		Machine,
		Description,
		Year,
		Manufacturer,
		SourceFile,

		Max = SourceFile
	};

	MachineListItemModel(QObject *parent, info::database &infoDb, IconLoader *iconLoader, std::function<void(info::machine)> &&machineIconAccessedCallback);

	// methods
	info::machine machineFromIndex(const QModelIndex &index) const;
	void setMachineFilter(std::function<bool(const info::machine &machine)> &&machineFilter);
	void auditStatusChanged(const MachineAuditIdentifier &identifier);
	void allAuditStatusesChanged();

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual AuditIdentifier getAuditIdentifier(int row) const override;
	virtual bool isAuditIdentifierPresent(const AuditIdentifier &identifier) const override;

private:
	typedef std::unordered_map<std::reference_wrapper<const QString>, int> ReverseIndexMap;

	info::database &									m_infoDb;
	IconLoader *										m_iconLoader;
	std::function<bool(const info::machine &machine)>	m_machineFilter;
	std::vector<int>									m_indexes;
	ReverseIndexMap										m_reverseIndexes;
	std::function<void(info::machine)>					m_machineIconAccessedCallback;

	void iconsChanged(int startIndex, int endIndex);
	void populateIndexes();
	info::machine machineFromRow(int row) const;
	bool isMachinePresent(const info::machine &machine) const;
};

#endif // MACHINELISTITEMMODEL_H
