#!/bin/sh
#Standart shell script
NAME=securitiestrackerbot
VERSION=0.0.1-1
cd ..
echo "Уменьшаем размер файла"
strip build/$NAME

echo "Переносим в дерево сборки"
cp -va build/$NAME deb/usr/bin/

cd deb
echo "Обновляем суммы md5"
md5deep -r usr/ var/ etc/ srv/ lib/ > DEBIAN/md5sums

echo "Обновляем размер файлов пакета"
SIZE=$(du -d0 -s -BK . 2>/dev/null | awk '{print $1}'| tr -d "K")
sed -i "s/Installed-Size: [0-9]\+/Installed-Size: $SIZE/" DEBIAN/control

cd ..
dpkg-deb --build deb ${NAME}_${VERSION}_i386.deb


