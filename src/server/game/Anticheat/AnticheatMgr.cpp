#include "AnticheatMgr.h"
#include "AnticheatData.h"
#include "MapManager.h"
#include "AccountMgr.h"

AnticheatMgr::~AnticheatMgr()
{
    m_Players.clear();
}

void AnticheatMgr::HandlePlayerLogin(Player* player)
{
    // we initialize the pos of lastMovementPosition var
    m_Players[player->GetGUIDLow()].SetPosition(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation());
}

void AnticheatMgr::HandlePlayerLogout(Player* player)
{
    // Delete not needed data from the memory
    //m_Players.erase(player->GetGUIDLow());
}

void AnticheatMgr::StartHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    if (player->isGameMaster())
        return;

    uint32 key = player->GetGUIDLow();

    if (player->isInFlight() || player->GetTransport())
    {
        m_Players[key].SetLastMovementInfo(movementInfo);
        m_Players[key].SetLastOpcode(opcode);
        return;
    }

    SpeedHackDetection(player, movementInfo);
    FlyHackDetection(player, movementInfo);
    WalkOnWaterHackDetection(player, movementInfo);
    TeleportHackDetection(player, movementInfo);
    JumpHackDetection(player, movementInfo, opcode);
    ClimbHackDetection(player, movementInfo, opcode);

    m_Players[key].SetLastMovementInfo(movementInfo);
    m_Players[key].SetLastOpcode(opcode);
}

void AnticheatMgr::BuildReport(Player* player, uint8 reportType, uint8 reportAction)
{
    uint32 key = player->GetGUIDLow();
    uint32 actualTime = getMSTime();

    if (!m_Players[key].GetTempReportsTimer(reportType))
        m_Players[key].SetTempReportsTimer(actualTime,reportType);

    if (getMSTimeDiff(m_Players[key].GetTempReportsTimer(reportType), actualTime) < 3000)
    {
        m_Players[key].SetTempReports(m_Players[key].GetTempReports(reportType) + 1, reportType);

        if (m_Players[key].GetTempReports(reportType) < 3)
            return;
    }
    else
    {
        m_Players[key].SetTempReportsTimer(actualTime, reportType);
        m_Players[key].SetTempReports(1, reportType);
        return;
    }

    std::string accName;
    switch (reportAction)
    {
        case ACTION_NOTIFY:
            break;
        /*case ACTION_KICK:
            player->GetSession()->KickPlayer();
            break;
        case ACTION_BAN:
            sAccountMgr->GetName(player->GetSession()->GetAccountId(), accName);
            sWorld->BanAccount(BAN_ACCOUNT, accName, "1d", "Anticheat violation. See Characters.log file for more information.", "Anticheat");
            break;*/
        default:
            break;
    }

    sLog->outWarden("AntiCheat: Player: %s (GUID: %u, Account: %u, Ping: %u, IP: %u) triggered AnticheatMgr report ID: %u.", 
    player->GetName(), key, player->GetSession()->GetAccountId(), player->GetSession()->GetLatency(), player->GetSession()->GetRemoteAddress().c_str(), reportType);

    sLog->outWarden("AntiCheat Detail: Player: %s, report ID: %u, moveFlags: %u, moveFlags2: %u, opcode: %u",
        player->GetName(), reportType, m_Players[key].GetLastMovementInfo().moveFlags, m_Players[key].GetLastMovementInfo().moveFlags2, m_Players[key].GetLastOpcode());
};

void AnticheatMgr::SpeedHackDetection(Player* player, MovementInfo movementInfo)
{
    uint32 key = player->GetGUIDLow();

    // We also must check the map because the movementFlag can be modified by the client
    // If we just check the flag, they could always add that flag and always skip the speed hacking detection
    // 369 == DEEPRUN TRAM
    if (m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEFLAG_ONTRANSPORT) && player->GetMapId() == 369)
        return;

    uint32 distance2D = (uint32)movementInfo.pos.GetExactDist2d(&m_Players[key].GetLastMovementInfo().pos);
    uint8 moveType = 0;

    // we need to know HOW is the player moving
    // TO-DO: Should we check the incoming movement flags?
    if (player->HasUnitMovementFlag(MOVEFLAG_SWIMMING))
        moveType = MOVE_SWIM;
    else if (player->IsFlying())
        moveType = MOVE_FLIGHT;
    else if (player->HasUnitMovementFlag(MOVEFLAG_WALK_MODE))
        moveType = MOVE_WALK;
    else
        moveType = MOVE_RUN;

    // how many yards the player can do in one sec.
    uint32 speedRate = (uint32)(player->GetSpeed(UnitMoveType(moveType)) + movementInfo.j_xyspeed);

    // how long the player took to move to here.
    uint32 timeDiff = getMSTimeDiff(m_Players[key].GetLastMovementInfo().time, movementInfo.time);

    if (!timeDiff)
        timeDiff = 1;

    // this is the distance doable by the player in 1 sec, using the time done to move to this point.
    uint32 clientSpeedRate = distance2D * 1000 / timeDiff;

    // we did the (uint32) cast to accept a margin of tolerance
    if (clientSpeedRate > speedRate)
        BuildReport(player, SPEED_HACK_REPORT, ACTION_NOTIFY);
}

