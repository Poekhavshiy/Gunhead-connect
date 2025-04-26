#pragma once

#include <string>
#include "logger.h"
#include <fstream>
#include <regex>
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;  // Add this line to define json alias

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
