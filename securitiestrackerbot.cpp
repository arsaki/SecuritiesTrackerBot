/*          ***SecuritiesTrackerBot***
 *
 * This program tracks shares and cryptocurrency quotes and send you telegram notification,
 * when quotes reaches desired price levels.
 * Now, you don't need wasting time in waiting.
 * Just go into bot telegram channel and subscribe to tracking, like
 *          TSLA 1000
 *          BTCUSDT 100000
 *
 * Program need /etc/securitytrackerbot.conf which contains Telegram API Token
 * (Can be obtained from @BotFather , go to https://telegram.me/BotFather)
 * It contain two token - main token for all, and optional control token for owner.
 *
 * Also, it needs optional Tinkoff OpenAPI token to track shares prices
 * (Obtained from https://www.tinkoff.ru/invest/settings/ after purchasing credit card).
 */
#define TELEGRAM_MESSAGE_MAX_SIZE 4096

//#include <QMessageBox>
#include <QApplication>
#include <QtWidgets/QWidget>
#include <QtCore/QDateTime>
#include <QtCore/QByteArray>
#include <QFile>
#include <QDir>
#include <QtNetwork>
#include <QtNetwork/QNetworkAccessManager>
#include <QUrl>
#include <QTimer>
#include <QTime>

#include "securitiestrackerbot.h"

bool SecuritiesTrackerBot::loggingEnabled = true;


SecuritiesTrackerBot::SecuritiesTrackerBot(): spotBinanceWebSocket(Spot),   perpetualBinanceWebSocket(Perpetual)
{    
    log("START", "--------------------------------------------------------");
    log("Ok", "SecuritiesTrackerBot(): Сервис стартует.");
    if (loadSettings("/etc/securitiestrackerbot.conf"))
    {
        std::cout<<"Ошибка загрузки файла конфигурации securitiestrackerbot.conf"<<std::endl;
        log("Error", "SecuritiesTrackerBot(): securitiestrackerbot.conf не найден.");
    }
    if (telegramMainToken == "")
    {
        std::cout<<"Работа невозможна. Токен Telegram не задан. Укажите работающий токен Telegram!"<<std::endl;
        log("Error", "SecuritiesTrackerBot(): Токен Telegram не задан. Программа завершается.");
        QApplication::exit(1);
    }
    StartTime = QDateTime::currentDateTime();

    connect(&tinkoffWebSocket, &TinkoffWebSocket::priceUpdated, this, &SecuritiesTrackerBot::webSocketMessageReceived);
    connect(&spotBinanceWebSocket, &BinanceWebSocket::priceUpdated, this, &SecuritiesTrackerBot::webSocketMessageReceived);
    connect(&perpetualBinanceWebSocket, &BinanceWebSocket::priceUpdated, this, &SecuritiesTrackerBot::webSocketMessageReceived);
    connect(&propertiesListRefreshTimer, &QTimer::timeout, this, &SecuritiesTrackerBot::downloadPropertiesList);
    propertiesListRefreshTimer.start(60*60*1000); //Every hour

    mainBot = new TelegramBot(telegramMainToken);
    controlBot = new TelegramBot(telegramControlToken);
    downloadPropertiesList();       //Sets all data
    tinkoffWebSocket.setToken(tinkoffToken);
    int loadedSubscriptions = loadUsersSubscriptionsFromFiles();      //Subscribes by all channels
    connect(mainBot, &TelegramBot::newMessage, this, &SecuritiesTrackerBot::mainBotMessage);
    mainBot->startMessagePulling();
    //При совпадении токенов возникает конфликт
    if (telegramMainToken != telegramControlToken)
    {
        connect(controlBot, &TelegramBot::newMessage, this, &SecuritiesTrackerBot::controlBotMessage);
        controlBot->startMessagePulling();
    }
    controlBot->sendMessage(telegramMasterId, "SecuritiesTrackerBot(): Сервис запущен, загружено подписок: " + QString::number(loadedSubscriptions) + ".");
    log("Ok", "SecuritiesTrackerBot(): сервис запущен.");
}

