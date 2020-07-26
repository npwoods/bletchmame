/***************************************************************************

    dialogs/inputs_base.h

    Base class for Inputs and Switches dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_INPUTS_BASE_H
#define DIALOGS_INPUTS_BASE_H

#include <QDialog>
#include "status.h"

QT_BEGIN_NAMESPACE
namespace Ui { class InputsDialogBase; }
QT_END_NAMESPACE


// ======================> InputsDialogBase

class InputsDialogBase : public QDialog
{
    Q_OBJECT
public:
    InputsDialogBase(QWidget *parent, status::input::input_class input_class, bool restoreEnabled = true);
    ~InputsDialogBase();

protected:
    bool isRelevantInputClass(status::input::input_class inputClass) const;
    void addWidgetsToGrid(int row, std::initializer_list<std::reference_wrapper<QWidget>> widgets);
    virtual void OnRestoreButtonPressed() = 0;

private slots:
    void on_restoreDefaultsButton_clicked();

private:
    std::unique_ptr<Ui::InputsDialogBase>	m_ui;
    status::input::input_class			    m_input_class;

    static QString getDialogTitle(status::input::input_class input_class);
};

#endif // DIALOGS_INPUTS_BASE_H
