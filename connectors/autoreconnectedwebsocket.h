#ifndef AUTORECONNECTEDWEBSOCKET_H
#define AUTORECONNECTEDWEBSOCKET_H

#include <QObject>
#include <QWebSocket>

class AutoReconnectedWebSocket : public QWebSocket
{
    Q_OBJECT
public:
    explicit AutoReconnectedWebSocket();
        ~AutoReconnectedWebSocket() override;
        void open(const QUrl &url);
        void open(const QNetworkRequest &request);
        void close();
        int getDisconnectionCount();
private:
        QString URL;
        QNetworkRequest request;
        bool connectedByURL;
        int disconnectionCount;
protected slots:
        void webSocketDisconnected();
};

#endif // AUTORECONNECTEDWEBSOCKET_H
