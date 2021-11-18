#include "securitiestrackerbot.h"
#include "autoreconnectedwebsocket.h"
#include "binancewebsocket.h"
#define CONNECT_TIMEOUT_MS 100


BinanceWebSocket::BinanceWebSocket(bool type)
{
    BinanceWebSocket::type = type;
    if (type == Spot)
        URL = "wss://stream.binance.com:9443/ws/";
     if (type == Perpetual)
        URL="wss://fstream.binance.com/ws/";
}

BinanceWebSocket::~BinanceWebSocket()
{
    QList<AutoReconnectedWebSocket*> socketList = propertyIdToSocket.values();
    for (auto& socket: socketList)
        socket->close();
}


void BinanceWebSocket::subscribeToProperty(const QString& propertyId)
{
    QString property = propertyId.toLower();
    if (type == Perpetual)
        property.remove("perp");
    QString typeString = ((type == Spot) ? "Spot" : "Perpetual");
    //Check if none exist
    if (!propertyIdToSocket.contains(property))
    {
        //1. Make socket
        AutoReconnectedWebSocket * newSocket = new AutoReconnectedWebSocket();
        //2. Add socket to list
        propertyIdToSocket[property] = newSocket;
        QString fullUrl = URL + property + "@aggTrade";
        newSocket->open(fullUrl);
        while (newSocket->state() != QAbstractSocket::ConnectedState)
            SecuritiesTrackerBot::delay(CONNECT_TIMEOUT_MS);
        //3. Connect socket to handler
        connect(newSocket,&AutoReconnectedWebSocket::textMessageReceived, this, &BinanceWebSocket::webSocketMessageReceived);
        SecuritiesTrackerBot::log("Ok", "BinanceWebSocket::subscribeToProperty(): Открыт сокет Binance " + typeString + " по валютной паре " + property + ".");
    }
    else
        SecuritiesTrackerBot::log("Error",  "BinanceWebSocket::subscribeToProperty(): Сокет Binance " + typeString + " по валютной паре " + property + " уже открыт." );
}

void BinanceWebSocket::unsubscribeFromProperty(const QString& propertyId)
{
    QString property = propertyId.toLower();
    if (type == Perpetual)
        property.remove("perp");
    QString typeString = ((type == Spot) ? "Spot" : "Perpetual");
    if (propertyIdToSocket.contains(property))
    {
        AutoReconnectedWebSocket * removedSocket = propertyIdToSocket[property];
        disconnect(removedSocket,&QWebSocket::textMessageReceived, this, &BinanceWebSocket::webSocketMessageReceived);
        removedSocket->close();
        propertyIdToSocket.remove(property);
        while (removedSocket->state() != QAbstractSocket::UnconnectedState)
        {
            SecuritiesTrackerBot::log("Error", "BinanceWebSocket::unsubscribeFromProperty(): websocket не закрылся, ожидание 100ms");
            SecuritiesTrackerBot::delay(CONNECT_TIMEOUT_MS);
        }
        delete removedSocket;   //Постоянно здесь крашится. Надо ждать, пока закроется.
        SecuritiesTrackerBot::log("Ok", "BinanceWebSocket::unsubscribeFromProperty():  Cокет Binance " + typeString + " по валютной паре " + property + " закрыт.");
    }
    else
        SecuritiesTrackerBot::log("Error", "BinanceWebSocket::unsubscribeFromProperty(): Попытка закрытия несуществующего  в таблице сокета Binance " +
                                                    typeString + " по валютной паре " + property + "." );

}

int BinanceWebSocket::getDisconnectionCount()
{
    int disconnectionCount = 0;
    QList<AutoReconnectedWebSocket*> socketList =  propertyIdToSocket.values();
    for(auto& socket: socketList)
        disconnectionCount += socket->getDisconnectionCount();
    return disconnectionCount;
}


void BinanceWebSocket::webSocketMessageReceived(const QString &message)
{
    QFile file;
    file.setFileName("/srv/securitiestrackerbot/binancewebsocket.log");
    if (file.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Append))
    {
        QTextStream out(&file);
        out << message << Qt::endl;
        out.flush();
        file.flush();
        file.close();
    }
    QJsonObject jsonObject = QJsonDocument::fromJson(message.toLatin1()).object();
    QString typeString = ((type == Spot) ? "Spot" : "Perpetual");

    //ERROR MESSAGE
    if (jsonObject.contains("error"))  //Сообщение об ошибке
    {
        QString errorMessage = jsonObject["error"].toObject()["msg"].toString();

        SecuritiesTrackerBot::log("Error", "BinanceWebSocket::webSocketMessageReceived(): По сокету Binance " + typeString + " получена ошибка: " + errorMessage + "." );
        return;
    }
    //RESULT MESSAGE
    if (jsonObject.contains("result"))
        return;
    //FORMAT ERROR
    if (!jsonObject.contains("s") || !jsonObject.contains("p"))  //Неправильный формат файла
    {
        SecuritiesTrackerBot::log("Error", "BinanceWebSocket::webSocketMessageReceived(): По сокету Binance " + typeString + " получен пакет неправильной структуры: " + message + "." );
        return;
    }
   QString propertyId = jsonObject["s"].toString();
   double price = jsonObject["p"].toString().toDouble();
   // WRONG VALUES
   if (propertyId == "" || qFpClassify(price) == FP_ZERO)
   {
        SecuritiesTrackerBot::log("Error", "BinanceWebSocket::webSocketMessageReceived(): По сокету Binance " + typeString + " получен пакет с некорректными значениями: " + message + "." );
    }
   //OK
   else
   {
      if (type == Spot)
           emit priceUpdated(propertyId, price);
      else if (type == Perpetual)
           emit priceUpdated(propertyId + "PERP", price);
   }
}




