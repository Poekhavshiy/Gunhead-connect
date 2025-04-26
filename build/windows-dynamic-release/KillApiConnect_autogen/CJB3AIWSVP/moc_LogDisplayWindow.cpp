/****************************************************************************
** Meta object code from reading C++ file 'LogDisplayWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../includes/LogDisplayWindow.h"
#include <QtGui/qtextcursor.h>
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LogDisplayWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN16LogDisplayWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto LogDisplayWindow::qt_create_metaobjectdata<qt_meta_tag_ZN16LogDisplayWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "LogDisplayWindow",
        "windowClosed",
        "",
        "toggleMonitoringRequested",
        "filterPvPChanged",
        "show",
        "filterPvEChanged",
        "filterNPCNamesChanged",
        "clearLog",
        "changeTextColor",
        "changeBackgroundColor",
        "increaseFontSize",
        "decreaseFontSize",
        "handleTestButton",
        "handleMonitoringButton",
        "onShowPvPToggled",
        "checked",
        "onShowPvEToggled",
        "onShowNPCNamesToggled",
        "processLogQueue",
        "log",
        "updateFilterSettings",
        "showPvP",
        "showPvE",
        "showNPCNames"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'windowClosed'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'toggleMonitoringRequested'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'filterPvPChanged'
        QtMocHelpers::SignalData<void(bool)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 5 },
        }}),
        // Signal 'filterPvEChanged'
        QtMocHelpers::SignalData<void(bool)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 5 },
        }}),
        // Signal 'filterNPCNamesChanged'
        QtMocHelpers::SignalData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 5 },
        }}),
        // Slot 'clearLog'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'changeTextColor'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'changeBackgroundColor'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'increaseFontSize'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'decreaseFontSize'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleTestButton'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleMonitoringButton'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onShowPvPToggled'
        QtMocHelpers::SlotData<void(bool)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 16 },
        }}),
        // Slot 'onShowPvEToggled'
        QtMocHelpers::SlotData<void(bool)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 16 },
        }}),
        // Slot 'onShowNPCNamesToggled'
        QtMocHelpers::SlotData<void(bool)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 16 },
        }}),
        // Slot 'processLogQueue'
        QtMocHelpers::SlotData<void(const QString &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 20 },
        }}),
        // Slot 'updateFilterSettings'
        QtMocHelpers::SlotData<void(bool, bool, bool)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 22 }, { QMetaType::Bool, 23 }, { QMetaType::Bool, 24 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<LogDisplayWindow, qt_meta_tag_ZN16LogDisplayWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject LogDisplayWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16LogDisplayWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16LogDisplayWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16LogDisplayWindowE_t>.metaTypes,
    nullptr
} };

void LogDisplayWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LogDisplayWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->windowClosed(); break;
        case 1: _t->toggleMonitoringRequested(); break;
        case 2: _t->filterPvPChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->filterPvEChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->filterNPCNamesChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->clearLog(); break;
        case 6: _t->changeTextColor(); break;
        case 7: _t->changeBackgroundColor(); break;
        case 8: _t->increaseFontSize(); break;
        case 9: _t->decreaseFontSize(); break;
        case 10: _t->handleTestButton(); break;
        case 11: _t->handleMonitoringButton(); break;
        case 12: _t->onShowPvPToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->onShowPvEToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 14: _t->onShowNPCNamesToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->processLogQueue((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->updateFilterSettings((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (LogDisplayWindow::*)()>(_a, &LogDisplayWindow::windowClosed, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (LogDisplayWindow::*)()>(_a, &LogDisplayWindow::toggleMonitoringRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (LogDisplayWindow::*)(bool )>(_a, &LogDisplayWindow::filterPvPChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (LogDisplayWindow::*)(bool )>(_a, &LogDisplayWindow::filterPvEChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (LogDisplayWindow::*)(bool )>(_a, &LogDisplayWindow::filterNPCNamesChanged, 4))
            return;
    }
}

const QMetaObject *LogDisplayWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LogDisplayWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16LogDisplayWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int LogDisplayWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 17;
    }
    return _id;
}

// SIGNAL 0
void LogDisplayWindow::windowClosed()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void LogDisplayWindow::toggleMonitoringRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void LogDisplayWindow::filterPvPChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void LogDisplayWindow::filterPvEChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void LogDisplayWindow::filterNPCNamesChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
