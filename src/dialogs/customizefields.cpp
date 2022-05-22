/***************************************************************************

	dialogs/customizefields.cpp

	Customize fields on list views

***************************************************************************/

// bletchmame headers
#include "dialogs/customizefields.h"
#include "ui_customizefields.h"

// Qt headers
#include <QStringListModel>

// standard headers
#include <set>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

CustomizeFieldsDialog::CustomizeFieldsDialog(QWidget *parent)
	: QDialog(parent)
{
	// set up user interface
	m_ui = std::make_unique<Ui::CustomizeFieldsDialog>();
	m_ui->setupUi(this);

	// set models
	m_ui->availableFieldsListView->setModel(new QStringListModel(this));
	m_ui->shownFieldsListView->setModel(new QStringListModel(this));

	// other
	m_fields.reserve(24);

	// selection
	connect(m_ui->availableFieldsListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &, const QItemSelection &)
	{
		availableFieldsSelectionChanged();
	});
	connect(m_ui->shownFieldsListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &, const QItemSelection &)
	{
		shownFieldsSelectionChanged();
	});
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

CustomizeFieldsDialog::~CustomizeFieldsDialog()
{
}


//-------------------------------------------------
//  addField
//-------------------------------------------------

void CustomizeFieldsDialog::addField(QString &&name, std::optional<int> order)
{
	Field &field = m_fields.emplace_back();
	field.m_name = std::move(name);
	field.m_order = order;
}


//-------------------------------------------------
//  updateStringViews
//-------------------------------------------------

void CustomizeFieldsDialog::updateStringViews()
{
	// clear out the field vectors
	m_availableFields.clear();
	m_shownFields.clear();

	// and repopulate them
	for (int i = 0; i < m_fields.size(); i++)
	{
		const Field &field = m_fields[i];

		if (field.m_order.has_value())
		{
			// this field is shown - insert it into the list in the correct place
			auto iter = std::ranges::lower_bound(
				m_shownFields,
				i,
				[this](int x, int y)
				{
					return m_fields[x].m_order.value() < m_fields[y].m_order.value();
				});
			m_shownFields.insert(iter, i);
		}
		else
		{
			// put this in the list of the unavailable fields
			m_availableFields.push_back(i);
		}
	}

	// go through and normalize the order of shown fields
	for (int i = 0; i < m_shownFields.size(); i++)
		m_fields[m_shownFields[i]].m_order = i;

	// and update the list models
	updateListModel(*m_ui->availableFieldsListView, m_availableFields);
	updateListModel(*m_ui->shownFieldsListView, m_shownFields);
}


//-------------------------------------------------
//  updateListModel
//-------------------------------------------------

void CustomizeFieldsDialog::updateListModel(QListView &listView, const std::vector<int> &fields)
{
	// determine the current string
	QStringListModel &listViewModel = *dynamic_cast<QStringListModel *>(listView.model());
	QModelIndex currentIndex = listView.currentIndex();
	QString currentString = currentIndex.isValid()
		? listViewModel.data(currentIndex, Qt::DisplayRole).toString()
		: QString();

	// determine the list of selected strings
	QItemSelectionModel &selectionModel = *listView.selectionModel();
	std::set<QString> selectedStrings;
	QModelIndexList selectedIndexes = selectionModel.selectedIndexes();
	for (const QModelIndex &index : selectedIndexes)
	{
		QString selectedString = listViewModel.data(index, Qt::DisplayRole).toString();
		selectedStrings.insert(std::move(selectedString));
	}

	// prepare to note the new indexes for the selection
	std::vector<int> newSelectedRows;
	newSelectedRows.reserve(selectedIndexes.size());

	// figure out the new string list
	std::optional<int> newCurrentRow;
	QStringList stringList;
	for (int row = 0; row < fields.size(); row++)
	{
		const Field &field = m_fields[fields[row]];
		stringList.push_back(field.m_name);

		if (currentString == field.m_name)
			newCurrentRow = row;

		if (selectedStrings.contains(field.m_name))
			newSelectedRows.push_back(row);
	}

	// set the string list
	listViewModel.setStringList(stringList);

	// fix the current index
	if (newCurrentRow.has_value())
	{
		QModelIndex index = listViewModel.index(newCurrentRow.value(), 0);
		listView.setCurrentIndex(index);
	}

	// fix the selection
	selectionModel.clearSelection();
	for (int row : newSelectedRows)
	{
		QModelIndex index = listViewModel.index(row, 0);
		selectionModel.select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
	}
}


