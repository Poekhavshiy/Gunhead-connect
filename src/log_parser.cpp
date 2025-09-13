#include "globals.h"
#include "log_parser.h"
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;

std::vector<Rule> rules;
std::unordered_map<std::string, json> transforms; // Changed from std::string to json

std::string load_regex_rules(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        log_error("log_parser", "Failed to open rules file at: ", path);
        return "";
    }

    log_debug("log_parser", "Successfully opened rules file at: ", path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    log_debug("log_parser", "Loaded rules content: ", content.length(), " bytes");

    try {
        auto json_rules = nlohmann::json::parse(content);

        // Clear existing rules and transforms
        rules.clear();
        transforms.clear();

        // Load transforms
        if (json_rules.contains("transforms")) {
            log_debug("log_parser", "Loading transforms...");
            for (const auto& [key, value] : json_rules["transforms"].items()) {
                transforms[key] = value; // Store the entire JSON object
                log_debug("log_parser", "Loaded transform: ", key);
            }
            log_debug("log_parser", "Loaded ", transforms.size(), " transforms");
        } else {
            log_warn("log_parser", "No transforms found in rules file");
        }

        // Load rules
        if (json_rules.contains("rules")) {
            log_debug("log_parser", "Loading rules...");
            const auto& rulesArray = json_rules["rules"];
            for (size_t i = 0; i < rulesArray.size(); ++i) {
                const auto& item = rulesArray[i];
                try {
                    Rule rule;
                    rule.identifier = item.at("identifier").get<std::string>();
                    rule.pattern = std::regex(item.at("pattern").get<std::string>(), 
                                             std::regex::ECMAScript); // Specify regex type for consistency
                    rule.fields = item.at("fields"); // Load the fields array
                    rules.push_back(rule);
                    log_debug("log_parser", "Loaded rule: ", rule.identifier);
                } catch (const std::exception& e) {
                    std::string ruleId = item.contains("identifier") ? item["identifier"].get<std::string>() : "<unknown>";
                    log_error("log_parser", "Failed to load rule at index ", i + 1, " (identifier: ", ruleId, "): ", e.what());
                }
            }
            log_debug("log_parser", "Loaded ", rules.size(), " rules");
        } else {
            log_warn("log_parser", "No rules found in rules file");
        }
        
        log_debug("log_parser", "Successfully loaded and parsed rules.");
    } catch (const json::parse_error& e) {
        log_error("log_parser", "JSON parse error: ", e.what());
    } catch (const std::exception& e) {
        log_error("log_parser", "Failed to parse rules JSON: ", e.what());
    }

    return content;
}

