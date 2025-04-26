#pragma once
#include "logger.h"
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>

namespace QtLogger {
    // Convert QString to std::string for logging
    inline void debug(const QString& module, const QString& message) {
        log_debug(module.toStdString(), message.toStdString());
    }
    
    inline void info(const QString& module, const QString& message) {
        log_info(module.toStdString(), message.toStdString());
    }
    
    inline void warn(const QString& module, const QString& message) {
        log_warn(module.toStdString(), message.toStdString());
    }
    
    inline void error(const QString& module, const QString& message) {
        log_error(module.toStdString(), message.toStdString());
    }
    
    // Log JSON objects
    inline void debug(const QString& module, const QString& prefix, const QJsonObject& json) {
        log_debug(module.toStdString(), prefix.toStdString() + " " + 
                 QJsonDocument(json).toJson().toStdString());
    }
}