#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <functional>

#ifdef ESBOZO_HAS_MULTIMEDIA
#include <QSoundEffect>
#endif

class Sounds : public QObject {
public:
    enum class Id {
        BlockCreated,
        BlockDeleted,
        ChangeFocus,
        WizardAction,
        Error
    };

    explicit Sounds (QObject* parent = nullptr)
        : QObject (parent)
    {
        m_enabled = QSettings().value ("sounds/enabled", true).toBool();
#ifdef ESBOZO_HAS_MULTIMEDIA
        loadEffects();
#endif
    }

    static QString fileName (Id id) {
        switch (id) {
            case Id::BlockCreated: return "block_created.wav";
            case Id::BlockDeleted: return "block_deleted.wav";
            case Id::ChangeFocus: return "change_focus.wav";
            case Id::WizardAction: return "wizard_action.wav";
            case Id::Error: return "error.wav";
        }
        return QString();
    }

    static QList<Id> allIds() {
        return { Id::BlockCreated, Id::BlockDeleted, Id::ChangeFocus, Id::WizardAction, Id::Error };
    }

    bool enabled() const { return m_enabled; }

    bool available() const {
#ifdef ESBOZO_HAS_MULTIMEDIA
        return true;
#else
        return false;
#endif
    }

    void setEnabled (bool enabled, bool persist = true) {
        m_enabled = enabled;
        if (persist)
            QSettings().setValue ("sounds/enabled", enabled);
    }

    void setObserver (std::function<void (Id)> observer) {
        m_observer = std::move (observer);
    }

    void play (Id id) {
        if (!m_enabled)
            return;
        if (m_observer)
            m_observer (id);

#ifdef ESBOZO_HAS_MULTIMEDIA
        QList<QSoundEffect*>& pool = m_pools[static_cast<int> (id)];
        if (pool.isEmpty())
            return;
        int& next = m_next[static_cast<int> (id)];
        QSoundEffect* effect = pool[next % pool.size()];
        ++next;
        effect->play();
#endif
    }

private:
#ifdef ESBOZO_HAS_MULTIMEDIA
    void loadEffects() {
        constexpr int poolSize = 3;
        for (Id id : allIds()) {
            QList<QSoundEffect*> pool;
            for (int i = 0; i < poolSize; ++i) {
                QSoundEffect* effect = new QSoundEffect (this);
                effect->setSource (QUrl (QString ("qrc:/sounds/%1").arg (fileName (id))));
                effect->setVolume (1.0f);
                pool.append (effect);
            }
            m_pools.insert (static_cast<int> (id), pool);
            m_next.insert (static_cast<int> (id), 0);
        }
    }

    QHash<int, QList<QSoundEffect*>> m_pools;
    QHash<int, int> m_next;
#endif

    bool m_enabled = true;
    std::function<void (Id)> m_observer;
};
