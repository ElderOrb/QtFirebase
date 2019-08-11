#include "qtfirebasestorage.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <firebase/storage/metadata.h>
namespace store = ::firebase::storage;

QtFirebaseStorage* QtFirebaseStorage::self = 0;

QtFirebaseStorage::QtFirebaseStorage(QObject *parent) : QtFirebaseService(parent),
    m_storage(nullptr)
{
    startInit();
}

void QtFirebaseStorage::init()
{
    if(!qFirebase->ready()) {
        qDebug() << self << "::init" << "base not ready";
        return;
    }

    if(!ready() && !initializing()) {
        setInitializing(true);
        m_storage = store::Storage::GetInstance(qFirebase->firebaseApp());
        qDebug() << self << "::init" << "native initialized";
        setInitializing(false);
        setReady(true);
    }
}

void QtFirebaseStorage::onFutureEvent(QString eventId, firebase::FutureBase future)
{
    if(!eventId.startsWith(__QTFIREBASE_ID))
        return;

    qDebug() << self << "::onFutureEvent" << eventId;

    QMutexLocker locker(&m_futureMutex);
    QMap<QString, QtFirebaseStorageRequest*>::iterator it = m_requests.find(eventId);
    if(it!=m_requests.end() && it.value()!=nullptr)
    {
        QtFirebaseStorageRequest* request = it.value();
        m_requests.erase(it);
        QString id = eventId.right(eventId.size()-prefix().size());
        request->onFutureEvent(id, future);
    }
    else
    {
        qDebug() << this << "::onFutureEvent internal error, no request object found";
    }
}

QtFirebaseStorageRequest *QtFirebaseStorage::request(const QString &futureKey) const
{
    auto it = m_requests.find(futureKey);
    return it!=m_requests.end() ? *it : nullptr;
}

void QtFirebaseStorage::addFuture(QString requestId, QtFirebaseStorageRequest *request, firebase::FutureBase future)
{
    QString futureKey = prefix() + requestId;
    qFirebase->addFuture(futureKey, future);
    m_requests[futureKey] = request;
}

void QtFirebaseStorage::unregisterRequest(QtFirebaseStorageRequest *request)
{
    QMutexLocker locker(&m_futureMutex);
    for(QMap<QString, QtFirebaseStorageRequest*>::iterator it = m_requests.begin();it!=m_requests.end();++it)
    {
        if(it.value() == request)
        {
            m_requests.erase(it);
            break;
        }
    }
}

QString QtFirebaseStorage::prefix() const
{
    return __QTFIREBASE_ID + QStringLiteral(".db.");
}

//================QtFirebaseDatabaseRequest===================

namespace StorageActions {
    const QString Save = QStringLiteral("save");
    const QString GetUrl = QStringLiteral("getUrl");
    const QString Delete = QStringLiteral("delete");
}

QtFirebaseStorageRequest::QtFirebaseStorageRequest():
    m_inComplexRequest(false)
    ,m_complete(true)
{
    clearError();
}

QtFirebaseStorageRequest::~QtFirebaseStorageRequest()
{
    qFirebaseStorage->unregisterRequest(this);
}

QtFirebaseStorageRequest* QtFirebaseStorageRequest::child(const QString &path)
{
    if(!running())
    {
        if(!m_inComplexRequest)
        {
            clearError();
            m_inComplexRequest = true;
        }
        m_pushChildKey.clear();
        if(path.isEmpty())
        {
            m_storageRef = qFirebaseStorage->m_storage->GetReference();
        }
        else
        {
            m_storageRef = qFirebaseStorage->m_storage->GetReference(path.toUtf8().constData());
        }
    }
    return this;
}

void QtFirebaseStorageRequest::remove()
{
    if(m_inComplexRequest && !running())
    {
        m_inComplexRequest = false;
        setComplete(false);
        firebase::Future<void> future = m_storageRef.Delete();
        qFirebaseStorage->addFuture(StorageActions::Delete, this, future);
    }
}

void QtFirebaseStorageRequest::setValue(const QByteArray &value)
{
    if(m_inComplexRequest && !running())
    {
        m_inComplexRequest = false;
        setComplete(false);

        firebase::Future<firebase::storage::Metadata> future = m_storageRef.PutBytes(value.constData(), value.size());
        qFirebaseStorage->addFuture(StorageActions::Save, this, future);
    }
}

void QtFirebaseStorageRequest::setValue(const QString &value)
{
    if(m_inComplexRequest && !running())
    {
        m_inComplexRequest = false;
        setComplete(false);

        firebase::Future<firebase::storage::Metadata> future = m_storageRef.PutBytes(value.constData(), value.size() * sizeof(QChar));
        qFirebaseStorage->addFuture(StorageActions::Save, this, future);
    }
}

void QtFirebaseStorageRequest::exec()
{
    if(m_inComplexRequest && !running())
    {
        m_inComplexRequest = false;
        setComplete(false);

        firebase::Future<std::string> future = m_storageRef.GetDownloadUrl();
        qFirebaseStorage->addFuture(StorageActions::GetUrl, this, future);
    }
}

int QtFirebaseStorageRequest::errorId() const
{
    return m_errId;
}

bool QtFirebaseStorageRequest::hasError() const
{
    return m_errId != QtFirebaseStorage::ErrorNone;
}

QString QtFirebaseStorageRequest::errorMsg() const
{
    return m_errMsg;
}

QString QtFirebaseStorageRequest::childKey() const
{
    return m_pushChildKey;
}

void QtFirebaseStorageRequest::onFutureEvent(QString eventId, firebase::FutureBase future)
{
    if(future.status() != firebase::kFutureStatusComplete)
    {
        qDebug() << this << "::onFutureEvent " << "ERROR: Action failed with status: " << future.status();
        setError(QtFirebaseStorage::ErrorUnknown);
    }
    else if (future.error() != firebase::storage::kErrorNone)
    {
        qDebug() << this << "::onFutureEvent Error occured in result:" << future.error() << future.error_message();
        setError(future.error(), future.error_message());
    }
    else if(eventId == StorageActions::GetUrl)
    {
        auto futureVoidResult = future.result_void();
        auto futureResult = futureVoidResult ? *((std::string*) futureVoidResult) : std::string();

        m_downloadUrl = QString::fromStdString(futureResult);
    }

    setComplete(true);
}

void QtFirebaseStorageRequest::setComplete(bool value)
{
    if(m_complete!=value)
    {
        m_complete = value;
        emit runningChanged();
        if(m_complete)
        {
            emit completed(m_errId == QtFirebaseStorage::ErrorNone);
        }
    }
}

void QtFirebaseStorageRequest::setError(int errId, const QString &msg)
{
    m_errId = errId;
    m_errMsg = msg;
}

void QtFirebaseStorageRequest::clearError()
{
    setError(QtFirebaseStorage::ErrorNone);
}

QString QtFirebaseStorageRequest::downloadUrl() const
{
    return m_downloadUrl;
}

bool QtFirebaseStorageRequest::running() const
{
    return !m_complete;
}

void QtFirebaseStorageRequest::onRun()
{
    exec();
}
