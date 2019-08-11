#include "qtfirebase.h"
#include "firebase/instance_id.h"
#include <QMutableMapIterator>
#include <QThread>
#include <firebase/instance_id.h>

QtFirebase *QtFirebase::self = nullptr;

QtFirebase::QtFirebase(QObject* parent) : QObject(parent)
{
    _ready = false;

    qDebug() << self << ":QtFirebase(QObject* parent)" ;

    Q_ASSERT_X(!self, "QtFirebase", "there should be only one firebase object");
    QtFirebase::self = this;

    // NOTE where having trouble getting a valid UIView pointer using just signals.
    // So this is a hack that let's us check the pointer every 1 seconds during startup.
    // We should probably set a limit to how many checks should be done before giving up?
    //connect(qGuiApp,&QGuiApplication::focusWindowChanged, this, &QtFirebase::init); // <-- Crashes on iOS
    _initTimer = new QTimer(self);
    _initTimer->setSingleShot(false);
    connect(_initTimer, &QTimer::timeout, self, &QtFirebase::requestInit);
    _initTimer->start(1000);

    _futureWatchTimer = new QTimer(self);
    _futureWatchTimer->setSingleShot(false);
    connect(_futureWatchTimer, &QTimer::timeout, self, &QtFirebase::processEvents);
}

QtFirebase::~QtFirebase()
{
    self = nullptr;
    //delete _firebaseApp;
}

bool QtFirebase::checkInstance(const char *function)
{
    bool b = (QtFirebase::self != nullptr);
    if (!b)
        qWarning("QtFirebase::%s: Please instantiate the QtFirebase object first", function);
    return b;
}

bool QtFirebase::ready() const
{
    return _ready;
}

void QtFirebase::waitForFutureCompletion(firebase::FutureBase future)
{
    static int count = 0;
    qDebug() << self << "::waitForFutureCompletion" << "waiting for future" << &future << "completion. Initial status" << future.status();
    while(future.status() == firebase::kFutureStatusPending) {
        QGuiApplication::processEvents();
        count++;

        if(count % 100 == 0)
            qDebug() << count << "Future" << &future << "is still pending. Has current status" << future.status();

        if(count % 200 == 0) {
            qDebug() << count << "Future" << &future << "is still pending. Something is probably wrong. Breaking wait cycle. Current status" << future.status();
            count = 0;
            break;
        }
        QThread::msleep(10);
    }
    count = 0;

    if(future.status() == firebase::kFutureStatusComplete) {
       qDebug() << self << "::waitForFutureCompletion" << "ended with COMPLETE";
    }

    if(future.status() == firebase::kFutureStatusInvalid) {
       qDebug() << self << "::waitForFutureCompletion" << "ended with INVALID";
    }

    if(future.status() == firebase::kFutureStatusPending) {
       qDebug() << self << "::waitForFutureCompletion" << "ended with PENDING";
    }
}

firebase::App* QtFirebase::firebaseApp() const
{
    return _firebaseApp;
}

void QtFirebase::addFuture(const QString &eventId, const firebase::FutureBase &future)
{
    qDebug() << self << "::addFuture" << "adding" << eventId;

    if (_futureMap.contains(eventId))
    {
        qWarning() << "_futureMap already contains '" << eventId << "'.";
    }

    _futureMap.insert(eventId,future);

    //if(!_futureWatchTimer->isActive()) {
    qDebug() << self << "::addFuture" << "starting future watch";
    _futureWatchTimer->start(1000);
    //}

}

void QtFirebase::setOptions(const firebase::AppOptions &options)
{
    _appOptions = options;
}

void QtFirebase::requestInit()
{
    #if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    if(!PlatformUtils::getNativeWindow()) {
        qDebug() << self << "::requestInit" << "no native UI pointer";
        return;
    }
    #endif

    if(!_ready) {

        #if defined(Q_OS_ANDROID)

        jobject activity = PlatformUtils::getNativeWindow();

        QAndroidJniEnvironment env;

        // Create the Firebase app.
        _firebaseApp = firebase::App::Create(_appOptions, env, activity);

        #else // Q_OS_ANDROID

        // Create the Firebase app.
        _firebaseApp = firebase::App::Create(_appOptions);

        #endif

        qDebug("QtFirebase::requestInit created the Firebase App (%x)",static_cast<int>(reinterpret_cast<intptr_t>(_firebaseApp)));

        _initTimer->stop();
        delete _initTimer;

        _ready = true;
        qDebug() << self << "::requestInit" << "initialized";
        emit readyChanged();
    }
}

