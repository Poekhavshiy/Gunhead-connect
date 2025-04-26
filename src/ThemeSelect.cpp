#include <QSettings>
#include <QFile>
#include <QFontDatabase>
#include <QApplication>
#include <QDebug>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include <QTimer>
#include <QMainWindow>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "ThemeSelect.h"

ThemeSelectWindow::ThemeSelectWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Theme Selector"));
    setObjectName("themeSelectWindow");

    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    setFixedSize(settings.value("chooseThemeWindowPreferredSize", QSize(400, 400)).toSize());

    QVector<Theme> themes = loadThemes();
    if (themes.isEmpty()) {
        qWarning() << "No themes loaded. Exiting.";
        return;
    }

    Theme current = loadCurrentTheme();
    applyTheme(current);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    mainLayout = new QVBoxLayout(central);

    header = new QLabel(tr("> THEME SELECTOR"), central);
    header->setObjectName("headerLabel");
    header->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(header);

    const QString defaultStyle = "QPushButton { padding: 8px; }";
    const QString selectedStyle = "QPushButton { padding: 8px; background-color: #00aaff; color: white; }";

    QButtonGroup* group = new QButtonGroup(this);
    for (const Theme& t : themes) {
        QPushButton* btn = new QPushButton(t.friendlyName, central);
        btn->setObjectName("themeButton");
        btn->setStyleSheet(defaultStyle);
        mainLayout->addWidget(btn);
        group->addButton(btn);

        if (t.stylePath == current.stylePath) {
            btn->setEnabled(false);
            btn->setStyleSheet(selectedStyle);
            m_currentThemeButton = btn;
            resize(t.chooseThemeWindowPreferredSize);
        }

        connect(btn, &QPushButton::clicked, this, [this, t, btn, defaultStyle, selectedStyle]() {
            if (m_currentThemeButton && m_currentThemeButton != btn) {
                m_currentThemeButton->setEnabled(true);
                m_currentThemeButton->setStyleSheet(defaultStyle);
            }

            saveTheme(t);
            applyTheme(t);
            resize(t.chooseThemeWindowPreferredSize);

            btn->setEnabled(false);
            btn->setStyleSheet(selectedStyle);
            m_currentThemeButton = btn;
            qDebug() << "Emitting selected theme:" << t.friendlyName;
            qDebug() << "==== Theme Changed ====\nTheme Window Preferred Size:" << t.chooseThemeWindowPreferredSize << "\nMain Window Preferred Size:" << t.mainWindowPreferredSize << "\nSettings Window Preferred Size:" << t.settingsWindowPreferredSize << "\nChoose Language Window Preferred Size:" << t.chooseLanguageWindowPreferredSize << "\nBackground Image:" << t.backgroundImage;
            emit themeChanged(t); // Ensure this is emitted
        });
    }
}

void ThemeSelectWindow::connectSignals() {
}

void ThemeSelectWindow::applyTheme(const Theme& theme) {
    QFile f(theme.stylePath);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        QString style = f.readAll();
        QTimer::singleShot(0, [style]() { qApp->setStyleSheet(style); });
        qDebug() << "Applied QSS:" << theme.stylePath;
    } else {
        qWarning() << "Cannot load QSS:" << theme.stylePath;
    }

    qApp->setFont(QFont());
    int id = QFontDatabase::addApplicationFont(theme.fontPath);
    if (id != -1) {
        const auto fam = QFontDatabase::applicationFontFamilies(id);
        if (!fam.isEmpty()) {
            qApp->setFont(QFont(fam.first()));
            qDebug() << "Applied font:" << fam.first();
        }
    } else {
        qWarning() << "Cannot load font:" << theme.fontPath;
    }
}

void ThemeSelectWindow::saveTheme(const Theme& t) {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    settings.setValue("currentTheme", t.stylePath);
    settings.setValue("currentFont", t.fontPath);
    settings.setValue("currentBackgroundImage", t.backgroundImage);
    settings.setValue("mainWindowPreferredSize", t.mainWindowPreferredSize);
    settings.setValue("chooseThemeWindowPreferredSize", t.chooseThemeWindowPreferredSize);
    settings.setValue("chooseLanguageWindowPreferredSize", t.chooseLanguageWindowPreferredSize);
    settings.setValue("settingsWindowPreferredSize", t.settingsWindowPreferredSize);
    settings.setValue("statusLabelPreferredSize", t.statusLabelPreferredSize);
    settings.setValue("mainButtonRightSpace", t.mainButtonRightSpace); // Add this line

    qDebug() << "Saved theme:" << t.stylePath << "with font:" << t.fontPath;
}

Theme ThemeSelectWindow::loadCurrentTheme() {
    QSettings settings("KillApiConnect", "KillApiConnectPlus");
    Theme theme;
    theme.stylePath = settings.value("currentTheme", ":/themes/originalsleek.qss").toString();
    theme.fontPath = settings.value("currentFont", ":/fonts/PressStart2P-Regular.ttf").toString();
    theme.backgroundImage = settings.value("currentBackgroundImage", "").toString();
    theme.mainWindowPreferredSize = settings.value("mainWindowPreferredSize", QSize(800, 600)).toSize();
    theme.chooseThemeWindowPreferredSize = settings.value("chooseThemeWindowPreferredSize", QSize(400, 400)).toSize();
    theme.chooseLanguageWindowPreferredSize = settings.value("chooseLanguageWindowPreferredSize", QSize(300, 300)).toSize();
    theme.settingsWindowPreferredSize = settings.value("settingsWindowPreferredSize", QSize(500, 400)).toSize();
    theme.statusLabelPreferredSize = settings.value("statusLabelPreferredSize", QSize(280, 80)).toSize();
    theme.mainButtonRightSpace = settings.value("mainButtonRightSpace", 140).toInt(); // Add this line
    return theme;
}

void ThemeSelectWindow::init(QApplication& app) {
    Theme t = loadCurrentTheme();
    applyTheme(t);
}

QVector<Theme> ThemeSelectWindow::loadThemes() {
    QVector<Theme> themes;
    QFile file(":/themes/themes.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to open themes.json";
        return themes;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        qWarning() << "Invalid JSON format for themes.";
        return themes;
    }

    for (const QJsonValue& val : doc.array()) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();
        Theme t;
        t.friendlyName = obj.value("name").toString();
        t.stylePath = obj.value("qss").toString();
        t.fontPath = obj.value("font").toString();
        t.backgroundImage = obj.value("background").toString();

        auto extractSize = [](const QJsonObject& o) -> QSize {
            return QSize(o.value("width").toInt(), o.value("height").toInt());
        };

        if (obj.contains("mainWindowPreferredSize")) t.mainWindowPreferredSize = extractSize(obj["mainWindowPreferredSize"].toObject());
        if (obj.contains("chooseThemeWindowPreferredSize")) t.chooseThemeWindowPreferredSize = extractSize(obj["chooseThemeWindowPreferredSize"].toObject());
        if (obj.contains("chooseLanguageWindowPreferredSize")) t.chooseLanguageWindowPreferredSize = extractSize(obj["chooseLanguageWindowPreferredSize"].toObject());
        if (obj.contains("settingsWindowPreferredSize")) t.settingsWindowPreferredSize = extractSize(obj["settingsWindowPreferredSize"].toObject());
        if (obj.contains("statusLabelPreferredSize")) t.statusLabelPreferredSize = extractSize(obj["statusLabelPreferredSize"].toObject());
        t.mainButtonRightSpace = obj.value("mainButtonRightSpace").toInt(140); // Default 140px

        themes.append(t);
    }

    return themes;
}
