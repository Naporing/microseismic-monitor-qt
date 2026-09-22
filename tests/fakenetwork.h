#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QTimer>
#include <cstring>

struct FakeResponse
{
    QByteArray bytes;
    int status = 200;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QUrl redirect;
};

class FakeReply : public QNetworkReply
{
public:
    FakeReply(const QNetworkRequest &request, FakeResponse replyData, QObject *parent)
        : QNetworkReply(parent), response(std::move(replyData))
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, this->response.status);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] {
            if (isFinished())
                return;
            if (!response.redirect.isEmpty())
            {
                emit redirected(response.redirect);
                return;
            }
            if (response.error != NoError)
                setError(response.error, "模拟网络错误");
            emit readyRead();
            if (isFinished())
                return;
            emit downloadProgress(response.bytes.size(), response.bytes.size());
            setFinished(true);
            emit finished();
        });
    }

    void abort() override
    {
        if (isFinished())
            return;
        setError(OperationCanceledError, "已取消");
        setFinished(true);
        emit finished();
    }

    qint64 bytesAvailable() const override
    {
        return response.bytes.size() - offset + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 count = qMin(maxSize, response.bytes.size() - offset);
        if (count == 0)
            return -1;
        std::memcpy(data, response.bytes.constData() + offset, size_t(count));
        offset += count;
        return count;
    }

private:
    FakeResponse response;
    qint64 offset = 0;
};

class FakeNetwork : public QNetworkAccessManager
{
public:
    QQueue<FakeResponse> responses;
    QList<QNetworkRequest> requests;

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        requests.append(request);
        const auto response = responses.isEmpty()
            ? FakeResponse{{}, 503, QNetworkReply::ServiceUnavailableError, {}}
            : responses.dequeue();
        return new FakeReply(request, response, this);
    }
};