SecuritiesTrackerBot::~SecuritiesTrackerBot()
{
    disconnect(&tinkoffWebSocket, &TinkoffWebSocket::priceUpdated, this, &SecuritiesTrackerBot::webSocketMessageReceived);
    disconnect(&spotBinanceWebSocket, &BinanceWebSocket::priceUpdated, this, &SecuritiesTrackerBot::webSocketMessageReceived);
    disconnect(&perpetualBinanceWebSocket, &BinanceWebSocket::priceUpdated, this, &SecuritiesTrackerBot::webSocketMessageReceived);
    disconnect(&propertiesListRefreshTimer, &QTimer::timeout, this, &SecuritiesTrackerBot::downloadPropertiesList);
    log("Ok", "~SecuritiesTrackerBot(): сервис остановлен.");
}

void SecuritiesTrackerBot::log(QString type, QString message)
{
    if (SecuritiesTrackerBot::loggingEnabled)
    {
        QFile file;
        file.setFileName("/srv/securitiestrackerbot/securitiestrackerbot.log");
        file.open(QIODevice::Text | QIODevice::Append | QIODevice::WriteOnly);
        QTextStream textStream(&file);
        QString dateTime = QDateTime::currentDateTime().toString(Qt::ISODate);
        textStream<<dateTime + " [" + type + "]: " + message + "\r\n";
        file.close();
    }
}

void SecuritiesTrackerBot::delay(int msec)
{
    QTimer timer;
    timer.start(msec);
    QEventLoop loop;
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();
}

QJsonDocument SecuritiesTrackerBot::getJSONfromURL(QString URL, QString authHeader, QString authToken)
{
    QUrl url = QUrl::fromUserInput(URL);
    QNetworkRequest  request(url);
    request.setRawHeader("Accept","application/json");
    if (authHeader != "" && authToken != "")
        request.setRawHeader(authHeader.toLatin1(), authToken.toLatin1());
    QNetworkAccessManager networkaccess;
    QNetworkReply * reply = networkaccess.get(request);
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    QByteArray buffer;
    buffer.append(reply->readAll());
    delete reply;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(buffer);
    return jsonDocument;
}

int SecuritiesTrackerBot::downloadTinkoffProperties(QString type)
{
    QJsonArray propertiesArray = getJSONfromURL("https://api-invest.tinkoff.ru/openapi/sandbox/market/" + type, \
                                                                                    "Authorization", tinkoffToken ).object()["payload"].toObject()["instruments"].toArray();
    int counter;
    for (counter = 0; counter < propertiesArray.size(); counter ++)
    {
        PropertyDescriptor propertyDescriptor;
        propertyDescriptor.id = propertiesArray[counter].toObject()["figi"].toString();
        propertyDescriptor.name = propertiesArray[counter].toObject()["name"].toString();
        propertyDescriptor.type = propertiesArray[counter].toObject()["type"].toString();
        propertyDescriptor.currency = propertiesArray[counter].toObject()["currency"].toString();
        QString ticker = propertiesArray[counter].toObject()["ticker"].toString();
        figiToTicker[propertyDescriptor.id] = ticker;
        propertiesList.insert(ticker, propertyDescriptor);
    }
    if (counter == 0)
        log("Error", "downloadTinkoffProperties(): загружено " + type + ": " +QString::number(counter));
    else
        log("Ok", "downloadTinkoffProperties(): успешно загружено " + type + ": " +QString::number(counter));
    return counter;
}

int SecuritiesTrackerBot::downloadBinanceProperties(bool type = Spot)
{
    QJsonArray propertiesArray;
    if (type == Spot)
        propertiesArray = getJSONfromURL("https://api.binance.com/api/v3/exchangeInfo").object()["symbols"].toArray();
    else if (type == Perpetual)
        propertiesArray = getJSONfromURL("https://testnet.binancefuture.com/fapi/v1/exchangeInfo").object()["symbols"].toArray();
    int counter;
    for (counter = 0; counter < propertiesArray.size(); counter ++)
    {
        PropertyDescriptor propertyDescriptor;
        if (type == Spot)
        {
            propertyDescriptor.type = "Spot";
            propertyDescriptor.id = propertiesArray[counter].toObject()["symbol"].toString();
        }
        else if (type == Perpetual)
        {
            propertyDescriptor.type = "Perpetual";
            propertyDescriptor.id = propertiesArray[counter].toObject()["symbol"].toString() + "PERP";
        }
        propertyDescriptor.name = propertyDescriptor.id;
        QString ticker = propertyDescriptor.id;
        propertyDescriptor.currency = propertiesArray[counter].toObject()["quoteAsset"].toString();
        figiToTicker[ticker] = ticker;
        propertiesList.insert(ticker, propertyDescriptor);
    }
    QString item;
    if (type == Spot)
        item = "криптовалютных пар";
    else if (type == Perpetual)
        item = "криптофьючерсов";
    if (counter == 0)
        log("Error", "downloadBinanceProperties(): загружено " + item + ": " + QString::number(counter));
    else
        log("Ok", "downloadBinanceProperties(): загружено " + item + ": " + QString::number(counter));
    return counter;
}

