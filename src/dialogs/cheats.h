/***************************************************************************

    dialogs/cheats.h

    Cheats dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_CHEATS_H
#define DIALOGS_CHEATS_H

#include <QDialog>
#include <observable/observable.hpp>

#include "status.h"

QT_BEGIN_NAMESPACE
namespace Ui { class CheatsDialog; }
class QCheckBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

// ======================> ICheatsHost

class ICheatsHost
{
public:
	virtual observable::value<std::vector<status::cheat>> &getCheats() = 0;
    virtual void setCheatState(const QString &id, bool enabled, std::optional<std::uint64_t> parameter = { }) = 0;
};


// ======================> PathsDialog

class CheatsDialog : public QDialog
{
    Q_OBJECT
public:
    CheatsDialog(QWidget *parent, ICheatsHost &host);
    ~CheatsDialog();

private:
    enum class CheatType
    {
        Text,
        OneShot,
        OnOff,
        OneShotParameter,
        ValueParameter,
        ItemListParameter
    };

    class CheatWidgeterBase;
    class TextCheatWidgeter;
    class OneShotCheatWidgeter;
    class OnOffCheatWidgeter;
    class OneShotParameterCheatWidgeter;
    class ValueParameterCheatWidgeter;
    class ItemListParameterCheatWidgeter;

    std::unique_ptr<Ui::CheatsDialog>		        m_ui;
    ICheatsHost &                                   m_host;
    observable::unique_subscription                 m_subscription;
    std::vector<std::unique_ptr<CheatWidgeterBase>> m_cheatWidgeters;

    void updateCheats();
    static CheatType classifyCheat(const status::cheat &cheat);
    template<typename TWidgeter> CheatWidgeterBase &getCheatWidgeter(int row);
};


#endif // DIALOGS_CHEATS_H
