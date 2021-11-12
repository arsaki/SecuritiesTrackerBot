# SecuritiesTrackerBot
Telegram bot, which sends notification when reached desired level of cryptocurrencies and shares prices.


 *         ***SecuritiesTrackerBot***
  
 * This program tracks shares and cryptocurrency quotes and send you telegram notification,
   when quotes reaches desired price levels.
   Now, you don't need wasting time in waiting.
   Just go into bot telegram channel and subscribe to tracking, like
 *          TSLA 1000
 *          BTCUSDT 100000
  
 * Program need /etc/securitytrackerbot.conf which contains Telegram API Token
   (Can be obtained from @BotFather , go to https://telegram.me/BotFather)
   It contains two tokens - main token for all, and optional control token for owner.
  
 * Also, it needs optional Tinkoff OpenAPI token to track shares prices
   (Obtained from https://www.tinkoff.ru/invest/settings/ after purchasing credit card).
 * Writed on C++11 with Qt 5.15 for Linux, i686 architecture
 * Use Qt Creator to open project file.  