int SecuritiesTrackerBot::loadSettings(QString filename = "/etc/securitiestrackerbot.conf")
{
    QFile file;
    file.setFileName(filename);
    int result = 0;
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
            QTextStream in(&file);
            while(!in.atEnd())
            {
                    QStringList parseLine = in.readLine().split(QLatin1Char(' '), Qt::SkipEmptyParts);
                     if (parseLine.size() == 2)
                     {
                             if (parseLine[0] == "TinkoffToken")
                                tinkoffToken = "Bearer " + parseLine[1];
                             if (parseLine[0] == "TelegramMainToken")
                                telegramMainToken = parseLine[1];
                             if (parseLine[0] == "TelegramControlToken")
                                telegramControlToken = parseLine[1];
                             if (parseLine[0] == "TelegramMasterId")
                                telegramMasterId = parseLine[1].toInt();
                     }
            }
            file.close();
    }
    else
        result = 1;
    if (!tinkoffToken.size())
    {
        std::cout<<"TinkoffToken doesn`t defined!"<< std::endl;
        log("Error", "loadSettings(): не задан TinkoffToken");
        result = 1;
    }
    if (!telegramMainToken.size())
    {
        std::cout<<"TelegramMainToken doesn`t defined!"<< std::endl;
        log("Error", "loadSettings(): не задан TelegramMainToken");
        result = 1;
    }
    if (!telegramControlToken.size())
    {
        std::cout<<"TelegramControlToken doesn`t defined!"<< std::endl;
        log("Error", "loadSettings(): не задан TelegramControlToken");

        result = 1;
    }
    if (!telegramMasterId)
    {
        std::cout<<"TelegramOwnerId doesn`t defined!"<< std::endl;
        log("Error", "loadSettings(): не задан TelegramOwnerId");
        result = 1;
    }
    if (result == 1)
        log("Error", "loadSettings(): файл конфигурации некорректен ");
    return result ;
}

int SecuritiesTrackerBot::saveUserToFile(qint32 id)
{
    QDir dir;
    if (!dir.exists("/srv/securitiestrackerbot/usersdata"))
        dir.mkdir("/srv/securitiestrackerbot/usersdata");
    dir.cd("/srv/securitiestrackerbot/usersdata");
    QFile file;
    file.setFileName(dir.filePath(QString::number(id)));
    if (file.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Truncate))
    {
        QTextStream out(&file);
        QList<UserSubscription>::Iterator userOrderIterator, end;
        end = usersMap[id].end();
        for (userOrderIterator = usersMap[id].begin(); userOrderIterator != end; userOrderIterator ++)
        {
            out<<userOrderIterator->figi<<" ";
            out<<userOrderIterator->price<<" ";
            out<<userOrderIterator->upwards<<"\r\n";
        }
        file.close();
        if (file.size() == 0)
            file.remove();
        return 0;
    }
    else
        return 1;
}

int SecuritiesTrackerBot::loadUsersSubscriptionsFromFiles()
{
    QDir dir;
    if (!dir.exists("/srv/securitiestrackerbot/usersdata"))
        dir.mkdir("/srv/securitiestrackerbot/usersdata");
    if (!dir.cd("/srv/securitiestrackerbot/usersdata"))
        return 0;
    QFile file;
    QList<QString>::Iterator filenameIterator, filenameEnd;
    QStringList fileList = dir.entryList();
    filenameEnd = fileList.end();
    int subscribesCount = 0;
    for (filenameIterator = fileList.begin(); filenameIterator !=filenameEnd; filenameIterator++)
    {
        file.setFileName(dir.filePath(*filenameIterator));
        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
                QTextStream in(&file);
                Subscription subscription;
                subscription.userId = filenameIterator->toInt();
                while(!in.atEnd())
                {
                        QStringList parseLine = in.readLine().split(QLatin1Char(' '), Qt::SkipEmptyParts);
                        QString id = parseLine[0];        //Can be figi either cryptocurrency pair name
                        subscription.price  = parseLine[1].toDouble();
                        subscription.upwards = parseLine[2].toInt();
                        addSubscription(id, subscription);
                        subscribesCount++;
                }
                file.close();
        }
    }
    log("Ok", "loadUsersSubscriptionsFromFiles(): загружено из файлов " + QString::number(subscribesCount) + " подписок.");
    return subscribesCount;
}

