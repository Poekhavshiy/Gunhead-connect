#pragma once

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include "logger.h"

/**
 * FilterUtils namespace provides filtering functionality that can be used
 * across multiple components without creating dependencies between them.
 * It uses QSettings to maintain consistent filter settings application-wide.
 */
namespace FilterUtils {
    /**
     * Determines if an event should be displayed based on filter settings.
     * 
     * @param jsonPayload The JSON event data as a string
     * @return true if the event should be displayed, false if it should be filtered out
     */
    inline bool shouldDisplayEvent(const QString& jsonPayload) {
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        bool showPvP = settings.value("LogDisplay/ShowPvP", false).toBool();
        bool showPvE = settings.value("LogDisplay/ShowPvE", true).toBool();
        
        // Use your logger instead of qDebug
        log_debug("FilterUtils", "shouldDisplayEvent - Current filter settings: showPvP=" + 
                  std::to_string(showPvP) + ", showPvE=" + std::to_string(showPvE));
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonPayload.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || doc.isEmpty()) {
            log_warn("FilterUtils", "shouldDisplayEvent - Invalid JSON, can't filter: " + 
                     jsonPayload.toStdString());
            return true; // Can't filter invalid JSON, so show it
        }
        
        QJsonObject obj = doc.object();
        QString identifier = obj["identifier"].toString();
        
        if (identifier == "kill_log") {
            bool victimIsNPC = obj["victim_is_npc"].toBool();
            bool killerIsNPC = obj["killer_is_npc"].toBool();
            
            log_debug("FilterUtils", "shouldDisplayEvent - Kill log: victimIsNPC=" + 
                     std::to_string(victimIsNPC) + ", killerIsNPC=" + 
                     std::to_string(killerIsNPC));
            
            // Filter out PvP events if showPvP is false
            if (!showPvP && !victimIsNPC && !killerIsNPC) {
                log_debug("FilterUtils", "shouldDisplayEvent - Filtering out PvP event");
                return false; // Filter out PvP events (player vs player)
            }
            
            // Filter out PvE events if showPvE is false
            if (!showPvE && (victimIsNPC || killerIsNPC)) {
                log_debug("FilterUtils", "shouldDisplayEvent - Filtering out PvE event");
                return false; // Filter out PvE events
            }
        }
        
        log_debug("FilterUtils", "shouldDisplayEvent - Event will be displayed");
        return true; // Show all other events
    }

    /**
     * Applies NPC name filtering based on settings.
     * 
     * @param jsonPayload The JSON event data as a string
     * @return The modified JSON payload with NPC names replaced if needed
     */
    inline QString applyNPCNameFilter(const QString& jsonPayload) {
        QSettings settings("KillApiConnect", "KillApiConnectPlus");
        bool showNPCNames = settings.value("LogDisplay/ShowNPCNames", true).toBool();
        
        qDebug() << "FilterUtils::applyNPCNameFilter - showNPCNames=" << showNPCNames;
        
        if (showNPCNames) {
            qDebug() << "FilterUtils::applyNPCNameFilter - Showing NPC names, no changes needed";
            return jsonPayload; // No change needed if showing NPC names
        }
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonPayload.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || doc.isEmpty()) {
            qWarning() << "FilterUtils::applyNPCNameFilter - Invalid JSON, cannot modify:" << jsonPayload;
            return jsonPayload; // Can't modify invalid JSON
        }
        
        QJsonObject obj = doc.object();
        QString identifier = obj["identifier"].toString();
        
        if (identifier == "kill_log") {
            bool victimIsNPC = obj["victim_is_npc"].toBool();
            bool killerIsNPC = obj["killer_is_npc"].toBool();
            
            qDebug() << "FilterUtils::applyNPCNameFilter - Processing kill log:" 
                     << "victimIsNPC=" << victimIsNPC 
                     << "killerIsNPC=" << killerIsNPC;
            
            bool changed = false;
            
            if (victimIsNPC) {
                QString originalVictim = obj["victim"].toString();
                obj["victim"] = "NPC";
                changed = true;
                qDebug() << "FilterUtils::applyNPCNameFilter - Replaced victim name:" 
                         << originalVictim << " -> NPC";
            }
            
            if (killerIsNPC) {
                QString originalKiller = obj["killer"].toString();
                obj["killer"] = "NPC";
                changed = true;
                qDebug() << "FilterUtils::applyNPCNameFilter - Replaced killer name:" 
                         << originalKiller << " -> NPC";
            }
            
            if (changed) {
                QString modifiedJson = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                qDebug() << "FilterUtils::applyNPCNameFilter - Modified JSON:" << modifiedJson;
                return modifiedJson;
            }
        }
        
        qDebug() << "FilterUtils::applyNPCNameFilter - No changes made";
        return jsonPayload; // No changes for other event types
    }
}