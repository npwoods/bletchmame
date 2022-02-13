/***************************************************************************

    dialogs/inputs_multiquick.cpp

    Multi Axis Input Dialog

***************************************************************************/

// bletchmame headers
#include "dialogs/inputs_multiquick.h"
#include "ui_inputs_multiquick.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class InputsDialog::MultipleQuickItemsDialog::Model : public QAbstractItemModel
{
public:
    Model(QWidget *parent, std::vector<QuickItem>::const_iterator iter, int size)
        : QAbstractItemModel(parent)
        , m_iter(iter)
        , m_size(size)
    {
    }

    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const
    {
        return createIndex(row, column);
    }

    virtual QModelIndex parent(const QModelIndex &child) const
    {
        return QModelIndex();
    }

    virtual int rowCount(const QModelIndex &parent) const
    {
        return m_size;
    }
    
    virtual int columnCount(const QModelIndex &parent) const
    {
        return 1;
    }

    virtual QVariant data(const QModelIndex &index, int role) const
    {
        QVariant result;
        const QuickItem *item;

        switch (role)
        {
        case Qt::DisplayRole:
            item = getItemByIndex(index);
            if (item)
                result = item->m_label;
            break;
        }
        return result;
    }

    const QuickItem *getItemByIndex(const QModelIndex &index) const
    {
        return index.row() >= 0 && index.row() < m_size && index.column() == 0
            ? &*(m_iter + index.row())
            : nullptr;
    }

private:
    std::vector<QuickItem>::const_iterator  m_iter;
    int                                     m_size;
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::MultipleQuickItemsDialog::MultipleQuickItemsDialog(InputsDialog &parent, std::vector<QuickItem>::const_iterator first, std::vector<QuickItem>::const_iterator last)
    : QDialog(&parent)
{
    // set up UI
    m_ui = std::make_unique<Ui::MultipleQuickItemsDialog>();
    m_ui->setupUi(this);

    // create a model
    Model &model = *new Model(this, first, last - first);
    m_ui->listView->setModel(&model);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

InputsDialog::MultipleQuickItemsDialog::~MultipleQuickItemsDialog()
{
}


//-------------------------------------------------
//  GetSelectedQuickItems
//-------------------------------------------------

std::vector<std::reference_wrapper<const InputsDialog::QuickItem>> InputsDialog::MultipleQuickItemsDialog::GetSelectedQuickItems() const
{
    // get the selection
    QModelIndexList selection = m_ui->listView->selectionModel()->selectedIndexes();

    // get our model
    Model &model = *dynamic_cast<Model *>(m_ui->listView->model());

    // build the results
    std::vector<std::reference_wrapper<const InputsDialog::QuickItem>> results;
    results.reserve(selection.size());
    for (const QModelIndex &index : selection)
    {
        const QuickItem &item = *model.getItemByIndex(index);
        results.push_back(item);
    }
    return results;
}
