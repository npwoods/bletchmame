/***************************************************************************

    dialogs/inputs_multiaxis.h

    Multi Axis Input Dialog

***************************************************************************/

#ifndef DIALOGS_INPUTS_MULTIAXIS
#define DIALOGS_INPUTS_MULTIAXIS

// bletchmame headers
#include "dialogs/inputs.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MultiAxisInputDialog; }
QT_END_NAMESPACE

class InputsDialog::MultiAxisInputDialog : public QDialog
{
Q_OBJECT
public:
    MultiAxisInputDialog(InputsDialog &parent, const QString &title, const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref);
    ~MultiAxisInputDialog();

private slots:
    void on_clearButton_pressed();
    void on_restoreDefaultsButton_pressed();

private:
    std::unique_ptr<Ui::MultiAxisInputDialog>   m_ui;
    std::vector<SingularInputEntry>	            m_singular_inputs;
    observable::unique_subscription		        m_inputs_subscription;

    void setupInputPanel(QLabel &label, QPushButton &button, QPushButton &menuButton, const std::optional<InputFieldRef> &fieldRef, status::input_seq::type seqType, const QString &text);
    void OnInputsChanged();
    void SetAllInputs(const QString &tokens);
    InputsDialog &host();
};



#endif // DIALOGS_INPUTS_MULTIAXIS
