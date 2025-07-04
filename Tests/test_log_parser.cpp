#include <catch2/catch_all.hpp>
#include "log_parser.h"  // Your log parsing function
#include "logger.h"
#include <fstream>
#include <sstream>



TEST_CASE("Load and test rule: kill_log", "[log_parser]") {
    bool ISDEBUG = true;
    init_logger(ISDEBUG, ISDEBUG, "/test_data");
    log_debug("log_parser", "Debug mode enabled for tests.");
    log_info("log_parser", "Starting test for kill_log rule.");
    std::string path = "data/logfile_regex_rules.json";
    std::string raw = load_regex_rules(path);
    INFO("Loaded regex rules: " << raw);

    std::string line = "<2025-04-18T12:22:16.453Z> [Notice] <Actor Death> CActor::Kill: 'PU_Human_Enemy_GroundCombat_NPC_pyro_outlaw_soldier_2779078238568' [2779078238568] in zone 'pyro4' killed by 'Hawklord' [201924351750] using 'P6-LR_Sniper_Rifle' [Class BEHR P6_LR_Sniper_Rifle] with damage type 'Bullet' from direction x: -0.205918, y: -0.660191, z: -0.722320 [Team_ActorTech][Actor]";
    auto result = parse_log_line(line);
    INFO("Parsed result: " << result);

    REQUIRE(result.find("kill_log") != std::string::npos);
    log_debug("log_parser", "Parsed result: ", result);
}

TEST_CASE("Handle unmatched line gracefully", "[log_parser]") {
    log_info("log_parser", "Starting test for unmatched line.");
    std::string path = "data/logfile_regex_rules.json";
    std::string raw = load_regex_rules(path);
    CAPTURE(raw);

    std::string line = "Completely irrelevant system log entry";
    auto result = parse_log_line(line);
    CAPTURE(result);

    REQUIRE(result == "{}");
    log_debug("log_parser", "Parsed result: ", result);
}

TEST_CASE("Parse all log lines from game.log", "[log_parser]") {
    log_info("log_parser", "Starting test for parsing all log lines from game.log.");
    // Path to the sample game.log file
    std::string logFilePath = "tests/game.log";

    // Open the log file
    std::ifstream logFile(logFilePath);
    REQUIRE(logFile.is_open()); // Ensure the file is opened successfully

    std::string line;
    size_t lineNumber = 0;

    // Iterate through each line in the log file
    while (std::getline(logFile, line)) {
        lineNumber++;

        // Parse the log line
        auto result = parse_log_line(line);

        // Log the parsed result for debugging
        INFO("Line " << lineNumber << ": " << line);
        INFO("Parsed result: " << result);

        // Validate the result
        if (result != "{}") {
            // If the result is not empty, ensure it contains a valid identifier
            nlohmann::json parsedJson = nlohmann::json::parse(result);
            REQUIRE(parsedJson.contains("identifier"));
            REQUIRE(!parsedJson["identifier"].get<std::string>().empty());
        } else {
            // If no match, ensure the result is an empty JSON object
            REQUIRE(result == "{}");
        }
    }

    logFile.close();
    log_info("log_parser", "Finished parsing all log lines from game.log.");
    log_debug("log_parser", "Total lines processed: ", lineNumber);
}

TEST_CASE("Enhanced is_npc_name function with ID checking", "[log_parser]") {
    log_info("log_parser", "Starting test for enhanced is_npc_name function.");
    
    SECTION("NPC names with matching IDs") {
        // Test PU_Human_Enemy_GroundCombat_NPC pattern with ID at end
        REQUIRE(is_npc_name("PU_Human_Enemy_GroundCombat_NPC_Contestedzones_grunt_4761609868390", "4761609868390"));
        REQUIRE(is_npc_name("PU_Human_Enemy_GroundCombat_NPC_Contestedzones_grunt_4761609868393", "4761609868393"));
        REQUIRE(is_npc_name("PU_Human_Enemy_GroundCombat_NPC_Contestedzones_juggernaut_4761609865538", "4761609865538"));
        REQUIRE(is_npc_name("Kopion_4141066440373", "4141066440373"));
        REQUIRE(is_npc_name("Kopion_4141066445916", "4141066445916"));
        REQUIRE(is_npc_name("Kopion_4220719238291", "4220719238291"));
        REQUIRE(is_npc_name("Kopion_4762818481788", "4762818481788"));
    }
    
    SECTION("Player names should not be detected as NPCs even with ID") {
        // Regular player names with IDs should not be detected as NPCs
        REQUIRE_FALSE(is_npc_name("RapidUnscheduledDisassembly", "1234567890"));
        REQUIRE_FALSE(is_npc_name("SomePlayer", "9876543210"));
        REQUIRE_FALSE(is_npc_name("Hawklord", "201924351750"));
    }
    
    SECTION("NPC names with known prefixes but wrong IDs should fall back to pattern matching, where we cannot prefix match and have no valid ID assume player name") {
        // These should still be detected as NPCs due to pattern matching
        REQUIRE(is_npc_name("PU_Human_Enemy_GroundCombat_NPC_Contestedzones_grunt_4761609868390", "wrong_id"));
        REQUIRE_FALSE(is_npc_name("Kopion_4141066440373", "wrong_id"));
        
        // But regular names with wrong IDs should not be NPCs
        REQUIRE_FALSE(is_npc_name("RegularPlayer", "wrong_id"));
    }
    
    SECTION("Fallback to pattern-based detection without ID") {
        // Test original pattern-based detection when no ID is provided
        REQUIRE(is_npc_name("PU_Human_Enemy_GroundCombat_NPC_Something"));
        REQUIRE(is_npc_name("AIModule_Unmanned_PU_SomeThing"));
        
        // Regular names should not be detected as NPCs
        REQUIRE_FALSE(is_npc_name("RegularPlayer"));
        REQUIRE_FALSE(is_npc_name("ShortName"));
    }
    
    log_info("log_parser", "Enhanced is_npc_name function tests completed.");
}