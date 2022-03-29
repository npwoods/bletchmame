/***************************************************************************

	dialogs/importmameini.cpp

	Dialog for importing MAME.ini files

***************************************************************************/

// bletchmame headers
#include "importmameini.h"
#include "importmameinijob.h"
#include "ui_importmameini.h"
#include "prefs.h"
#include "iniparser.h"

// Qt headers
#include <QComboBox>
#include <QPushButton>
#include <QStyledItemDelegate>


//**************************************************************************
//  TYPES
//**************************************************************************

// ======================> TableModel

class ImportMameIniDialog::TableModel : public QAbstractListModel
{
public:
	enum class Column
	{
		Label,
		Value,
		Action,

		Max = Action
	};

	// ctor
	TableModel(Preferences &prefs, QObject *parent = nullptr);

	// accessor
	const ImportMameIniJob &job() const { return m_job; }
	ImportMameIniJob &job() { return m_job; }

	// methods
	bool loadMameIni(const QString &fileName);
	const ImportMameIniJob::Entry *entry(const QModelIndex &index) const;
	ImportMameIniJob::Entry *entry(const QModelIndex &index);

	// virtuals
	virtual int rowCount(const QModelIndex &parent) const override final;
	virtual int columnCount(const QModelIndex &parent) const override final;
	virtual QVariant data(const QModelIndex &index, int role) const override final;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override final;
	virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override final;

private:
	// variables
	ImportMameIniJob	m_job;
};


// ======================> ActionColumnItemDelegate

class ImportMameIniDialog::ActionColumnItemDelegate : public QStyledItemDelegate
{
public:
	ActionColumnItemDelegate(TableModel &model, QObject *parent = nullptr);

	virtual QString displayText(const QVariant &value, const QLocale &locale) const override;
	virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
	virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
	virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
	TableModel &m_model;
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ImportMameIniDialog::ImportMameIniDialog(Preferences &prefs, QWidget *parent)
	: QDialog(parent)
{
	// create the UI
	m_ui = std::make_unique<Ui::ImportMameIniDialog>();
	m_ui->setupUi(this);

	// create the model
	TableModel &model = *new TableModel(prefs, this);
	m_ui->tableView->setModel(&model);

	// set up the headers
	m_ui->tableView->horizontalHeader()->hide();
	m_ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_ui->tableView->horizontalHeader()->setStretchLastSection(true);
	m_ui->tableView->verticalHeader()->hide();

	// we may need to update buttons
	connect(&model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &, const QModelIndex &)
	{
		updateButtonsEnabled();
	});
	connect(&model, &QAbstractItemModel::modelReset, this, [this]()
	{
		updateButtonsEnabled();
	});
	connect(m_ui->tableView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &previous)
	{
		tableViewCurrentChanged(current);
	});

	// add the item delegate to provide combo boxes
	QStyledItemDelegate &itemDelegate = *new ActionColumnItemDelegate(model, this);
	m_ui->tableView->setItemDelegateForColumn((int) TableModel::Column::Action, &itemDelegate);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ImportMameIniDialog::~ImportMameIniDialog()
{
}


//-------------------------------------------------
//  loadMameIni
//-------------------------------------------------

bool ImportMameIniDialog::loadMameIni(const QString &fileName)
{
	return tableModel().loadMameIni(fileName);
}


//-------------------------------------------------
//  tableModel
//-------------------------------------------------

ImportMameIniDialog::TableModel &ImportMameIniDialog::tableModel()
{
	return *dynamic_cast<TableModel *>(m_ui->tableView->model());
}


//-------------------------------------------------
//  tableModel
//-------------------------------------------------

const ImportMameIniDialog::TableModel &ImportMameIniDialog::tableModel() const
{
	return *dynamic_cast<const TableModel *>(m_ui->tableView->model());
}


//-------------------------------------------------
//  apply
//-------------------------------------------------

void ImportMameIniDialog::apply()
{
	tableModel().job().apply();
}


//-------------------------------------------------
//  hasImportables
//-------------------------------------------------