std::string parse_log_line(const std::string& line) {
    log_debug("log_parser", "Parsing line: ", line);
    if (rules.empty()) {
        log_error("log_parser", "No regex rules loaded.");
        return "{}";
    }

    for (const auto& rule : rules) {
        log_debug("log_parser", "Applying rule: ", rule.identifier);
        std::smatch match;
        try {
            if (std::regex_search(line, match, rule.pattern)) {
                log_debug("log_parser", "Matched rule: ", rule.identifier);
                nlohmann::json result;
                result["identifier"] = rule.identifier;

                try {
                    // First pass: extract all field values (especially IDs)
                    std::unordered_map<std::string, std::string> fieldValues;
                    for (const auto& field : rule.fields) {
                        const std::string& fieldName = field["name"];
                        int groupIndex = field["group"];
                        if (groupIndex < match.size()) {
                            fieldValues[fieldName] = match[groupIndex].str();
                        }
                    }

                    // Second pass: apply transforms and build result
                    std::unordered_map<std::string, bool> npcStatus; // Track NPC status for each base field

                    for (const auto& field : rule.fields) {
                        const std::string& fieldName = field["name"];
                        int groupIndex = field["group"];
                        std::string transform = field.value("transform", "");

                        if (groupIndex >= match.size()) {
                            log_warn("log_parser", "Group index out of range for field: ", fieldName);
                            continue;
                        }

                        std::string value = match[groupIndex].str();
                        log_debug("log_parser", "Field: ", fieldName, ", Value: ", value);

                        // Only apply transforms to base fields, not *_is_npc fields
                        if (!transform.empty()) {
                            auto transformIt = transforms.find(transform);
                            if (transformIt != transforms.end()) {
                                const auto& transformData = transformIt->second;
                                if (transformData.contains("steps")) {
                                    const auto& steps = transformData["steps"];
                                    try {
                                        // For clean_actor_name, run is_npc and store result
                                        if (transform == "clean_actor_name") {
                                            std::string idFieldName;
                                            if (fieldName == "victim") idFieldName = "victim_id";
                                            else if (fieldName == "killer") idFieldName = "killer_id";
                                            else if (fieldName == "driver") idFieldName = "driver_id";
                                            else if (fieldName == "cause") idFieldName = "cause_id";

                                            std::string correspondingId;
                                            if (!idFieldName.empty()) {
                                                auto it = fieldValues.find(idFieldName);
                                                if (it != fieldValues.end()) {
                                                    correspondingId = it->second;
                                                    log_debug("log_parser", "Found corresponding ID: ", correspondingId, " for field: ", fieldName);
                                                }
                                            }
                                            bool isNpc = is_npc_name(value, correspondingId);
                                            npcStatus[fieldName] = isNpc;
                                            // Continue with the rest of the clean_actor_name steps
                                            value = process_transforms(value, steps);
                                        } else {
                                            value = process_transforms(value, steps);
                                        }
                                        log_debug("log_parser", "Transformed value: ", value);
                                    } catch (const std::exception& e) {
                                        log_error("log_parser", "Error applying transform: ", transform, ": ", e.what());
                                    }
                                } else {
                                    log_warn("log_parser", "Transform missing 'steps' array: ", transform);
                                }
                            } else {
                                log_warn("log_parser", "Unknown transform: ", transform);
                            }
                        }

                        // Store the value in the result
                        if (fieldName == "timestamp" && transform == "parseTimestampToUnixTime") {
                            try {
                                long long unixTime = std::stoll(value);
                                result[fieldName] = unixTime; // Store as a number
                            } catch (...) {
                                result[fieldName] = value; // fallback to string if conversion fails
                            }
                        } else {
                            result[fieldName] = value;
                        }
                    }

                    // After all fields, set *_is_npc fields based on npcStatus
                    if (npcStatus.find("victim") != npcStatus.end()) {
                        result["victim_is_npc"] = npcStatus["victim"];
                    }
                    if (npcStatus.find("killer") != npcStatus.end()) {
                        result["killer_is_npc"] = npcStatus["killer"];
                    }
                    if (npcStatus.find("driver") != npcStatus.end()) {
                        result["driver_is_npc"] = npcStatus["driver"];
                    }
                    if (npcStatus.find("cause") != npcStatus.end()) {
                        result["cause_is_npc"] = npcStatus["cause"];
                    }

                    std::string resultJson = result.dump();
                    log_debug("log_parser", "Result: ", resultJson);
                    return resultJson;
                } catch (const std::exception& e) {
                    log_error("log_parser", "Error parsing log line: ", e.what());
                    return "{}";
                }
            }
        } catch (const std::regex_error& e) {
            log_error("log_parser", "Regex error in rule ", rule.identifier, ": ", e.what());
        }
    }

    log_warn("log_parser", "No matching rule for log line: ", line);
    return "{}";
}