QString SecuritiesTrackerBot::findTicker(QString searchkey)
{

    QString answer;
    if (searchkey.size() < 3)
    {
        answer = "Слишком короткий запрос.";
        return answer;
    }
    int finded = 0;
    QMap<QString,PropertyDescriptor>::iterator iterator, end;
    end = propertiesList.end();
    for (iterator = propertiesList.begin(); iterator!=end; iterator ++)
    {
        if (iterator->name.contains(searchkey, Qt::CaseInsensitive))
            {
            if (iterator->type == "Spot")
                 answer += "Криптопара \"" + iterator.key() + "\".\r\n";
            else if (iterator->type == "Perpetual")
                 answer += "Фьючерс на криптопару \"" + iterator.key() + "\".\r\n";
            else
            {
                if (iterator->type == "Stock")
                    answer += "Акция ";
                if (iterator->type == "Bond")
                    answer += "Облигация ";
                if (iterator->type == "Etf")
                    answer += "ETF  ";
                if (iterator->type == "Currency")
                    answer += "Валютная пара ";
                answer += "\"" + iterator->name + "\", тикер \"" + iterator.key() + "\".\r\n";
            }
                finded ++;
            }
    }

    if (!finded)
        answer =  "Такой команды, ценной бумаги или криптовалюты не найдено.";
    else
        answer += " Найдено " + QString::number(finded) + ".";
    return answer;

}

double  SecuritiesTrackerBot::getTinkoffPropertyCurrentPrice(QString figi)
{
    return getJSONfromURL("https://api-invest.tinkoff.ru/openapi/sandbox/market/orderbook?figi="+figi+"&depth=1", \
                          "Authorization", tinkoffToken ).object()["payload"].toObject()["lastPrice"].toDouble();
}

double SecuritiesTrackerBot::getBinanceSpotCurrentPrice(QString cryptoPair)
{
    return getJSONfromURL("https://api.binance.com/api/v3/ticker/price?symbol=" + cryptoPair).object()["price"].toString().toDouble() ;
}

double SecuritiesTrackerBot::getBinancePerpetualCurrentPrice(QString cryptoPair)
{
    cryptoPair.remove("PERP");
    return getJSONfromURL("https://testnet.binancefuture.com/fapi/v1/ticker/price?symbol=" + cryptoPair).object()["price"].toString().toDouble() ;
}

void SecuritiesTrackerBot::addSubscription(QString figi, Subscription subscription)
{
    //If we don`t track this figi, subscribe to websocket
    if(!subscriptionsMap.contains(figi))
    {
        subscriptionsMap[figi].ticker = figiToTicker[figi];
        subscriptionsMap[figi].currency = propertiesList[subscriptionsMap[figi].ticker].currency;
        log("Ok", "SecuritiesTrackerBot::addSubscription(): подписка на " + figiToTicker[figi] + ".");
        if (propertiesList[subscriptionsMap[figi].ticker].type == "Spot")
            spotBinanceWebSocket.subscribeToProperty(figi);
        else if (propertiesList[subscriptionsMap[figi].ticker].type == "Perpetual")
            perpetualBinanceWebSocket.subscribeToProperty(figi);
        else
            tinkoffWebSocket.subscribeToProperty(figi);
    }
    // Добавление условия в таблицу отслеживания.
    PropertySubscriptionsList * subscribeList = &(subscriptionsMap[figi]);
    QList<Subscription> * list = &subscribeList->list;
    list->append(subscription);
    //Добавление в таблицу пользователей.
    UserSubscription userSubscription;
    userSubscription.ticker = subscribeList->ticker;
    userSubscription.figi = figi;
    userSubscription.price = subscription.price;
    userSubscription.upwards =  subscription.upwards;
    usersMap[subscription.userId].append(userSubscription);
}

