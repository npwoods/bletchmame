/***************************************************************************

	softwarelistitemmodel.h

	QAbstractItemModel implementation for the software list

***************************************************************************/

#ifndef SOFTWARELISTITEMMODEL_H
#define SOFTWARELISTITEMMODEL_H

#include <QAbstractItemModel>

#include "softwarelist.h"


#define SOFTLIST_VIEW_DESC_NAME "softlist"


// ======================> SoftwareListItemModel

class SoftwareListItemModel : public QAbstractItemModel
{
public:
	enum class Column
	{
		Name,
		Description,
		Year,
		Manufacturer,
		Count
	};

	SoftwareListItemModel(QObject *parent);

	// methods
	void load(const software_list_collection &software_col, bool load_parts, const QString &dev_interface = "");
	void reset();

	// accessors
	const software_list::software &getSoftwareByIndex(int index) const { return m_parts[index].software(); }

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
	// ======================> SoftwareAndPart
	class SoftwareAndPart
	{
	public:
		SoftwareAndPart(const software_list &sl, const software_list::software &sw, const software_list::part *p);

		const software_list &softlist() const { return m_softlist; }
		const software_list::software &software() const { return m_software; }
		const software_list::part &part() const { assert(m_part); return *m_part; }
		bool has_part() const { return m_part != nullptr; }

	private:
		const software_list &m_softlist;
		const software_list::software &m_software;
		const software_list::part *m_part;
	};

	std::vector<SoftwareAndPart>	m_parts;
	std::vector<QString>			m_softlist_names;

	void internalReset();
};

#endif // SOFTWARELISTITEMMODEL_H