// Process transforms from JSON configuration
std::string process_transforms(const std::string& input, const json& steps) {
    std::string result = input;
    
    try {
        if (!steps.is_array()) {
            log_warn("log_parser", "Steps is not an array");
            return result;
        }

        for (size_t i = 0; i < steps.size(); ++i) {
            const auto& step = steps[i];
            
            if (!step.contains("type")) {
                log_warn("log_parser", "Transform step missing 'type'");
                continue;
            }
            
            const std::string& type = step.at("type");
            log_debug("log_parser", "Applying transform step: ", type);

            try {
                if (type == "is_npc") {
                    if (!is_npc_name(result)) { // Uses default empty ID parameter
                        // If the string is not an NPC, skip the rest of the transforms
                        log_debug("log_parser", "Not an NPC name, skipping remaining transforms");
                        break;
                    }
                } else if (type == "strip_prefix") {
                    if (step.contains("prefixes") && step["prefixes"].is_array()) {
                        const auto& prefixes = step.at("prefixes").get<std::vector<std::string>>();
                        result = strip_prefix(result, prefixes);
                    } else {
                        log_warn("log_parser", "strip_prefix transform missing 'prefixes' array");
                    }
                } else if (type == "remove_trailing_numbers") {
                    result = remove_trailing_numbers(result);
                } else if (type == "remove_underscores") {
                    result = remove_underscores(result);
                } else if (type == "replace_character") {
                    if (step.contains("from") && step.contains("to")) {
                        char from = step.at("from").get<std::string>()[0];
                        char to = step.at("to").get<std::string>()[0];
                        result = replace_character(result, from, to);
                    } else {
                        log_warn("log_parser", "replace_character transform missing 'from' or 'to'");
                    }
                } else if (type == "parseTimestampToUnixTime") {
                    try {
                        long long unixTime = parseTimestampToUnixTime(result);
                        result = std::to_string(unixTime);
                    } catch (const std::exception& e) {
                        log_error("log_parser", "Failed to parse timestamp: ", e.what());
                    }
                    continue;
                } else if (type == "map_values") {
                    if (step.contains("map") && step["map"].is_object()) {
                        const auto& map = step.at("map");
                        
                        // Check if the input string is a key in the map
                        if (map.contains(result)) {
                            result = map[result].get<std::string>();
                            log_debug("log_parser", "Mapped value to: " + result);
                        } else {
                            log_debug("log_parser", "No mapping found for: " + result);
                        }
                    } else {
                        log_warn("log_parser", "map_values transform missing 'map' object");
                    }
                } else {
                    log_warn("log_parser", "Unknown transform type: ", type);
                }
            } catch (const std::exception& e) {
                log_error("log_parser", "Error in transform step: ", type, ": ", e.what());
            }
        }
    } catch (const std::exception& e) {
        log_error("log_parser", "Error processing transforms: ", e.what());
    }

    return result;
}

// Sub-transform: Removes a specified prefix from the string
std::string strip_prefix(const std::string& input, const std::vector<std::string>& prefixes) {
    if (prefixes.empty()) {
        log_warn("log_parser", "Empty prefixes list in strip_prefix");
        return input;
    }
    
    for (const auto& prefix : prefixes) {
        if (input.find(prefix) == 0) {
            return input.substr(prefix.length());
        }
    }
    return input;
}

// Sub-transform: Replaces all occurrences of a character with another character
std::string replace_character(const std::string& input, char from, char to) {
    std::string result = input;
    std::replace(result.begin(), result.end(), from, to);
    return result;
}

// Sub-transform: Removes trailing numbers (with or without an underscore)
std::string remove_trailing_numbers(const std::string& input) {
    return std::regex_replace(input, std::regex(R"(_\d+$|\d+$)"), "");
}

// Sub-transform: Replaces all underscores with spaces
std::string remove_underscores(const std::string& input) {
    return std::regex_replace(input, std::regex("_"), " ");
}

// Transform: Cleans up actor names
std::string clean_actor_name(const std::string& name) {
    static const std::vector<std::string> prefixes = {
        "PU_Human_Enemy_GroundCombat_NPC_",
        "AIModule_Unmanned_PU_"
    };

    if (!is_npc_name(name)) { // Uses default empty ID parameter
        // If name is a player name and not an NPC name, return it as-is
        log_debug("clean_actor_name", "Name is not an NPC, returning as-is: ", name);
        return name;
    }

    std::string cleanedName = strip_prefix(name, prefixes);
    cleanedName = remove_trailing_numbers(cleanedName);
    cleanedName = remove_underscores(cleanedName); // Replace underscores with spaces
    return cleanedName;
}

// Transform: Cleans up weapon names
std::string clean_weapon_name(const std::string& weapon) {
    std::string cleanedWeapon = remove_trailing_numbers(weapon);
    cleanedWeapon = remove_underscores(cleanedWeapon); // Replace underscores with spaces
    return cleanedWeapon;
}

// Transform: Cleans up zone names
std::string clean_zone_name(const std::string& zone) {
    std::string cleanedZone = remove_trailing_numbers(zone);
    cleanedZone = remove_underscores(cleanedZone); // Replace underscores with spaces
    return cleanedZone;
}

