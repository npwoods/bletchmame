/***************************************************************************

	softwarelistitemmodel.h

	QAbstractItemModel implementation for the software list

***************************************************************************/

#ifndef SOFTWARELISTITEMMODEL_H
#define SOFTWARELISTITEMMODEL_H

// bletchmame headers
#include "softwarelist.h"
#include "auditablelistitemmodel.h"

// Qt headers
#include <QAbstractItemModel>


#define SOFTLIST_VIEW_DESC_NAME u8"softlist"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class IconLoader;

// ======================> SoftwareListItemModel

class SoftwareListItemModel : public AuditableListItemModel
{
public:
	enum class Column
	{
		Name,
		Description,
		Year,
		Manufacturer,

		Max = Manufacturer
	};

	SoftwareListItemModel(IconLoader *iconLoader, std::function<void(const software_list::software &)> &&softwareIconAccessedCallback, QObject *parent = nullptr);

	// methods
	void load(const software_list_collection &software_col, bool load_parts, const QString &dev_interface = "");
	void reset();
	void auditStatusChanged(const SoftwareAuditIdentifier &identifier);
	void allAuditStatusesChanged();

	// accessors
	const software_list::software &getSoftwareByIndex(int index) const { return m_parts[index].software(); }

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual AuditIdentifier getAuditIdentifier(int row) const override;
	virtual bool isAuditIdentifierPresent(const AuditIdentifier &identifier) const override;

private:
	// ======================> SoftwareAndPart
	class SoftwareAndPart
	{
	public:
		SoftwareAndPart(const software_list::software &sw, const software_list::part *p);

		const software_list::software &software() const { return m_software; }
		const software_list::part &part() const { assert(m_part); return *m_part; }
		bool has_part() const { return m_part != nullptr; }

	private:
		const software_list::software &	m_software;
		const software_list::part *		m_part;
	};

	IconLoader *											m_iconLoader;
	std::function<void(const software_list::software &)>	m_softwareIconAccessedCallback;
	std::vector<SoftwareAndPart>							m_parts;
	std::vector<QString>									m_softlist_names;
	std::unordered_map<SoftwareAuditIdentifier, int>		m_softwareIndexMap;

	void internalReset();
	void iconsChanged(int startIndex, int endIndex);
};

#endif // SOFTWARELISTITEMMODEL_H
