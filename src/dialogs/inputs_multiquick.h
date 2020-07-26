/***************************************************************************

    dialogs/inputs_multiquick.h

    Multi Axis Input Dialog

***************************************************************************/

#ifndef DIALOGS_INPUTS_MULTIQUICK
#define DIALOGS_INPUTS_MULTIQUICK

#include "dialogs/inputs.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MultipleQuickItemsDialog; }
QT_END_NAMESPACE

class InputsDialog::MultipleQuickItemsDialog : public QDialog
{
    Q_OBJECT
public:
    MultipleQuickItemsDialog(InputsDialog &parent, std::vector<QuickItem>::const_iterator first, std::vector<QuickItem>::const_iterator last);
    ~MultipleQuickItemsDialog();

private:
    std::unique_ptr<Ui::MultipleQuickItemsDialog> m_ui;
};

#endif // DIALOGS_INPUTS_MULTIQUICK