// Transform: Cleans up damage type names
std::string clean_damage_type(const std::string& damageType) {
    std::string cleanedDamageType = remove_trailing_numbers(damageType);
    cleanedDamageType = remove_underscores(cleanedDamageType); // Replace underscores with spaces
    return cleanedDamageType;
}

// Transform: Cleans up destroy cause names
std::string clean_destroy_cause(const std::string& cause) {
    std::string cleanedCause = remove_trailing_numbers(cause); // Fixed - was using cleanedCause before initialization
    cleanedCause = remove_underscores(cleanedCause);
    return cleanedCause;
}

// Transform: Cleans up kill mechanism names
std::string clean_kill_mechanism(const std::string& mechanism) {
    std::string cleanedMechanism = remove_trailing_numbers(mechanism);
    cleanedMechanism = remove_underscores(cleanedMechanism); // Replace underscores with spaces
    return cleanedMechanism;
}

// Transform: Cleans up vehicle names
std::string clean_vehicle_name(const std::string& vehicle) {
    std::string cleanedVehicle = std::regex_replace(vehicle, std::regex(R"(_\d+$)"), "");
    cleanedVehicle = remove_underscores(cleanedVehicle); // Replace underscores with spaces
    return cleanedVehicle;
}

// Function to check if a name is an NPC name
bool is_npc_name(const std::string& name, const std::string& id) {
    log_debug("is_npc_name", "Checking if name is NPC: ", name, ", ID: ", id);
    // Quick backout: if name starts with a known NPC prefix, it's an NPC (legacy/compat)
    static const std::vector<std::string> npc_prefixes = {
        "PU_Pilots-Human-",
        "PU_Human-NineTails-",
        "PU_Human-Xenothreat-",
        "PU_Human-Populace-",
        "PU_Human-Frontier-",
        "PU_Human-Headhunter-",
        "PU_Human-Dusters-",
        "RotationSimple-",
        "Hazard_Pit",
        "Hazard_Toggleable_Effects_",
        "Shipjacker_Hangar_",
        "Shipjacker_LifeSupport_",
        "Shipjacker_HUB_",
        "Shipjacker_MeetingRoom_",
        "PU_Human_Enemy_GroundCombat_NPC_",
        "AIModule_Unmanned_PU_",
        "LoadingPlatform_ShipElevator_"
    };
    for (const auto& prefix : npc_prefixes) {
        if (name.rfind(prefix, 0) == 0) {
            log_debug("is_npc_name", "Found NPC prefix: ", prefix);
            return true;
        }
    }
    // If ID is provided, check if the last underscore-separated part matches the ID
    if (!id.empty()) {
        auto last_underscore = name.rfind('_');
        if (last_underscore != std::string::npos) {
            std::string tail = name.substr(last_underscore + 1);
            log_debug("is_npc_name", "Extracted tail: '", tail, "', ID: '", id, "'");
            
            // Check if extracted tail matches the ID
            if (tail == id) {
                log_debug("is_npc_name", "ID match found - returning true");
                return true;
            }
        }
    }
    log_debug("is_npc_name", "No NPC pattern matched - returning false");
    return false;
}

// Function to parse an ISO 8601 timestamp and convert to Unix time
long long parseTimestampToUnixTime(const std::string& isoTimestamp) {
    std::tm tm = {};
    std::istringstream ss(isoTimestamp);

    // Parse the timestamp in ISO 8601 format
    // Format is: YYYY-MM-DDTHH:MM:SS.sssZ (the Z is ignored, implying UTC)
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    if (ss.fail()) {
        throw std::runtime_error("Failed to parse timestamp");
    }

    // Convert std::tm to time_t (Unix time)
    // std::mktime interprets the tm struct as local time, so we adjust for UTC by setting tm.tm_isdst to -1.
    tm.tm_isdst = -1;  // Ensure daylight saving time is not considered
    std::time_t time = std::mktime(&tm);

    // Return the Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
    return static_cast<long long>(time);
}

