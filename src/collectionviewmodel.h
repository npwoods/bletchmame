/***************************************************************************

    collectionviewmodel.h

    Our implementation of QAbstractItemModel 

***************************************************************************/

#pragma once

#ifndef COLLECTIONVIEWMODEL_H
#define COLLECTIONVIEWMODEL_H

#include <QAbstractItemModel>
#include <memory>

#include "prefs.h"

class QItemSelectionModel;
class QTableView;

// ======================> ColumnDesc

struct ColumnDesc
{
	const char *	m_id;
	const QString	m_description;
	int				m_default_width;
};


// ======================> CollectionViewDesc

struct CollectionViewDesc
{
	const char *			m_name;
	const char *			m_key_column;
	std::vector<ColumnDesc>	m_columns;
};


// ======================> CollectionViewModel
class CollectionViewModel : public QAbstractItemModel
{
public:
	// ctor
	template<typename TFuncGetItemText, typename TFuncGetSize>
	CollectionViewModel(QTableView &tableView, Preferences &prefs, const CollectionViewDesc &desc, TFuncGetItemText &&func_get_item_text, TFuncGetSize &&func_get_size, bool support_label_edit)
        : CollectionViewModel(tableView, prefs, desc, std::make_unique<CollectionImpl<TFuncGetItemText, TFuncGetSize>>(std::move(func_get_item_text), std::move(func_get_size)), support_label_edit)
    {
    }

	// methods
	void updateListView();
	long getFirstSelected() const;
	int getActualIndex(long indirect_index) const	{ return m_indirections[indirect_index]; }

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

protected:
	Preferences &Prefs() { return m_prefs; }
	const Preferences &Prefs() const { return m_prefs; }

	virtual const QString &getListViewSelection() const;
	virtual void setListViewSelection(const QString &selection);

private:
	// ======================> ICollectionImpl
	class ICollectionImpl
	{
	public:
		virtual ~ICollectionImpl() { }
		virtual const QString &GetItemText(long item, long column) = 0;
		virtual long GetSize() = 0;
	};

	// ======================> CollectionImpl
	template<typename TFuncGetItemText, typename TFuncGetSize>
	class CollectionImpl : public ICollectionImpl
	{
	public:
		CollectionImpl(TFuncGetItemText &&func_get_item_text, TFuncGetSize &&func_get_size)
			: m_func_get_item_text(std::move(func_get_item_text))
			, m_func_get_size(std::move(func_get_size))
		{
		}

		virtual const QString &GetItemText(long item, long column) override
		{
			return m_func_get_item_text(item, column);
		}

		virtual long GetSize() override
		{
			return (long)m_func_get_size();
		}

	private:
		TFuncGetItemText	m_func_get_item_text;
		TFuncGetSize		m_func_get_size;
	};

	// invariant fields that do not change at runtime
	const CollectionViewDesc &			m_desc;
	Preferences &						m_prefs;
	std::unique_ptr<ICollectionImpl>	m_coll_impl;
	std::vector<int>					m_indirections;
	int									m_key_column_index;

	// ctor
	CollectionViewModel(QTableView &tableView, Preferences &prefs, const CollectionViewDesc &desc, std::unique_ptr<ICollectionImpl> &&coll_impl, bool support_label_edit);

	// methods
	int rowCount() const;
	int columnCount() const;
	QTableView &parentAsTableView();
	const QTableView &parentAsTableView() const;
	void selectByIndex(long item_index);
	const QString &getActualItemText(long item, long column) const;
};

#endif // COLLECTIONVIEWMODEL_H