void AnticheatMgr::FlyHackDetection(Player* player, MovementInfo movementInfo)
{
    uint32 key = player->GetGUIDLow();
    if (!m_Players[key].GetLastMovementInfo().HasMovementFlag(MovementFlags(MOVEFLAG_FLYING | MOVEFLAG_FLYING2)))
        return;

    if (player->HasAuraType(SPELL_AURA_FLY) ||
        player->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED) ||
        player->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED) ||
        player->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_STACKING) ||
        player->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_NOT_STACKING))
        return;

    uint8 reportType = m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEFLAG_FLYING) ? FLY_HACK_REPORT : MAELSTROM_FLY_HACK_REPORT;
    BuildReport(player, reportType, ACTION_NOTIFY);
}

void AnticheatMgr::WalkOnWaterHackDetection(Player* player, MovementInfo /* movementInfo */)
{
    uint32 key = player->GetGUIDLow();
    if (!m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEFLAG_WATERWALKING))
        return;

    // if we are a ghost we can walk on water
    if (!player->isAlive())
        return;

    if (player->HasAuraType(SPELL_AURA_FEATHER_FALL) ||
        player->HasAuraType(SPELL_AURA_SAFE_FALL) ||
        player->HasAuraType(SPELL_AURA_WATER_WALK))
        return;

    BuildReport(player, WALK_WATER_HACK_REPORT, ACTION_KICK);
}

void AnticheatMgr::TeleportHackDetection(Player* player, MovementInfo movementInfo)
{
    uint32 key = player->GetGUIDLow();
    if (!m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEFLAG_NONE))
        return;
}

void AnticheatMgr::JumpHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    uint32 key = player->GetGUIDLow();
    if (m_Players[key].GetLastOpcode() == MSG_MOVE_JUMP && opcode == MSG_MOVE_JUMP)
        BuildReport(player, JUMP_HACK_REPORT, ACTION_KICK);
}

void AnticheatMgr::TeleportPlaneHackDetection(Player* player, MovementInfo movementInfo)
{
    uint32 key = player->GetGUIDLow();

    if (m_Players[key].GetLastMovementInfo().pos.GetPositionZ() != 0 ||
        movementInfo.pos.GetPositionZ() != 0)
        return;

    if (movementInfo.HasMovementFlag(MOVEFLAG_FALLING))
        return;

    if (player->getDeathState() == DEAD_FALLING)
        return;

    float x, y, z;
    player->GetPosition(x, y, z);
    float ground_Z = player->GetMap()->GetHeight(x, y, z);
    float z_diff = fabs(ground_Z - z);

    // we are not really walking there
    if (z_diff > 1.0f)
        BuildReport(player, TELEPORTPLANE_HACK_REPORT, ACTION_KICK);
}

void AnticheatMgr::ClimbHackDetection(Player *player, MovementInfo movementInfo, uint32 opcode)
{
    uint32 key = player->GetGUIDLow();

    if (m_Players[key].GetLastOpcode() != MSG_MOVE_HEARTBEAT || 
        opcode != MSG_MOVE_HEARTBEAT)
        return;

    // in this case we don't care if they are "legal" flags, they are handled in another parts of the Anticheat Manager.
    if (player->IsInWater() ||
        player->IsFlying())
        return;

    if (movementInfo.HasMovementFlag(MOVEFLAG_FALLING))
        return;

    Position playerPos;
    player->GetPosition(&playerPos);

    float deltaZ = fabs(playerPos.GetPositionZ() - movementInfo.pos.GetPositionZ());
    float deltaXY = movementInfo.pos.GetExactDist2d(&playerPos);

    float angle = sMapMgr->NormalizeOrientation(tan(deltaZ / deltaXY));

    if (angle > 1.9f)
        BuildReport(player, CLIMB_HACK_REPORT, ACTION_KICK);
}