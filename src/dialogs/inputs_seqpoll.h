/***************************************************************************

    dialogs/inputs_seqpoll.h

    Input Sequence (InputSeq) polling dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_INPUTS_SEQPOLL_H
#define DIALOGS_INPUTS_SEQPOLL_H

#include <QDialog>

#include "dialogs/inputs.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SeqPollingDialog; }
QT_END_NAMESPACE

class IInputsHost;

class InputsDialog::SeqPollingDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Type
    {
        SPECIFY,
        ADD
    };

    // ctor/dtor
    SeqPollingDialog(InputsDialog &host, Type type, const QString &label);
    ~SeqPollingDialog();

    // accessor
    QString &DialogSelectedResult() { return m_dialog_selected_result; }

private slots:
    void on_specifyMouseInputButton_clicked();

private:
    std::unique_ptr<Ui::SeqPollingDialog>   m_ui;
    QString                                 m_dialog_selected_result;

    const std::vector<status::input_class> &GetInputClasses();
};


#endif // DIALOGS_INPUTS_SEQPOLL_H