//Returns one or all user subscriptions
QString SecuritiesTrackerBot::getSubscriptions(qint32 id, QString figi = "")
{
    QString result;
    QList<UserSubscription>::Iterator orderIterator, orderEnd;
    QList<UserSubscription> * userOrderList;
    userOrderList = &usersMap[id];
    orderEnd = userOrderList->end();
    QMap<QString,QList<double>> userSubscriptions;
    if (userOrderList->size())
    for(orderIterator = userOrderList->begin(); orderIterator!=orderEnd; orderIterator ++)
    {
       userSubscriptions[figiToTicker[orderIterator->figi]].append(orderIterator->price);
    }
    QString spacer = "";
    if (figi != "" && userSubscriptions.contains(figiToTicker[figi]))
    {
           for (int i = 0; i < userSubscriptions[figiToTicker[figi]].size(); i++)
           {
                 result += spacer + QString::number(userSubscriptions[figiToTicker[figi]][i]);
                 spacer = ", ";
           }
    }
    else
        if (figi != "") return "";
    else
            if(userSubscriptions.size())
            {
                QMap<QString,QList<double>>::Iterator iterator, end;
                end = userSubscriptions.end();
                for(iterator = userSubscriptions.begin(); iterator != end; iterator ++)
                {//Тикер
                        result += iterator.key() + ": ";
                        spacer = "";
                        for (int i = 0; i < userSubscriptions[iterator.key()].size(); i++)
                        {//Цены в 1 тикере
                              result += spacer + QString::number(userSubscriptions[iterator.key()][i]);
                              spacer = ", ";
                        }
                        result +=" " + propertiesList[iterator.key()].currency + ". \r\n";
                 }
            }
    return result;
}

void SecuritiesTrackerBot::disableSubscriptions(qint32 userId, QString figi = "")
{
    QList<Subscription> * subscribeList = &subscriptionsMap[figi].list;
    QList<Subscription>:: iterator iterator, end;
    end = subscribeList->end();
    int size = 0, disabled = 0;
    for (iterator = subscribeList->begin(); iterator != end; ++iterator)
    {
        if (userId == iterator->userId)
            iterator->userId = 0;
        if (iterator->userId == 0)
            disabled++;
        size++;
    }
    if (4*disabled >  size)
        cleanSubscriptions(figi);
    if (figi == "")
        usersMap.remove(userId);
    else
        {
            int i;
            QList<UserSubscription> * userSubscriptionsList;
            userSubscriptionsList = &usersMap[userId];
            for (i = 0; i != userSubscriptionsList->size(); i ++)
            {
                if ((* userSubscriptionsList)[i].figi == figi)
                {
                    userSubscriptionsList->removeAt(i);
                    i--;
                }
            }
        }
    saveUserToFile(userId);
}

void SecuritiesTrackerBot::cleanSubscriptions(QString figi)
{
    PropertySubscriptionsList * subscriptionsListOld = &subscriptionsMap[figi];
    PropertySubscriptionsList * subscriptionsListNew = new PropertySubscriptionsList;
    subscriptionsListNew->ticker = subscriptionsListOld->ticker;
    subscriptionsListNew->currency = subscriptionsListOld->currency;
    int subscriptionsListNewSize = 0;
    QList<Subscription>:: iterator iterator, iterator_end;
    iterator_end = subscriptionsListOld->list.end();
    for (iterator = subscriptionsListOld->list.begin(); iterator != iterator_end; ++iterator)
    {
        if (iterator->userId != 0)
        {
            subscriptionsListNew->list.append(*iterator);
            subscriptionsListNewSize++;
        }
    }
    subscriptionsMap.remove(figi);
    if (subscriptionsListNewSize == 0)
    {
        if (propertiesList[figiToTicker[figi]].type == "Spot")
            spotBinanceWebSocket.unsubscribeFromProperty(figi);
        else if (propertiesList[figiToTicker[figi]].type == "Perpetual")
            perpetualBinanceWebSocket.unsubscribeFromProperty(figi);
        else
            tinkoffWebSocket.unsubscribeFromProperty(figi);
        log("Ok", "SecuritiesTrackerBot::cleanSubscriptions(): отписка от " + figiToTicker[figi] + ".");
    }
    else
    {
        subscriptionsMap.insert(figi, *subscriptionsListNew);
    }
}

