#ifndef AUTORECONNECTEDWEBSOCKET_H
#define AUTORECONNECTEDWEBSOCKET_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>

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
        int getOfflineTime() {return offlineTime;}
        bool wasInterrupted = false;
private:
        QString URL;
        QNetworkRequest request;
        bool connectedByURL;
        int disconnectionCount;
        QTime disconnectTime;
        int offlineTime = 0;
        QTimer reconnectTimer;
protected slots:
        void webSocketDisconnected();
        void webSocketConnected();
        void reconnectTimerTimeout();

};

#endif // AUTORECONNECTEDWEBSOCKET_H