bool ImportMameIniDialog::hasImportables() const
{
	// find the entries
	const std::vector<ImportMameIniJob::Entry::ptr>& entries = tableModel().job().entries();

	// find an entry that is not 'ImportAction::AlreadyPresent'
	auto iter = std::ranges::find_if(entries, [](const ImportMameIniJob::Entry::ptr& x)
	{
		return x->importAction() != ImportMameIniJob::ImportAction::AlreadyPresent;
	});

	// did we find anything?
	return iter != entries.end();
}


//-------------------------------------------------
//  updateButtonsEnabled
//-------------------------------------------------

void ImportMameIniDialog::updateButtonsEnabled()
{
	bool anyAction = false;
	for (const ImportMameIniJob::Entry::ptr &entry : tableModel().job().entries())
	{
		if ((entry->importAction() == ImportMameIniJob::ImportAction::Supplement)
			|| (entry->importAction() == ImportMameIniJob::ImportAction::Replace))
		{
			anyAction = true;
		}
	}

	// set the ok button enabled property
	m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(anyAction);
}


//-------------------------------------------------
//  tableViewCurrentChanged
//-------------------------------------------------

void ImportMameIniDialog::tableViewCurrentChanged(const QModelIndex &current)
{
	const ImportMameIniJob::Entry *entry = tableModel().entry(current);
	m_ui->explanationLabel->setVisible(entry != nullptr);
	m_ui->explanationLabel->setText(entry ? entry->explanationDisplayText() : "");
}


//-------------------------------------------------
//  importActionString
//-------------------------------------------------

QString ImportMameIniDialog::importActionString(ImportAction importAction)
{
	QString result;
	switch (importAction)
	{
	case ImportAction::Ignore:
		result = "Ignore";
		break;
	case ImportAction::Supplement:
		result = "Supplement";
		break;
	case ImportAction::Replace:
		result = "Replace";
		break;
	case ImportAction::AlreadyPresent:
		result = "(already present)";
		break;
	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  TableModel ctor
//-------------------------------------------------

ImportMameIniDialog::TableModel::TableModel(Preferences &prefs, QObject *parent)
	: QAbstractListModel(parent)
	, m_job(prefs)
{
}


//-------------------------------------------------
//  TableModel::loadMameIni
//-------------------------------------------------

bool ImportMameIniDialog::TableModel::loadMameIni(const QString &fileName)
{
	beginResetModel();
	bool success = m_job.loadMameIni(fileName);
	endResetModel();
	return success;
}


//-------------------------------------------------
//  TableModel::entry
//-------------------------------------------------

const ImportMameIniJob::Entry *ImportMameIniDialog::TableModel::entry(const QModelIndex &index) const
{
	bool isValid = index.isValid()
		&& index.row() >= 0
		&& index.row() < m_job.entries().size();
	return isValid
		? &*m_job.entries()[index.row()]
		: nullptr;
}


//-------------------------------------------------
//  TableModel::entry
//-------------------------------------------------

ImportMameIniJob::Entry *ImportMameIniDialog::TableModel::entry(const QModelIndex &index)
{
	bool isValid = index.isValid()
		&& index.row() >= 0
		&& index.row() < m_job.entries().size();
	return isValid
		? &*m_job.entries()[index.row()]
		: nullptr;
}


//-------------------------------------------------
//  TableModel::rowCount
//-------------------------------------------------

int ImportMameIniDialog::TableModel::rowCount(const QModelIndex &parent) const
{
	return util::safe_static_cast<int>(m_job.entries().size());
}


//-------------------------------------------------
//  TableModel::columnCount
//-------------------------------------------------

int ImportMameIniDialog::TableModel::columnCount(const QModelIndex &parent) const
{
	return util::enum_count<Column>();
}


//-------------------------------------------------
//  TableModel::data
//-------------------------------------------------

QVariant ImportMameIniDialog::TableModel::data(const QModelIndex &index, int role) const
{
	QVariant result;
	const ImportMameIniJob::Entry *entry = TableModel::entry(index);
	if (entry)
	{
		Column column = (Column)index.column();

		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case Column::Label:
				result = entry->labelDisplayText();
				break;
			case Column::Value:
				result = entry->valueDisplayText();
				break;
			case Column::Action:
				result = (int)entry->importAction();
				break;
			}
			break;

		case Qt::FontRole:
			if (column == Column::Action)
			{
				std::optional<QFont> font;
				switch (entry->importAction())
				{
				case ImportMameIniJob::ImportAction::Ignore:
					// do nothing
					break;
				case ImportMameIniJob::ImportAction::Supplement:
				case ImportMameIniJob::ImportAction::Replace:
					font.emplace();
					font->setBold(true);
					break;
				case ImportMameIniJob::ImportAction::AlreadyPresent:
					font.emplace();
					font->setItalic(true);
					break;
				}
				if (font.has_value())
					result = std::move(font.value());
			}
			break;

		case Qt::EditRole:
			if (column == Column::Action)
			{
				result = (int)entry->importAction();
			}
			break;
		}
	}
	return result;
}