void SecuritiesTrackerBot::webSocketMessageReceived(QString propertyId, double price)
{
    // Обход таблицы отслеживания
    PropertySubscriptionsList * subscriptionsList = &subscriptionsMap[propertyId];
    QList<Subscription>::iterator iterator, iteratorEnd;
    iteratorEnd = subscriptionsList->list.end();
    int size = 0;
    int disabled = 0;
    for (iterator = subscriptionsList->list.begin(); iterator != iteratorEnd; ++iterator)
    {
        if (iterator->userId)
        {
                if (((price >= iterator->price) && (iterator->upwards)) \
                   || ((price <= iterator->price) && (!iterator->upwards)))
                {
                        QString answer = "Цена на " + subscriptionsList->ticker + " достигла " \
                        + QString::number(iterator->price) + " " +subscriptionsList->currency + " и составляет " \
                        + QString::number(price) + " " + subscriptionsList->currency + ".";

                        // В очередь на удаление;
                        for (int orderCount = 0; orderCount < usersMap[iterator->userId].size(); orderCount++)
                        {
                                if ((usersMap[iterator->userId][orderCount].figi == propertyId)
                                        && (abs(usersMap[iterator->userId][orderCount].price - iterator->price)<0.00001))
                                {
                                        usersMap[iterator->userId].removeAt(orderCount);
                                        break;
                                }
                        }
                        saveUserToFile(iterator->userId);
                        mainBot->sendMessage(iterator->userId, answer);
                        iterator->userId = 0;
                }
         }
        else
            disabled++;
        size++;

    }
    if (4*disabled >  size)
        cleanSubscriptions(propertyId);
}

bool SecuritiesTrackerBot::isNumber(QString string)
{
    return string.contains(QRegExp("^[0-9][0-9]*[,.]?[0-9]*$"));
}

void SecuritiesTrackerBot::mainBotMessage(TelegramBotUpdate update)
{
    /*
     *
     * This function needs refactorization
     *
     *
     */
        // only handle Messages
        if(update->type != TelegramBotMessageType::Message) return;
        // simplify message access
        TelegramBotMessage& message = *update->message;

        // send message (Format: Normal)
        TelegramBotMessage msgSent;
        QString answer;
        log ("Ok", "mainBotMessage(): Пользователь " + QString::number(message.chat.id) + " запросил команду: \"" + message.text + "\".");

        // Парсер тикера и цены
        QStringList commandLine = message.text.toUpper().split(QLatin1Char(' '), Qt::SkipEmptyParts);

        if ( (commandLine.size()<2)  && (commandLine[0] == "/START" || commandLine[0] == "HELP" || commandLine[0] == "H"))
        {
            answer = "Введите тикер с ценой, чтобы подписаться на отслеживание (Например, YNDX 4567.7, TSLA 150 200 250).\r\n\
Остановка отслеживания - тикер со словом stop или 0 (YNDX STOP, TSLA 0).\r\n\
Просмотр отслеживания - просто тикер (YNDX) или знак \"?\".\r\n\
Для поиска тикера отправьте слово поиска (Yandex, Газпром).";
        }
        else if((commandLine.size()<2)  && (commandLine[0] == "?"))
        {
                answer = getSubscriptions(message.chat.id);
                if (answer == "")
                    answer = "Вы ни на что не подписаны.";
        }
        else //Это не команда
        {
            QString ticker = commandLine[0];
            // Проверка тикера и цены
            PropertyDescriptor * property = &propertiesList[ticker];
            if (property->id == "")
                answer = findTicker(ticker);
            else //тикер найден
            {
                if (commandLine.size()<2) // Задан один тикер
                {
                    answer = "Цена " + ticker + ": " ;
                    if (property->type == "Spot")
                        answer += QString::number(getBinanceSpotCurrentPrice(property->id));
                    else if(property->type == "Perpetual")
                        answer += QString::number(getBinancePerpetualCurrentPrice(property->id));
                    else                        // Regular property
                        answer +=  QString::number(getTinkoffPropertyCurrentPrice(property->id));
                    answer += " " + property->currency + ".";
                    QString listOfSubscribes = getSubscriptions(message.chat.id, property->id);
                    if (listOfSubscribes != "")
                        answer += " Уведомление на уровне " + listOfSubscribes + " " + property->currency + ".";
                }
                else  // Кроме тикера еще параметр
                {
                    QString param = commandLine[1].replace(",",".");
                    if (property->id == "")
                        answer = "Тикер " + ticker + " не найден.";
                    else
                        if (param == "STOP"  ||  param == "OFF"  ||  param == "СТОП"
                        ||  (isNumber(param) && qFpClassify(param.toDouble()) == FP_ZERO))
                        {
                            disableSubscriptions(message.chat.id, property->id);
                            answer = "Ваша подписка на тикер "+ ticker + " отключена.";
                        }
                        else            //И это - не команда а заказ на подписку
                        {
                            double lastPrice;
                            if (property->type == "Spot")
                                lastPrice = getBinanceSpotCurrentPrice(property->id);
                            else if (property->type == "Perpetual")
                                lastPrice = getBinancePerpetualCurrentPrice(property->id);
                            else
                               lastPrice = getTinkoffPropertyCurrentPrice(property->id);
                            answer = "Текущая цена " + ticker + " " + QString::number(lastPrice) + " " + property->currency + ". \r\n";
                            for (int arg = 1; arg < commandLine.size(); arg ++)
                            {
                                    param = commandLine[arg].replace(",",".");
                                    if (qFpClassify(param.toDouble()) != FP_ZERO)
                                    {
                                            double thresholdPrice = param.toDouble();
                                            // Сравнение с заданной ценой
                                            bool upwards =  (thresholdPrice > lastPrice ? true : false);
                                            Subscription subscription;
                                            subscription.price = thresholdPrice;
                                            subscription.upwards = upwards;
                                            subscription.userId = message.chat.id;
                                            addSubscription(property->id, subscription);
                                            answer += "Уведомление прийдет, когда цена ";
                                            answer += (upwards? "поднимется" : "опустится");
                                            answer += " до " + QString::number(thresholdPrice) + " " + property->currency + ".\r\n";
                                    }
                            }
                            saveUserToFile(message.chat.id);

                       }
                  }
             }
        }
        //Split large messages into 4096 char size parts
        for ( int i = 0;   i < answer.size(); i += TELEGRAM_MESSAGE_MAX_SIZE)
            mainBot->sendMessage(message.chat.id, answer.mid(i, TELEGRAM_MESSAGE_MAX_SIZE));
}

