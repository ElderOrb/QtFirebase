#ifndef QTFIREBASE_H
#define QTFIREBASE_H

#if defined(qFirebase)
#undef qFirebase
#endif
#define qFirebase (static_cast<QtFirebase *>(QtFirebase::instance()))

#include "platformutils.h"

#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/util.h"

#include <QMap>
#include <QObject>
#include <QTimer>
#include <QGuiApplication>

class QtFirebase : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit QtFirebase(QObject* parent = nullptr);
    ~QtFirebase();

    static QtFirebase *instance() {
        if(!self)
            self = new QtFirebase();
        return self;
    }

    bool ready() const;

    static void waitForFutureCompletion(firebase::FutureBase future);
    bool checkInstance(const char *function);

    firebase::App* firebaseApp() const;
    QString instanceId() const;

    // TODO make protected and have friend classes?
    void addFuture(const QString &eventId, const firebase::FutureBase &future);

    void setOptions(const firebase::AppOptions& options);
signals:
    void readyChanged();

    void futureEvent(const QString &eventId, firebase::FutureBase future);

public slots:
    void requestInit();
    void processEvents();

private:
    static QtFirebase *self;
    Q_DISABLE_COPY(QtFirebase)

    bool _ready = false;
    firebase::App* _firebaseApp = nullptr;
    firebase::AppOptions _appOptions;

    QTimer *_initTimer = nullptr;

    QTimer *_futureWatchTimer = nullptr;
    QMap<QString, firebase::FutureBase> _futureMap;
};

class QtFirebaseGetInstanceRequest: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
public:
    QtFirebaseGetInstanceRequest();
    ~QtFirebaseGetInstanceRequest();
public slots:
    void exec();

    //State
    bool running() const;

    int errorId() const;
    bool hasError() const;
    QString errorMsg() const;

public:
    void onFutureEvent(QString eventId, firebase::FutureBase future);
    Q_INVOKABLE QString instanceId() const;

signals:
    void completed(bool success);
    void runningChanged();

private slots:
    void onRun();

private:
    void setComplete(bool value);
    void setError(int errId, const QString& msg = QString());
    void clearError();

    QString m_instanceId;
    bool m_complete;
    int m_errId;
    QString m_errMsg;
};


#endif // QTFIREBASE_H
