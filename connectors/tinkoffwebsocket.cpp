#include "securitiestrackerbot.h"
#include "tinkoffwebsocket.h"
#define CONNECT_TIMEOUT_MS 500
#define RECONNECT_TIMEOUT_MS 10000


TinkoffWebSocket::TinkoffWebSocket()
{
    connect(&webSocket, &QWebSocket::connected, this, &TinkoffWebSocket::webSocketConnected);
    connect(&webSocket, &QWebSocket::disconnected, this, &TinkoffWebSocket::webSocketDisconnected);
    connect(&webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TinkoffWebSocket::webSocketError);
    connect(&webSocket, &QWebSocket::textMessageReceived, this, &TinkoffWebSocket::webSocketMessageReceived);
}

TinkoffWebSocket::~TinkoffWebSocket()
{
    disconnect(&webSocket, &QWebSocket::connected, this, &TinkoffWebSocket::webSocketConnected);
    disconnect(&webSocket, &QWebSocket::disconnected, this, &TinkoffWebSocket::webSocketDisconnected);
    disconnect(&webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &TinkoffWebSocket::webSocketError);
    disconnect(&webSocket, &QWebSocket::textMessageReceived, this, &TinkoffWebSocket::webSocketMessageReceived);
    webSocket.close();
}

void TinkoffWebSocket::subscribeToProperty(const QString& propertyId)
{
    if (!propertiesList.contains(propertyId))
    {
        propertiesList.append(propertyId);
        if(webSocket.state() != QAbstractSocket::ConnectedState)
            openTinkoffWebSocket();
        QString streamingRequest = "{\"event\": \"candle:subscribe\",\"figi\": \"" + propertyId + "\",\"interval\": \"1min\"}";
        webSocket.sendTextMessage(streamingRequest);
        SecuritiesTrackerBot::log("Ok", "TinkoffWebSocket::subscribeToProperty():  " + propertyId + " подписан." );
    }
    else
        SecuritiesTrackerBot::log("Error", "TinkoffWebSocket::subscribeToProperty():  " + propertyId + " уже подписан." );
}

void TinkoffWebSocket::unsubscribeFromProperty(const QString& propertyId)
{
    if (propertiesList.contains(propertyId))
    {
        propertiesList.removeAll(propertyId);
        QString streamingRequest = "{\"event\": \"candle:unsubscribe\",\"figi\": \"" + propertyId + "\",\"interval\": \"1min\"}";
        webSocket.sendTextMessage(streamingRequest);
        SecuritiesTrackerBot::log("Ok", "TinkoffWebSocket::unsubscribeFromProperty():  " + propertyId + " отписан." );
        if (propertiesList.empty())
            webSocket.close();
    }
    else
        SecuritiesTrackerBot::log("Error", "TinkoffWebSocket::unsubscribeFromProperty():  " + propertyId +  "уже отписан." );
}



void TinkoffWebSocket::webSocketConnected()
{
    if (webSocketWasInterrupted)
    {
        int offlineTime = disconnectTime.msecsTo(QTime::currentTime());
        SecuritiesTrackerBot::log("Ok", "TinkoffWebSocket::webSocketConnected(): tinkoffWebSocket подключен. Система была offline " + QString::number((static_cast<double>(offlineTime))/1000) + " cекунд.");
    }
    else
        SecuritiesTrackerBot::log("Ok", "TinkoffWebSocket::webSocketConnected(): tinkoffWebSocket подключен.");

    for (auto& propertyId: propertiesList)
    {
            QString streamingRequest = "{\"event\": \"candle:subscribe\",\"figi\": \"" + propertyId + "\",\"interval\": \"1min\"}";
            webSocket.sendTextMessage(streamingRequest);
    }
}

