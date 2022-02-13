/***************************************************************************

    dialogs/inputs_multiaxis.cpp

    Multi Axis Input Dialog

***************************************************************************/

// bletchmame headers
#include "dialogs/inputs_multiaxis.h"
#include "ui_inputs_multiaxis.h"

// Qt headers
#include <QLabel>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::MultiAxisInputDialog::MultiAxisInputDialog(InputsDialog &parent, const QString &title, const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref)
    : QDialog(&parent)
{
    // set up UI
    m_ui = std::make_unique<Ui::MultiAxisInputDialog>();
    m_ui->setupUi(this);

    // set the title
    setWindowTitle(title);

    // come up with names for each of the axes
    QString x_axis_label = x_field_ref && y_field_ref ? title + " X" : title;
    QString y_axis_label = x_field_ref && y_field_ref ? title + " Y" : title;

    // configure each of the panels
    m_singular_inputs.reserve(4);
    setupInputPanel(*m_ui->leftLabel,   *m_ui->leftButton,  *m_ui->leftMenuButton,  x_field_ref, status::input_seq::type::DECREMENT, x_axis_label + " Dec");
    setupInputPanel(*m_ui->rightLabel,  *m_ui->rightButton, *m_ui->rightMenuButton, x_field_ref, status::input_seq::type::INCREMENT, x_axis_label + " Inc");
    setupInputPanel(*m_ui->upLabel,     *m_ui->upButton,    *m_ui->upMenuButton,    y_field_ref, status::input_seq::type::DECREMENT, y_axis_label + " Dec");
    setupInputPanel(*m_ui->downLabel,   *m_ui->downButton,  *m_ui->downMenuButton,  y_field_ref, status::input_seq::type::INCREMENT, y_axis_label + " Inc");

    // update the text
    for (SingularInputEntry &entry : m_singular_inputs)
        entry.UpdateText();

    // subscribe to events
    m_inputs_subscription = host().m_host.GetInputs().subscribe([this]() { OnInputsChanged(); });
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

InputsDialog::MultiAxisInputDialog::~MultiAxisInputDialog()
{
}


//-------------------------------------------------
//  on_clearButton_pressed
//-------------------------------------------------

void InputsDialog::MultiAxisInputDialog::on_clearButton_pressed()
{
    SetAllInputs("");
}


//-------------------------------------------------
//  on_restoreDefaultsButton_pressed
//-------------------------------------------------

void InputsDialog::MultiAxisInputDialog::on_restoreDefaultsButton_pressed()
{
    SetAllInputs("*");
}


//-------------------------------------------------
//  setupInputPanel
//-------------------------------------------------

void InputsDialog::MultiAxisInputDialog::setupInputPanel(QLabel &label, QPushButton &button, QPushButton &menuButton, const std::optional<InputFieldRef> &fieldRef, status::input_seq::type seqType, const QString &text)
{
    // only enable these controls if something is specified
    button.setEnabled(fieldRef.has_value());
    menuButton.setEnabled(fieldRef.has_value());
    label.setEnabled(fieldRef.has_value());

    // set the text
    button.setText(text);

    // some activities that only apply when we have a real field
    if (fieldRef)
    {
        // keep track of this input
        m_singular_inputs.emplace_back(host(), button, menuButton, label, InputFieldRef(*fieldRef), seqType);
    }
}


//-------------------------------------------------
//  OnInputsChanged
//-------------------------------------------------

void InputsDialog::MultiAxisInputDialog::OnInputsChanged()
{
    for (SingularInputEntry &entry : m_singular_inputs)
        entry.UpdateText();
}


//-------------------------------------------------
//  SetAllInputs
//-------------------------------------------------

void InputsDialog::MultiAxisInputDialog::SetAllInputs(const QString &tokens)
{
    std::vector<SetInputSeqRequest> seqs;
    seqs.reserve(m_singular_inputs.size());

    for (SingularInputEntry &entry : m_singular_inputs)
    {
        for (auto &[field_ref, seq_type] : entry.GetInputSeqRefs())
            seqs.emplace_back(std::move(field_ref.m_port_tag), field_ref.m_mask, seq_type, tokens);
    }

    host().SetInputSeqs(std::move(seqs));
}


//-------------------------------------------------
//  host
//-------------------------------------------------

InputsDialog &InputsDialog::MultiAxisInputDialog::host()
{
    return *dynamic_cast<InputsDialog *>(parent());
}

