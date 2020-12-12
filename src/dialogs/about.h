/***************************************************************************

    dialogs/about.h

    BletchMAME About Dialog

***************************************************************************/

#ifndef DIALOGS_ABOUT_H
#define DIALOGS_ABOUT_H

#include <QDialog>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class AboutDialog; }
class QTextStream;
QT_END_NAMESPACE

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    class Test;

    explicit AboutDialog(QWidget *parent, const QString &mameVersion);
    ~AboutDialog();

private:
    std::unique_ptr<Ui::AboutDialog>    m_ui;

    static QString getPrettyMameVersion(const QString &mameVersion);
    void getExtraText(QTextStream &stream, const QString &prettyMameVersion);
};

#endif // DIALOGS_ABOUT_H
