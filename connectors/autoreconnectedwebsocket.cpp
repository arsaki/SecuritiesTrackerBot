#include "securitiestrackerbot.h"
#include "autoreconnectedwebsocket.h"
#define RECONNECT_TIMEOUT_MS 10000


AutoReconnectedWebSocket::AutoReconnectedWebSocket() : QWebSocket()
{
    disconnectionCount = 0;
    connect(this, & QWebSocket::disconnected, this, &AutoReconnectedWebSocket::webSocketDisconnected);
}

AutoReconnectedWebSocket::~AutoReconnectedWebSocket()
{
    disconnect(this, & QWebSocket::disconnected, this, &AutoReconnectedWebSocket::webSocketDisconnected);
    QEventLoop loop;
    connect(this, &AutoReconnectedWebSocket::disconnected, &loop, &QEventLoop::quit);
    this->close();
    loop.exec();
    disconnect(this, &AutoReconnectedWebSocket::disconnected, &loop, &QEventLoop::quit);
}

void AutoReconnectedWebSocket::open(const QUrl &url)
{
    URL = url.toString();
    QWebSocket::open(URL);
}

void AutoReconnectedWebSocket::open(const QNetworkRequest &request)
{
    QWebSocket::open(request);
}

int AutoReconnectedWebSocket::getDisconnectionCount()
{
    return disconnectionCount;
}


void AutoReconnectedWebSocket::webSocketDisconnected()
{
    disconnectionCount++ ;
    while (this->state() != QAbstractSocket::ConnectedState)
    {
        open(URL);
        SecuritiesTrackerBot::delay(RECONNECT_TIMEOUT_MS);
    }
}
