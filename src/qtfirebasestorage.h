#ifndef QTFIREBASE_STORAGE_H
#define QTFIREBASE_STORAGEE_H

#include "qtfirebaseservice.h"
#include "firebase/storage.h"
#include <QMutex>

#ifdef QTFIREBASE_BUILD_STORAGE
#include "src/qtfirebase.h"
#if defined(qFirebaseStorage)
#undef qFirebaseStorage
#endif
#define qFirebaseStorage (static_cast<QtFirebaseStorage*>(QtFirebaseStorage::instance()))

class QtFirebaseStorageRequest;
class QtFirebaseStorage : public QtFirebaseService
{
    Q_OBJECT
    typedef QSharedPointer<QtFirebaseStorage> Ptr;
public:
    static QtFirebaseStorage* instance() {
        if(self == 0) {
            self = new QtFirebaseStorage(0);
            qDebug() << self << "::instance" << "singleton";
        }
        return self;
    }

    enum Error
    {
        ErrorNone = firebase::storage::kErrorNone,
        ErrorUnknown = firebase::storage::kErrorUnknown,
        ErrorObjectNotFound = firebase::storage::kErrorObjectNotFound,
        ErrorBucketNotFound = firebase::storage::kErrorBucketNotFound,
        ErrorProjectNotFound = firebase::storage::kErrorProjectNotFound,
        ErrorQuotaExceeded = firebase::storage::kErrorQuotaExceeded,
        ErrorUnauthenticated = firebase::storage::kErrorUnauthenticated,
        ErrorUnauthorized = firebase::storage::kErrorUnauthorized,
        ErrorRetryLimitExceeded = firebase::storage::kErrorRetryLimitExceeded,
        ErrorNonMatchingChecksum = firebase::storage::kErrorNonMatchingChecksum,
        ErrorDownloadSizeExceeded = firebase::storage::kErrorDownloadSizeExceeded,
        ErrorCancelled = firebase::storage::kErrorCancelled,
    };
    Q_ENUM(Error)

private:
    explicit QtFirebaseStorage(QObject *parent = 0);
    void init() override;
    void onFutureEvent(QString eventId, firebase::FutureBase future) override;

    QString futureKey(QtFirebaseStorageRequest* request) const;
    QtFirebaseStorageRequest* request(const QString& futureKey) const;
    void addFuture(QString requestId, QtFirebaseStorageRequest* request, firebase::FutureBase future);
    void unregisterRequest(QtFirebaseStorageRequest* request);
    QString prefix() const;
private:
    static QtFirebaseStorage* self;
    Q_DISABLE_COPY(QtFirebaseStorage)

    firebase::storage::Storage* m_storage;
    QMap<QString, QtFirebaseStorageRequest*> m_requests;
    QMutex m_futureMutex;

    friend class QtFirebaseStorageRequest;
};

class QtFirebaseStorageRequest: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
public:
    QtFirebaseStorageRequest();
    ~QtFirebaseStorageRequest();
public slots:
    //Data request
    QtFirebaseStorageRequest* child(const QString& path = QString());
    QString downloadUrl() const;

    void setValue(const QByteArray& value);
    void setValue(const QString& value);
    void remove();

    void exec();

    //State
    bool running() const;
    int errorId() const;
    bool hasError() const;
    QString errorMsg() const;

    //Data access
    QString childKey() const;
public:
    void onFutureEvent(QString eventId, firebase::FutureBase future);

signals:
    void completed(bool success);
    void runningChanged();
    void snapshotChanged();
private slots:
    void onRun();
private:
    void setComplete(bool value);
    void setError(int errId, const QString& msg = QString());
    void clearError();

    QString m_downloadUrl;
    bool m_inComplexRequest;
    firebase::storage::StorageReference m_storageRef;
    QString m_action;
    bool m_complete;
    QString m_pushChildKey;
    int m_errId;
    QString m_errMsg;
};

#endif //QTFIREBASE_BUILD_DATABASE

#endif // QTFIREBASE_DATABASE_H
