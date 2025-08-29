import re

def is_npc_name(name: str, geid: str) -> bool:
    npc_prefixes = [
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
        "AIModule_Unmanned_PU_"
    ]
    for prefix in npc_prefixes:
        if name.startswith(prefix):
            return True
    if geid:
        last_underscore = name.rfind('_')
        if last_underscore != -1:
            tail = name[last_underscore + 1:]
            if tail == geid:
                return True
    return False

def count_non_npc_rows(table_path: str) -> int:
    count = 0
    with open(table_path, encoding="utf-8") as f:
        for line in f:
            if "|" not in line or line.startswith("player_name"):
                continue
            name, geid = [x.strip() for x in line.split("|")[:2]]
            if not is_npc_name(name, geid):
                count += 1
                print(f"Non-NPC entry found: {name}, {geid}")
    return count

if __name__ == "__main__":
    table_path = "npc_names.table"
    non_npc_count = count_non_npc_rows(table_path)
    print(non_npc_count)