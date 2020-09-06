/***************************************************************************

    dialogs/cheats.h

    Cheats dialog

***************************************************************************/

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QStringListModel>

#include "dialogs/cheats.h"
#include "ui_cheats.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
    // ======================> CheatWidgeterHost
    class CheatWidgeterHost
    {
    public:
        const int TOTAL_COLUMNS = 2;

        CheatWidgeterHost(QWidget *widgetParent, ICheatsHost &cheatsHost, QGridLayout &layout, int row)
            : m_widgetParent(widgetParent)
            , m_cheatsHost(cheatsHost)
            , m_layout(layout)
            , m_row(row)
            , m_column(0)
        {
        }

        // accessors
        ICheatsHost &cheatsHost()   { return m_cheatsHost; }

        // widget setting
        template<typename T>
        T &createWidget(int columnSpan = TOTAL_COLUMNS)
        {
            // sanity check
            assert((columnSpan > 0) && (m_column + columnSpan) <= TOTAL_COLUMNS);

            // create the widget
            T &widget = *new T(m_widgetParent);

            // add it to the layout
            m_layout.addWidget(&widget, m_row, m_column, 1, columnSpan);

            // advance the column
            m_column += columnSpan;

            // and return it
            return widget;
        }

    private:
        QWidget *       m_widgetParent;
        ICheatsHost &   m_cheatsHost;
        QGridLayout &   m_layout;
        int             m_row;
        int             m_column;
    };

};


// ======================> CheatWidgeterBase
class CheatsDialog::CheatWidgeterBase : public QObject
{
public:
    virtual void update(const status::cheat &cheat) = 0;
};


// ======================> TextCheatWidgeter
class CheatsDialog::TextCheatWidgeter : public CheatWidgeterBase
{
public:
    TextCheatWidgeter(CheatWidgeterHost &host)
        : m_label(host.createWidget<QLabel>(2))
    {
    }

    virtual void update(const status::cheat &cheat) override
    {
        m_label.setText(cheat.m_description);
        m_label.setToolTip(cheat.m_comment);
    }

private:
    QLabel &    m_label;
};


// ======================> OneShotCheatWidgeter
class CheatsDialog::OneShotCheatWidgeter : public CheatWidgeterBase
{
public:
    OneShotCheatWidgeter(CheatWidgeterHost &host)
        : m_cheatsHost(host.cheatsHost())
        , m_button(host.createWidget<QPushButton>(2))
    {
        connect(&m_button, &QPushButton::clicked, this, [this]() { m_cheatsHost.setCheatState(m_id, true); });
    }

    virtual void update(const status::cheat &cheat) override
    {
        m_id = cheat.m_id;
        m_button.setText(cheat.m_description);
        m_button.setToolTip(cheat.m_comment);
    }

private:
    ICheatsHost &   m_cheatsHost;
    QString         m_id;
    QPushButton &   m_button;
};


// ======================> OnOffCheatWidgeter
class CheatsDialog::OnOffCheatWidgeter : public CheatWidgeterBase
{
public:
    OnOffCheatWidgeter(CheatWidgeterHost &host)
        : m_cheatsHost(host.cheatsHost())
        , m_checkBox(host.createWidget<QCheckBox>(2))
    {
        connect(&m_checkBox, &QPushButton::clicked, this, [this]() { onClicked(); });
    }

    virtual void update(const status::cheat &cheat) override
    {
        m_id = cheat.m_id;
        m_checkBox.setText(cheat.m_description);
        m_checkBox.setToolTip(cheat.m_comment);
        m_checkBox.setChecked(cheat.m_enabled);
    }

private:
    ICheatsHost &   m_cheatsHost;
    QCheckBox &     m_checkBox;
    QString         m_id;

    void onClicked()
    {
        m_cheatsHost.setCheatState(m_id, m_checkBox.isChecked());
    }
};


// ======================> OneShotParameterCheatWidgeter
class CheatsDialog::OneShotParameterCheatWidgeter : public CheatWidgeterBase
{
public:
    OneShotParameterCheatWidgeter(CheatWidgeterHost &host)
        : m_cheatsHost(host.cheatsHost())
        , m_button(host.createWidget<QPushButton>(2))
    {
        connect(&m_button, &QPushButton::clicked, this, [this]() { onClicked(); });
    }