// Add this implementation
json get_transform_steps(const std::string& transformName) {
    if (transforms.find(transformName) != transforms.end() && 
        transforms[transformName].contains("steps")) {
        return transforms[transformName]["steps"];
    }
    log_warn("log_parser", "Transform not found or missing 'steps': ", transformName);
    return json::array(); // Return empty array if transform not found
}

std::string get_json_value(const std::string& jsonString, const std::string& key) {
    try {
        json j = json::parse(jsonString);
        if (j.contains(key)) {
            auto value = j[key];
            if (value.is_string()) {
                return value.get<std::string>();
            } else {
                return value.dump();
            }
        }
    } catch (const std::exception& e) {
        log_error("log_parser", "JSON parsing error in get_json_value:", e.what());
    }
    return ""; // Return empty string if key not found or on error
}

// Function to get regex patterns for game mode detection
std::string getGameModePattern(const std::string& identifier) {
    // Look through the rules vector for the matching identifier
    for (const auto& rule : rules) {
        if (rule.identifier == identifier) {
            // Since std::regex doesn't have a .str() method to retrieve the pattern,
            // we need to return the original pattern based on the identifier
            if (identifier == "game_mode_pu") {
                return "\\[\\+\\] \\[CIG\\] \\{Join PU\\}";
            } else if (identifier == "game_mode_ac") {
                return "\\[EALobby\\]\\[CEALobby::SetGameModeId\\].+?Changing game mode from.+?to (EA_\\w+)";
            } else if (identifier == "player_character_info") {
                return "<\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}\\.\\d{3}Z>\\s*\\[Notice\\]\\s*<AccountLoginCharacterStatus_Character>\\s*Character:.*?geid\\s*(\\d+).*?name\\s*([\\w\\d-]+).*?STATE_CURRENT";
            }
            log_warn("log_parser", "Known identifier but no hardcoded pattern: " + identifier);
            return "";
        }
    }
    
    log_warn("log_parser", "Pattern for " + identifier + " not found in loaded rules");
    return "";
}

// Optimized implementation to detect game mode and player info efficiently
std::tuple<std::string, std::string, std::string, std::string> detectGameModeAndPlayerFast(const std::string& logFilePath, int maxLines) {
    std::string gameMode = "Unknown";
    std::string subGameMode = "";
    std::string playerName = "";
    std::string playerGEID = "";

    std::ifstream file(logFilePath);
    if (!file.is_open()) {
        log_error("log_parser", "Failed to open log file for detection:", logFilePath);
        return {gameMode, subGameMode, playerName, playerGEID};
    }

    int linesProcessed = 0;
    bool foundGameMode = false;
    bool foundPlayer = false;

    std::regex puRegex(getGameModePattern("game_mode_pu"));
    std::regex acRegex(getGameModePattern("game_mode_ac"));
    std::regex playerRegex(getGameModePattern("player_character_info"));

    std::string line;
    while (std::getline(file, line) && linesProcessed < maxLines && !(foundGameMode && foundPlayer)) {
        linesProcessed++;

        std::smatch match;

        if (!foundPlayer && std::regex_search(line, match, playerRegex)) {
            // Debug the match to see what's happening
            log_debug("log_parser", "Found potential player match with ", match.size(), " captures");
            for (size_t i = 0; i < match.size(); ++i) {
                log_debug("log_parser", "  Capture ", i, ": ", match[i].str());
            }
            
            if (match.size() >= 3) {  // Changed from 4 to 3 - need full match + 2 capture groups
                playerGEID = match[1].str();  // First capture group is GEID
                playerName = match[2].str();  // Second capture group is name
                log_debug("log_parser", "Extracted player info - Name: ", playerName, " GEID: ", playerGEID);
                foundPlayer = true;
            }
        }

        if (!foundGameMode && std::regex_search(line, match, puRegex)) {
            gameMode = "PU";
            foundGameMode = true;
        } else if (!foundGameMode && std::regex_search(line, match, acRegex)) {
            gameMode = "AC";
            subGameMode = process_transforms(match[1].str(), get_transform_steps("friendly_game_mode_name"));
            foundGameMode = true;
        }
    }

    log_info("log_parser", "Fast scan processed", linesProcessed, "lines, found game mode:", gameMode, "player:", playerName);
    return {gameMode, subGameMode, playerName, playerGEID};
}
