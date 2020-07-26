/***************************************************************************

    dialogs/inputs_multiaxis.h

    Multi Axis Input Dialog

***************************************************************************/

#ifndef DIALOGS_INPUTS_MULTIAXIS
#define DIALOGS_INPUTS_MULTIAXIS

#include "dialogs/inputs.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MultiAxisInputDialog; }
QT_END_NAMESPACE

class InputsDialog::MultiAxisInputDialog : public QDialog
{
Q_OBJECT
public:
    MultiAxisInputDialog(InputsDialog &parent, const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref);
    ~MultiAxisInputDialog();

private:
    std::unique_ptr<Ui::MultiAxisInputDialog> m_ui;
};



#endif // DIALOGS_INPUTS_MULTIAXIS