    virtual void update(const status::cheat &cheat) override
    {
        m_button.setText(cheat.m_description);
        m_button.setToolTip(cheat.m_comment);

        if (m_id != cheat.m_id)
        {
            m_id = cheat.m_id;
            m_items = cheat.m_parameter->m_items;
        }
    }

private:
    ICheatsHost &                       m_cheatsHost;
    QString                             m_id;
    QPushButton &                       m_button;
    std::map<std::uint64_t, QString>	m_items;

    void onClicked()
    {
        // create the popup menu
        QMenu popupMenu;
        for (const auto &pair : m_items)
        {
            std::uint64_t parameter = pair.first;
            popupMenu.addAction(pair.second, [this, parameter]() { m_cheatsHost.setCheatState(m_id, true, parameter); });
        }

        // and execute
        QPoint popupPos = globalPositionBelowWidget(m_button);
        popupMenu.exec(popupPos);
    }
};


// ======================> ValueParameterCheatWidgeter
class CheatsDialog::ValueParameterCheatWidgeter : public CheatWidgeterBase
{
public:
    ValueParameterCheatWidgeter(CheatWidgeterHost &host)
        : m_cheatsHost(host.cheatsHost())
        , m_label(host.createWidget<QLabel>(1))
        , m_slider(host.createWidget<QSlider>(1))
        , m_activateWhenValueChanged(false)
    {
        m_slider.setOrientation(Qt::Horizontal);
        m_slider.setTracking(false);

        connect(&m_slider, &QSlider::valueChanged, this, [this]() { onValueChanged(); });
    }

    virtual void update(const status::cheat &cheat) override
    {
        m_id = cheat.m_id;
        m_activateWhenValueChanged = cheat.m_has_change_script;
        m_label.setText(cheat.m_description);
        m_label.setToolTip(cheat.m_comment);
        m_slider.setMinimum(cheat.m_parameter->m_minimum);
        m_slider.setMaximum(cheat.m_parameter->m_maximum);
        m_slider.setValue(cheat.m_parameter->m_value);
    }

private:
    ICheatsHost &   m_cheatsHost;
    QLabel &        m_label;
    QSlider &       m_slider;
    QString         m_id;
    bool            m_activateWhenValueChanged;

    void onValueChanged()
    {
        if (m_activateWhenValueChanged)
            m_cheatsHost.setCheatState(m_id, true, m_slider.value());
    }
};


// ======================> ItemListParameterCheatWidgeter
class CheatsDialog::ItemListParameterCheatWidgeter : public CheatWidgeterBase
{
public:
    ItemListParameterCheatWidgeter(CheatWidgeterHost &host)
        : m_cheatsHost(host.cheatsHost())
        , m_label(host.createWidget<QLabel>(1))
        , m_comboBox(host.createWidget<QComboBox>(1))
    {
        QStringListModel &comboBoxModel = *new QStringListModel(&m_comboBox);
        m_comboBox.setModel(&comboBoxModel);
    }

    virtual void update(const status::cheat &cheat) override
    {
        m_label.setText(cheat.m_description);
        m_label.setToolTip(cheat.m_comment);

        if (m_id != cheat.m_id)
        {
            m_id = cheat.m_id;

            QStringList list;
            for (const auto &keyVal : cheat.m_parameter->m_items)
                list << keyVal.second;

            QStringListModel &model = *dynamic_cast<QStringListModel *>(m_comboBox.model());
            model.setStringList(list);
        }
    }

private:
    ICheatsHost &   m_cheatsHost;
    QString         m_id;
    QLabel &        m_label;
    QComboBox &     m_comboBox;
};


//-------------------------------------------------
//  ctor
//-------------------------------------------------

