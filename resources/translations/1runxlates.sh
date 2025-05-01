#!/bin/bash
QT_BIN=/c/Dev/Qt/qt-install-dynamic/bin
$QT_BIN/lupdate . -ts resources/translations/lang_de.ts
$QT_BIN/lupdate . -ts resources/translations/lang_en.ts
$QT_BIN/lupdate . -ts resources/translations/lang_es.ts
$QT_BIN/lupdate . -ts resources/translations/lang_fr.ts
$QT_BIN/lupdate . -ts resources/translations/lang_it.ts
$QT_BIN/lupdate . -ts resources/translations/lang_ja.ts
$QT_BIN/lupdate . -ts resources/translations/lang_ko.ts
$QT_BIN/lupdate . -ts resources/translations/lang_kr.ts
$QT_BIN/lupdate . -ts resources/translations/lang_pl.ts
$QT_BIN/lupdate . -ts resources/translations/lang_pt.ts
$QT_BIN/lupdate . -ts resources/translations/lang_ru.ts
$QT_BIN/lupdate . -ts resources/translations/lang_ua.ts
$QT_BIN/lupdate . -ts resources/translations/lang_uk.ts
$QT_BIN/lupdate . -ts resources/translations/lang_zh_CN.ts
echo "Translation files updated."