void TinkoffWebSocket::webSocketDisconnected()
{
    if (propertiesList.empty())
    {
        SecuritiesTrackerBot::log("Ok", "TinkoffWebSocket::webSocketDisconnected(): tinkoffWebSocket закрыт,  code "+ QString::number(webSocket.closeCode()) + ".");
        webSocketWasInterrupted = false;
    }
    else
    {
        SecuritiesTrackerBot::log("Error", "TinkoffWebSocket::webSocketDisconnected(): tinkoffWebSocket закрыт,  code "+ QString::number(webSocket.closeCode()) + ".");
        disconnectTime = QTime::currentTime();
        webSocketWasInterrupted = true;
        tinkoffDisconnectCount++;
        if (webSocket.state() != QAbstractSocket::ConnectedState)
            openTinkoffWebSocket();
    }
}

void TinkoffWebSocket::webSocketError()
{
    SecuritiesTrackerBot::log("Error", "TinkoffWebSocket::webSocketError(): Ошибка tinkoffWebSocket: " + webSocket.errorString());
}

void TinkoffWebSocket::webSocketMessageReceived(const QString &message)
{
    //Writing all network data to log
    QFile file;
    file.setFileName("/srv/securitiestrackerbot/tinkoffwebsocket.log");
    if (file.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Append))
    {
        QTextStream out(&file);
        out << message << Qt::endl;
        out.flush();
        file.flush();
        file.close();
    }
    //Input JSON parsing
    QJsonObject jsonObject = QJsonDocument::fromJson(message.toLatin1()).object();
    QString figi;
    double price;
    //If wrong format
    if (!jsonObject.contains("event"))
    {
        SecuritiesTrackerBot::log ("Error", "TinkoffWebSocket::webSocketMessageReceived(): Получен некорректный ответ из Tinkoff: " + message + ".");
        return;
    }
    //If error message
    else if (jsonObject["event"].toString() == "error")
    {
        QString errorString = jsonObject["payload"].toObject()["error"].toString();
        SecuritiesTrackerBot::log ("Error", "TinkoffWebSocket::webSocketMessageReceived(): Получена ошибка из Tinkoff: " + errorString + ".");
        return;
    }
    //If candle
    else if (jsonObject["event"].toString() == "candle")
    {
        figi = jsonObject["payload"].toObject()["figi"].toString().toUpper();
        price = jsonObject["payload"].toObject()["c"].toDouble();
        //If wrong price
        if (qFpClassify(price) == FP_ZERO)
            SecuritiesTrackerBot::log ("Error", "TinkoffWebSocket::webSocketMessageReceived(): Получена некорректная цена из Tinkoff. Price " + QString::number(price)  + ".");
        //Ok
        else
            emit  priceUpdated( figi, price);
    }
    //Not a candle
    else
        SecuritiesTrackerBot::log ("Error", "TinkoffWebSocket::webSocketMessageReceived(): Неопознанный event. " + jsonObject["event"].toString()  + ".");
}

void TinkoffWebSocket::openTinkoffWebSocket()
{
    QUrl urlWebSocket = QUrl::fromUserInput(URL);
    QNetworkRequest networkWebSocketRequest(urlWebSocket);
    networkWebSocketRequest.setRawHeader("Authorization", tinkoffToken.toLatin1());
    int attempts = 1;
    while (webSocket.state() != QAbstractSocket::ConnectedState)
    {
        webSocket.open(networkWebSocketRequest);
        SecuritiesTrackerBot::delay(CONNECT_TIMEOUT_MS);
        if (webSocket.state() != QAbstractSocket::ConnectedState)
        {
            SecuritiesTrackerBot::log("Error", "TinkoffWebSocket::openTinkoffWebSocket(): tinkoffWebSocket подключить сразу не удалось. Сейчас сокет в состоянии " + QString::number(webSocket.state())
                                                        + ", попытка переподключения через " + QString::number(RECONNECT_TIMEOUT_MS) + " секунд.");
            SecuritiesTrackerBot::delay(RECONNECT_TIMEOUT_MS);
            attempts ++;
        }
    }
    SecuritiesTrackerBot::log("Ok", "TinkoffWebSocket::openTinkoffWebSocket(): tinkoffWebSocket подключен с " + QString::number(attempts) + " попытки.");
}

