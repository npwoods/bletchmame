/***************************************************************************

	dialogs/customizefields.h

	Customize fields on list views

***************************************************************************/

#pragma once

#ifndef DIALOGS_CUSTOMIZEFIELDS_H
#define DIALOGS_CUSTOMIZEFIELDS_H

// Qt headers
#include <QDialog>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

QT_BEGIN_NAMESPACE
namespace Ui { class CustomizeFieldsDialog; }
class QItemSelection;
class QItemSelectionModel;
class QListView;
QT_END_NAMESPACE

// ======================> CustomizeFieldsDialog

class CustomizeFieldsDialog : public QDialog
{
	Q_OBJECT

public:
	struct Field
	{
		QString				m_name;
		std::optional<int>	m_order;
	};

	// ctor/dtor
	CustomizeFieldsDialog(QWidget *parent);
	~CustomizeFieldsDialog();

	// methods
	void addField(QString &&name, std::optional<int> order);
	void updateStringViews();

	// accessors
	const std::vector<Field> &fields() const { return m_fields; }

private slots:
	void on_addFieldButton_pressed();
	void on_removeFieldButton_pressed();
	void on_moveUpButton_pressed();
	void on_moveDownButton_pressed();

private:
	std::unique_ptr<Ui::CustomizeFieldsDialog>	m_ui;
	std::vector<Field>							m_fields;
	std::vector<int>							m_availableFields;
	std::vector<int>							m_shownFields;

	void updateListModel(QListView &listView, const std::vector<int> &fields);
	void availableFieldsSelectionChanged();
	void shownFieldsSelectionChanged();
	void moveSelectedShownFields(int adjustment);
};


#endif // DIALOGS_CUSTOMIZEFIELDS_H
