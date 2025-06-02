#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <fstream>
#include <regex>
#include "logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;  // Add this line to define json alias

// Define the Rule structure to hold regex patterns and fields
struct Rule {
    std::string identifier;
    std::regex pattern;
    std::vector<nlohmann::json> fields;
};

// Main parser functions
std::string parse_log_line(const std::string& line);
std::string load_regex_rules(const std::string& path);

// Sub-transform functions
std::string strip_prefix(const std::string& input, const std::vector<std::string>& prefixes);
std::string replace_character(const std::string& input, char from, char to);
std::string remove_trailing_numbers(const std::string& input);
std::string remove_underscores(const std::string& input);

// Clean-up functions
std::string clean_vehicle_name(const std::string& vehicle);
std::string clean_actor_name(const std::string& name);
std::string clean_weapon_name(const std::string& weapon);
std::string clean_zone_name(const std::string& zone);
std::string clean_damage_type(const std::string& damageType);
std::string clean_destroy_cause(const std::string& cause);
std::string clean_kill_mechanism(const std::string& mechanism);

// Utility functions
long long parseTimestampToUnixTime(const std::string& isoTimestamp);
bool is_npc_name(const std::string& name);

// Process transforms from JSON configuration
std::string process_transforms(const std::string& input, const json& steps);
std::pair<std::string, std::string> detectLastGameMode(const std::string& logFilePath);
std::string get_json_value(const std::string& jsonString, const std::string& key);
json get_transform_steps(const std::string& transformName);
std::tuple<std::string, std::string, std::string, std::string> detectGameModeAndPlayerFast(const std::string& logFilePath, int maxLines = 5000);

// Change the return type to std::string
std::string getGameModePattern(const std::string& identifier);