//-------------------------------------------------
//  TableModel::flags
//-------------------------------------------------

Qt::ItemFlags ImportMameIniDialog::TableModel::flags(const QModelIndex &index) const
{
	const ImportMameIniJob::Entry *entry = TableModel::entry(index);
	bool isEditable = entry
		&& entry->isEditable()
		&& (Column)index.column() == Column::Action;

	return isEditable
		? QAbstractListModel::flags(index) | Qt::ItemIsEditable
		: QAbstractListModel::flags(index);
}


//-------------------------------------------------
//  TableModel::setData
//-------------------------------------------------

bool ImportMameIniDialog::TableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	bool result = false;
	ImportMameIniJob::Entry *entry = TableModel::entry(index);
	if (entry
		&& (Column)index.column() == Column::Action
		&& role == Qt::EditRole)
	{
		auto importAction = (ImportMameIniJob::ImportAction)value.toInt();
		if (entry->importAction() != importAction)
		{
			entry->setImportAction(importAction);
			QVector<int> roles = { Qt::DisplayRole };
			emit dataChanged(index, index, roles);
		}
		result = true;
	}
	return result;
}


//-------------------------------------------------
//  ActionColumnItemDelegate ctor
//-------------------------------------------------

ImportMameIniDialog::ActionColumnItemDelegate::ActionColumnItemDelegate(TableModel &model, QObject *parent)
	: QStyledItemDelegate(parent)
	, m_model(model)
{
}


//-------------------------------------------------
//  ActionColumnItemDelegate::displayText
//-------------------------------------------------

QString ImportMameIniDialog::ActionColumnItemDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
	ImportAction importAction = (ImportAction)value.toInt();
	return importActionString(importAction);
}


//-------------------------------------------------
//  ActionColumnItemDelegate::createEditor
//-------------------------------------------------

QWidget *ImportMameIniDialog::ActionColumnItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	const ImportMameIniJob::Entry *entry = m_model.entry(index);
	bool canSupplement = entry && entry->canSupplement();
	bool canReplace = entry && entry->canReplace();

	QComboBox &comboBox = *new QComboBox(parent);
	comboBox.addItem(importActionString(ImportAction::Ignore));
	if (canSupplement)
		comboBox.addItem(importActionString(ImportAction::Supplement));
	if (canReplace)
		comboBox.addItem(importActionString(ImportAction::Replace));
	return &comboBox;
}


//-------------------------------------------------
//  ActionColumnItemDelegate::setEditorData
//-------------------------------------------------

void ImportMameIniDialog::ActionColumnItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	ImportAction importAction = (ImportAction)index.data(Qt::EditRole).toInt();

	QComboBox &comboBox = *reinterpret_cast<QComboBox *>(editor);
	comboBox.setCurrentIndex((int)importAction);
}


//-------------------------------------------------
//  ActionColumnItemDelegate::setModelData
//-------------------------------------------------

void ImportMameIniDialog::ActionColumnItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
	QComboBox &comboBox = *reinterpret_cast<QComboBox *>(editor);
	if (comboBox.currentIndex() >= 0)
	{
		ImportAction importAction = (ImportAction)comboBox.currentIndex();
		model->setData(index, (int)importAction, Qt::EditRole);
	}
}


//-------------------------------------------------
//  ActionColumnItemDelegate::updateEditorGeometry
//-------------------------------------------------

void ImportMameIniDialog::ActionColumnItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	editor->setGeometry(option.rect);
}