//-------------------------------------------------
//  availableFieldsSelectionChanged
//-------------------------------------------------

void CustomizeFieldsDialog::availableFieldsSelectionChanged()
{
	int selectedCount = m_ui->availableFieldsListView->selectionModel()->selectedIndexes().size();
	m_ui->addFieldButton->setEnabled(selectedCount > 0);
}


//-------------------------------------------------
//  shownFieldsSelectionChanged
//-------------------------------------------------

void CustomizeFieldsDialog::shownFieldsSelectionChanged()
{
	QModelIndexList selectedIndexes = m_ui->shownFieldsListView->selectionModel()->selectedIndexes();
	QModelIndex singleSelectedIndex = selectedIndexes.size() == 1
		? selectedIndexes[0]
		: QModelIndex();

	m_ui->removeFieldButton->setEnabled(!selectedIndexes.isEmpty());
	m_ui->moveUpButton->setEnabled(singleSelectedIndex.isValid() && singleSelectedIndex.row() > 0);
	m_ui->moveDownButton->setEnabled(singleSelectedIndex.isValid() && singleSelectedIndex.row() < m_shownFields.size() - 1);
}


//-------------------------------------------------
//  moveSelectedShownFields
//-------------------------------------------------

void CustomizeFieldsDialog::moveSelectedShownFields(int adjustment)
{
	QItemSelectionModel &selectionModel = *m_ui->shownFieldsListView->selectionModel();

	// even though in practice we only allow a single selection, we'll loop here
	QModelIndexList selectedIndexes = selectionModel.selectedIndexes();
	for (const QModelIndex &index : selectedIndexes)
	{
		// determine the row that we're moving, and the new row
		int oldRow = index.row();
		int newRow = oldRow + adjustment;

		// even though the UX blocks the user from moving past the end, we'll check this
		if (newRow >= 0 && newRow < m_shownFields.size())
		{
			// swap the field orders
			std::swap(
				m_fields[m_shownFields[oldRow]].m_order,
				m_fields[m_shownFields[newRow]].m_order);
		}
	}

	updateStringViews();
}


//-------------------------------------------------
//  on_addFieldButton_pressed
//-------------------------------------------------

void CustomizeFieldsDialog::on_addFieldButton_pressed()
{
	int shownFieldCount = (int)m_shownFields.size();

	QModelIndexList selectedIndexes = m_ui->availableFieldsListView->selectionModel()->selectedIndexes();
	for (const QModelIndex idx : selectedIndexes)
		m_fields[m_availableFields[idx.row()]].m_order = shownFieldCount++;

	updateStringViews();
}


//-------------------------------------------------
//  on_removeFieldButton_pressed
//-------------------------------------------------

void CustomizeFieldsDialog::on_removeFieldButton_pressed()
{
	QModelIndexList selectedIndexes = m_ui->shownFieldsListView->selectionModel()->selectedIndexes();
	for (const QModelIndex idx : selectedIndexes)
		m_fields[m_shownFields[idx.row()]].m_order = std::nullopt;

	updateStringViews();
}


//-------------------------------------------------
//  on_moveUpButton_pressed
//-------------------------------------------------

void CustomizeFieldsDialog::on_moveUpButton_pressed()
{
	moveSelectedShownFields(-1);
}


//-------------------------------------------------
//  on_moveDownButton_pressed
//-------------------------------------------------

void CustomizeFieldsDialog::on_moveDownButton_pressed()
{
	moveSelectedShownFields(+1);
}