CheatsDialog::CheatsDialog(QWidget *parent, ICheatsHost &host)
    : QDialog(parent)
    , m_host(host)
{
    // set up user interface
    m_ui = std::make_unique<Ui::CheatsDialog>();
    m_ui->setupUi(this);

    // we want to reflect the cheats in our UI
    m_subscription = m_host.getCheats().subscribe_and_call([this] { updateCheats(); });
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

CheatsDialog::~CheatsDialog()
{
}


//-------------------------------------------------
//  updateCheats
//-------------------------------------------------

void CheatsDialog::updateCheats()
{
    // get the list of cheats
    const std::vector<status::cheat> &cheats = m_host.getCheats().get();

    // reserve space for widgeters
    m_cheatWidgeters.reserve(cheats.size());

    int row = 0;
    for (const status::cheat &cheat : cheats)
    {
        // identify the type of cheat
        CheatType cheatType = classifyCheat(cheat);

        // get/create the cheat "widgeter" - responsible for a row of widgets on our grid
        CheatWidgeterBase *cheatWidget;
        switch (cheatType)
        {
        case CheatType::Text:
            cheatWidget = cheat.m_description.isEmpty()
                ? &getCheatWidgeter<TextCheatWidgeter>(row++)
                : nullptr;
            break;

        case CheatType::OneShot:
            cheatWidget = &getCheatWidgeter<OneShotCheatWidgeter>(row++);
            break;

        case CheatType::OnOff:
            cheatWidget = &getCheatWidgeter<OnOffCheatWidgeter>(row++);
            break;

        case CheatType::OneShotParameter:
            cheatWidget = &getCheatWidgeter<OneShotParameterCheatWidgeter>(row++);
            break;

        case CheatType::ValueParameter:
            cheatWidget = &getCheatWidgeter<ValueParameterCheatWidgeter>(row++);
            break;

        case CheatType::ItemListParameter:
            cheatWidget = &getCheatWidgeter<ItemListParameterCheatWidgeter>(row++);
            break;

        default:
            throw false;
        }

        // and update it
        if (cheatWidget)
            cheatWidget->update(cheat);
    }

    // be nice here
    m_cheatWidgeters.resize(row);
    m_cheatWidgeters.shrink_to_fit();
}


//-------------------------------------------------
//  classifyCheat
//-------------------------------------------------

CheatsDialog::CheatType CheatsDialog::classifyCheat(const status::cheat &cheat)
{
    CheatType result;
    if (!cheat.m_has_on_script && !cheat.m_has_off_script && !cheat.m_has_run_script && !cheat.m_parameter)
    {
        result = CheatType::Text;
    }
    else if (cheat.m_has_on_script && !cheat.m_has_off_script && !cheat.m_has_run_script && !cheat.m_parameter)
    {
        result = CheatType::OneShot;
    }
    else if ((cheat.m_has_on_script && cheat.m_has_off_script) || cheat.m_has_run_script && !cheat.m_parameter)
    {
        result = CheatType::OnOff;
    }
    else if (!cheat.m_has_on_script && !cheat.m_has_off_script && cheat.m_has_change_script && cheat.m_parameter)
    {
        result = cheat.m_parameter->m_items.empty()
            ? CheatType::ValueParameter
            : CheatType::OneShotParameter;
    }
    else if (cheat.m_parameter)
    {
        result = cheat.m_parameter->m_items.empty()
            ? CheatType::ValueParameter
            : CheatType::ItemListParameter;
    }
    else
    {
        throw false;
    }
    return result;
}


//-------------------------------------------------
//  getCheatWidgeter
//-------------------------------------------------

template<typename TWidgeter>
CheatsDialog::CheatWidgeterBase &CheatsDialog::getCheatWidgeter(int row)
{
    // resize if we need to do so
    if (row >= m_cheatWidgeters.size())
        m_cheatWidgeters.resize((size_t)row + 1);

    // do we have a widgeter of the appropriate type?
    if (!m_cheatWidgeters[row] || !dynamic_cast<TWidgeter *>(m_cheatWidgeters[row].get()))
    {
        // if not, configure the host so we can...
        CheatWidgeterHost host(this, m_host, *m_ui->gridLayout, row);

        // ...create it
        m_cheatWidgeters[row] = std::make_unique<TWidgeter>(host);
    }

    // and return it
    return *m_cheatWidgeters[row];
}
