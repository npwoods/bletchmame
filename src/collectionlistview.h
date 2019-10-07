/***************************************************************************

	collectionlistview.h

	Subclass of wxListView, providing rich views on collections

***************************************************************************/

#pragma once

#ifndef COLLECTIONLISTVIEW_H
#define COLLECTIONLISTVIEW_H

#include <wx/listctrl.h>
#include <memory>

#include "prefs.h"


// ======================> ColumnDesc

struct ColumnDesc
{
	const char *	m_id;
	const wxChar *	m_description;
	int				m_default_width;
};


// ======================> CollectionViewDesc

struct CollectionViewDesc
{
	const char *			m_name;
	const char *			m_key_column;
	std::vector<ColumnDesc>	m_columns;
};


// ======================> CollectionListView
class CollectionListView : public wxListView
{
public:
	template<typename TFuncGetItemText, typename TFuncGetSize>
	CollectionListView(wxWindow &parent, wxWindowID winid, Preferences &prefs, const CollectionViewDesc &desc, TFuncGetItemText &&func_get_item_text, TFuncGetSize &&func_get_size, bool support_label_edit)
		: CollectionListView(parent, winid, prefs, desc, std::make_unique<CollectionImpl<TFuncGetItemText, TFuncGetSize>>(std::move(func_get_item_text), std::move(func_get_size)), support_label_edit)
	{
	}

	void UpdateListView();
	void UpdateColumnPrefs();
	int GetActualIndex(long indirect_index) const	{ return m_indirections[indirect_index]; }

protected:
	Preferences &Prefs() { return m_prefs; }
	const Preferences &Prefs() const { return m_prefs; }

	virtual wxString OnGetItemText(long item, long column) const override;
	virtual const wxString &GetListViewSelection() const;
	virtual void SetListViewSelection(const wxString &selection);

private:
	// ======================> ICollectionImpl
	class ICollectionImpl
	{
	public:
		virtual ~ICollectionImpl() { }
		virtual const wxString &GetItemText(long item, long column) = 0;
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

		virtual const wxString &GetItemText(long item, long column) override
		{
			return m_func_get_item_text(item, column);
		}

		virtual long GetSize() override
		{
			return (long) m_func_get_size();
		}

	private:
		TFuncGetItemText	m_func_get_item_text;
		TFuncGetSize		m_func_get_size;
	};

	// invariant fields that do not change at runtime
	const CollectionViewDesc &			m_desc;
	Preferences &						m_prefs;
	std::unique_ptr<ICollectionImpl>	m_coll_impl;
	int									m_key_column_index;

	// mutable fields
	std::vector<int>					m_indirections;
	wxString							m_softlist;
	int									m_sort_column;
	ColumnPrefs::sort_type				m_sort_type;

	CollectionListView(wxWindow &parent, wxWindowID winid, Preferences &prefs, const CollectionViewDesc &desc, std::unique_ptr<ICollectionImpl> &&coll_impl, bool support_label_edit);

	const wxString &GetActualItemText(long item, long column) const;
	void ToggleColumnSort(int column_index);
	int CompareActualRows(int row_a, int row_b, int sort_column, ColumnPrefs::sort_type sort_type) const;
	void OnListItemSelected(long index);
};


#endif // COLLECTIONLISTVIEW_H