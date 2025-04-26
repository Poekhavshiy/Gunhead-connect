#include "globals.h"
#include "log_parser.h"
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;

struct Rule {
    std::string identifier;
    std::regex pattern;
    json fields; // Store the fields array from the JSON file
};

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
            for (const auto& item : json_rules["rules"]) {
                try {
                    Rule rule;
                    rule.identifier = item.at("identifier").get<std::string>();
                    rule.pattern = std::regex(item.at("pattern").get<std::string>(), 
                                             std::regex::ECMAScript); // Specify regex type for consistency
                    rule.fields = item.at("fields"); // Load the fields array
                    rules.push_back(rule);
                    log_debug("log_parser", "Loaded rule: ", rule.identifier);
                } catch (const std::exception& e) {
                    log_error("log_parser", "Failed to load rule: ", e.what());
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

                        // Apply transformation dynamically
                        if (!transform.empty()) {
                            auto transformIt = transforms.find(transform);
                            if (transformIt != transforms.end()) {
                                const auto& transformData = transformIt->second;
                                if (transformData.contains("steps")) {
                                    const auto& steps = transformData["steps"];
                                    try {
                                        value = process_transforms(value, steps);
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

                        // Check if the value is a boolean string
                        if (value == "true") {
                            result[fieldName] = true;
                        } else if (value == "false") {
                            result[fieldName] = false;
                        } else {
                            result[fieldName] = value;
                        }
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

        // Special case: if there's only one step and it's "is_npc", return true/false directly
        if (steps.size() == 1 && steps[0].contains("type") && steps[0]["type"] == "is_npc") {
            bool isNpc = is_npc_name(input);
            return isNpc ? "true" : "false";
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
                    if (!is_npc_name(result)) {
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

    if (!is_npc_name(name)) {
        // If name is a player name and not an NPC name, return it as-is
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
bool is_npc_name(const std::string& name) {
    return name.find("PU_Human_Enemy_GroundCombat_NPC_") == 0
        || name.find("AIModule_Unmanned_PU_") == 0
        || name.length() > 20;
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
