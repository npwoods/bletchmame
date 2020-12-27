/***************************************************************************

    sessionbehavior.h

    Abstracting runtime behaviors of the emulation (e.g. - profiles)

***************************************************************************/

#ifndef SESSIONEBEHAVIOR_H
#define SESSIONEBEHAVIOR_H

#include "status.h"
#include "profile.h"


// ======================> SessionBehavior

class SessionBehavior
{
public:
    virtual ~SessionBehavior();
    virtual QString getInitialSoftware() const = 0;
    virtual std::map<QString, QString> getOptions() const = 0;
    virtual std::map<QString, QString> getImages() const = 0;
    virtual QString getSavedState() const = 0;
    virtual void persistState(const std::vector<status::slot> &devslots, const std::vector<status::image> &images) = 0;
    virtual bool shouldPromptOnStop() const = 0;
};


// ======================> NormalSessionBehavior

class NormalSessionBehavior : public SessionBehavior
{
public:
    // ctor/dtor
    NormalSessionBehavior(const software_list::software *software);

    // virtuals
    virtual QString getInitialSoftware() const override;
    virtual std::map<QString, QString> getOptions() const override;
    virtual std::map<QString, QString> getImages() const override;
    virtual QString getSavedState() const override;
    virtual void persistState(const std::vector<status::slot> &devslots, const std::vector<status::image> &images) override;
    virtual bool shouldPromptOnStop() const override;

private:
    QString m_initialSoftware;
};


// ======================> ProfileSessionBehavior

class ProfileSessionBehavior : public SessionBehavior
{
public:
    // ctor/dtor
    ProfileSessionBehavior(std::shared_ptr<profiles::profile> &&profile);

    // virtuals
    virtual QString getInitialSoftware() const override;
    virtual std::map<QString, QString> getOptions() const override;
    virtual std::map<QString, QString> getImages() const override;
    virtual QString getSavedState() const override;
    virtual void persistState(const std::vector<status::slot> &devslots, const std::vector<status::image> &images) override;
    virtual bool shouldPromptOnStop() const override;

private:
    std::shared_ptr<profiles::profile>  m_profile;
};


#endif // SESSIONEBEHAVIOR_H