void QtFirebase::processEvents()
{
    qDebug() << self << "::processEvents" << "processing events";
    QMutableMapIterator<QString, firebase::FutureBase> i(_futureMap);
    while (i.hasNext()) {
        i.next();
        if(i.value().status() != firebase::kFutureStatusPending) {
            qDebug() << self << "::processEvents" << "future event" << i.key();
            //if(_futureMap.remove(i.key()) >= 1) //QMap holds only one key. Use QMultiMap for multiple keys.
            const auto key = i.key();
            const auto value = i.value();
            i.remove();
            qDebug() << self << "::processEvents" << "removed future event" << key;
            emit futureEvent(key, value);
            // To easen events up:
            //break;
        }

    }

    if(_futureMap.isEmpty()) {
        qDebug() << self << "::processEvents" << "stopping future watch";
        _futureWatchTimer->stop();
    }

}

QtFirebaseGetInstanceRequest::QtFirebaseGetInstanceRequest():
    m_complete(true)
{
    clearError();
}

QtFirebaseGetInstanceRequest::~QtFirebaseGetInstanceRequest()
{
}

void QtFirebaseGetInstanceRequest::exec()
{
    if(!running())
    {
        connect(QtFirebase::instance(), &QtFirebase::futureEvent, this, &QtFirebaseGetInstanceRequest::onFutureEvent, Qt::UniqueConnection);

        setComplete(false);

        firebase::InitResult initResult;
        auto instanceId = firebase::instance_id::InstanceId::GetInstanceId(QtFirebase::instance()->firebaseApp(), &initResult);
        auto getInstanceId = instanceId->GetId();

        QtFirebase::instance()->addFuture("getInstance", getInstanceId);
    }
}

int QtFirebaseGetInstanceRequest::errorId() const
{
    return m_errId;
}

bool QtFirebaseGetInstanceRequest::hasError() const
{
    return m_errId != firebase::instance_id::kErrorNone;
}

QString QtFirebaseGetInstanceRequest::errorMsg() const
{
    return m_errMsg;
}

void QtFirebaseGetInstanceRequest::onFutureEvent(QString eventId, firebase::FutureBase future)
{
    disconnect(QtFirebase::instance(), &QtFirebase::futureEvent, this, &QtFirebaseGetInstanceRequest::onFutureEvent);

    if(future.status() != firebase::kFutureStatusComplete)
    {
        qDebug() << this << "::onFutureEvent " << "ERROR: Action failed with status: " << future.status();
        setError(0);
    }
    else if (future.error() != firebase::instance_id::kErrorNone)
    {
        qDebug() << this << "::onFutureEvent Error occured in result:" << future.error() << future.error_message();
        setError(future.error(), future.error_message());
    }

    auto& getInstanceIdFuture = static_cast<firebase::Future<std::string>&>(future);
    if(getInstanceIdFuture.result()) {
        m_instanceId = QString::fromStdString(*getInstanceIdFuture.result());
    } else {
        m_instanceId.clear();
    }

    setComplete(true);
}

void QtFirebaseGetInstanceRequest::setComplete(bool value)
{
    if(m_complete!=value)
    {
        m_complete = value;
        emit runningChanged();
        if(m_complete)
        {
            emit completed(m_errId == firebase::instance_id::kErrorNone);
        }
    }
}

void QtFirebaseGetInstanceRequest::setError(int errId, const QString &msg)
{
    m_errId = errId;
    m_errMsg = msg;
}

void QtFirebaseGetInstanceRequest::clearError()
{
    setError(firebase::instance_id::kErrorNone);
}

QString QtFirebaseGetInstanceRequest::instanceId() const
{
    return m_instanceId;
}

bool QtFirebaseGetInstanceRequest::running() const
{
    return !m_complete;
}

void QtFirebaseGetInstanceRequest::onRun()
{
    exec();
}
