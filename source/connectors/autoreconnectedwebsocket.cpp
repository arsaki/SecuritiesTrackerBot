#include "securitiestrackerbot.h"
#include "autoreconnectedwebsocket.h"
#define RECONNECT_TIMEOUT_MS 10000


AutoReconnectedWebSocket::AutoReconnectedWebSocket() : QWebSocket()
{
    disconnectionCount = 0;
    connect(this, & QWebSocket::disconnected, this, &AutoReconnectedWebSocket::webSocketDisconnected);
    connect(&reconnectTimer, &QTimer::timeout, this, &AutoReconnectedWebSocket::reconnectTimerTimeout);
}

AutoReconnectedWebSocket::~AutoReconnectedWebSocket()
{
    if (this->state()!=QAbstractSocket::UnconnectedState)
        close();
}

void AutoReconnectedWebSocket::open(const QUrl &url)
{
    URL = url.toString();
    connectedByURL = true;
    QWebSocket::open(URL);
    reconnectTimer.setInterval(RECONNECT_TIMEOUT_MS);
    reconnectTimer.start();
}

void AutoReconnectedWebSocket::open(const QNetworkRequest &request)
{
    AutoReconnectedWebSocket::request = request;
    connectedByURL = false;
    QWebSocket::open(request);
    reconnectTimer.setInterval(RECONNECT_TIMEOUT_MS);
    reconnectTimer.start();
}

void AutoReconnectedWebSocket::close()
{
    disconnect(this, & QWebSocket::disconnected, this, &AutoReconnectedWebSocket::webSocketDisconnected);
    QWebSocket::close();
}

int AutoReconnectedWebSocket::getDisconnectionCount()
{
    return disconnectionCount;
}


void AutoReconnectedWebSocket::webSocketDisconnected()
{
    disconnectionCount++ ;
    disconnectTime = QTime::currentTime();
    if (connectedByURL)
        open(URL);
    else
        open(request);
}

void AutoReconnectedWebSocket::webSocketConnected()
{
    if (wasInterrupted)
        offlineTime = disconnectTime.msecsTo(QTime::currentTime());
    reconnectTimer.stop();
}

/*It automatically reconnects if still not opened*/
void AutoReconnectedWebSocket::reconnectTimerTimeout()
{
    if (state()!= QAbstractSocket::ConnectedState)
    {
        if (connectedByURL)
            open(URL);
        else
            open(request);
    }
}