void SecuritiesTrackerBot::controlBotMessage(TelegramBotUpdate update)
{
    // only handle Messages
    if(update->type != TelegramBotMessageType::Message) return;
    // simplify message access
    TelegramBotMessage& message = *update->message;

    // send message (Format: Normal)
    if (message.chat.id == telegramMasterId && message.text == "?")
           controlBot->sendMessage(message.chat.id, " Версия 0.0.3 , запущен " + StartTime.toString(Qt::ISODate) +
                                                        ", выгружено позиций: " + QString::number(propertiesList.size()) +
                                                        ", разрывов WebSocket Tinkoff " + QString::number(tinkoffWebSocket.getDisconnectionCount()) +
                                                        ", разрывов WebSocket Binance " + QString::number(spotBinanceWebSocket.getDisconnectionCount() +
                                                                                                                                                perpetualBinanceWebSocket.getDisconnectionCount() ) + ".");
}

void SecuritiesTrackerBot::downloadPropertiesList()
{
    //ToDo: Properties removement
    //Remember old amounts
    int beforeSize = propertiesList.size();
    int regularPropertiesQty = 0, cryptoPropertiesQty = 0;
    //Downloading
    regularPropertiesQty += downloadTinkoffProperties("stocks");
    regularPropertiesQty += downloadTinkoffProperties("bonds");
    regularPropertiesQty += downloadTinkoffProperties("etfs");
    regularPropertiesQty += downloadTinkoffProperties("currencies");
    cryptoPropertiesQty += downloadBinanceProperties(Spot);
    cryptoPropertiesQty += downloadBinanceProperties(Perpetual);
    //Comparing new amounts with new ones
    int newSize = propertiesList.size();
    if (beforeSize != newSize)
        log ("Ok", "downloadPropertiesList(): список позиций обновлен, доступно бумаг: " + QString::number(regularPropertiesQty) + \
                ", криптовалютных активов: "+ QString::number(cryptoPropertiesQty) + ".");
}
