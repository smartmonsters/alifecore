// Copyright (C) 2015-2016 Crypto Realities Ltd

//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "amount.h"
#include "consensus/params.h"
#include "game/common.h"
#include "uint256.h"
#include "serialize.h"

#include <univalue.h>

#include <cmath>
#include <map>
#include <string>

class GameState;
class Move;
class StepData;
class StepResult;

/* Return the minimum necessary amount of locked coins.  This influences
   both the minimum move game fees (for spawning a new player) and
   also the damage / HP calculation for life-steal.  */
CAmount GetNameCoinAmount (const Consensus::Params& param, unsigned nHeight);

/**
 * A character on the map that stores information while processing attacks.
 * Keep track of all attackers, so that we can both construct the killing gametx
 * and also handle life-stealing.
 */
struct AttackableCharacter
{

  /** The character this represents.  */
  CharacterID chid;

  /** The character's colour.  */
  unsigned char color;

  /**
   * Amount of coins already drawn from the attacked character's life.
   * This is the value that can be redistributed to the attackers.
   */
  CAmount drawnLife;

  /** All attackers that hit it.  */
  std::set<CharacterID> attackers;

  /**
   * Perform an attack by the given character.  Its ID and state must
   * correspond to the same attacker.
   */
  void AttackBy (const CharacterID& attackChid, const PlayerState& pl);

  /**
   * Handle self-effect of destruct.  The game state's height is used
   * to determine whether or not this has an effect (before the life-steal
   * fork).
   */
  void AttackSelf (const GameState& state);

};

/**
 * Hold the map from tiles to attackable characters.  This is built lazily
 * when attacks are done, so that we can save the processing time if not.
 */
struct CharactersOnTiles
{

  /** The map type used.  */
  typedef std::multimap<Coord, AttackableCharacter> Map;

  /** The actual map.  */
  Map tiles;

  /** Whether it is already built.  */
  bool built;

  /**
   * Construct an empty object.
   */
  inline CharactersOnTiles ()
    : tiles(), built(false)
  {}

  /**
   * Build it from the game state if not yet built.
   * @param state The game state from which to extract characters.
   */
  void EnsureIsBuilt (const GameState& state);

  /**
   * Perform all attacks in the moves.
   * @param state The current game state to build it if necessary.
   * @param moves All moves in the step.
   */
  void ApplyAttacks (const GameState& state, const std::vector<Move>& moves);

  /**
   * Deduct life from attached characters.  This also handles killing
   * of those with too many attackers, including pre-life-steal.
   * @param state The game state, will be modified.
   * @param result The step result object to fill in.
   */
  void DrawLife (GameState& state, StepResult& result);

  /**
   * Remove mutual attacks from the attacker arrays.
   * @param state The state to look up players.
   */
  void DefendMutualAttacks (const GameState& state);

  /**
   * Give drawn life back to attackers.  If there are more attackers than
   * available coins, distribute randomly.
   * @param rnd The RNG to use.
   * @param state The state to update.
   */
  void DistributeDrawnLife (RandomGenerator& rnd, GameState& state) const;

};

// Do not use for user-provided coordinates, as abs can overflow on INT_MIN.
// Use for algorithmically-computed coordinates that guaranteedly lie within the game map.
inline unsigned distLInf(const Coord &c1, const Coord &c2)
{
    return std::max(std::abs(c1.x - c2.x), std::abs(c1.y - c2.y));
}

struct LootInfo
{
    CAmount nAmount;
    // Time span over the which this loot accumulated
    // This is merely for informative purposes, plus to make
    // hash of the loot tx unique
    int firstBlock, lastBlock;

    LootInfo() : nAmount(0), firstBlock(-1), lastBlock(-1) { }
    LootInfo(CAmount nAmount_, int nHeight)
      : nAmount(nAmount_), firstBlock(nHeight), lastBlock(nHeight)
    {}

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
      inline void SerializationOp (Stream& s, Operation ser_action,
                                   int nType, int nVersion)
    {
      READWRITE (nAmount);
      READWRITE (firstBlock);
      READWRITE (lastBlock);
    }
};

struct CollectedLootInfo : public LootInfo
{
    /* Time span over which the loot was collected.  If this is a
       player refund bounty, collectedFirstBlock = -1 and collectedLastBlock
       is set to the refunding block height.  */
    int collectedFirstBlock, collectedLastBlock;
    
    CollectedLootInfo() : LootInfo(), collectedFirstBlock(-1), collectedLastBlock(-1) { }

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
      inline void SerializationOp (Stream& s, Operation ser_action,
                                   int nType, int nVersion)
    {
      READWRITE (*static_cast<LootInfo*> (this));
      READWRITE (collectedFirstBlock);
      READWRITE (collectedLastBlock);
      assert (!IsRefund ());
    }

    void Collect(const LootInfo &loot, int nHeight)
    {
        assert (!IsRefund ());

        if (loot.nAmount <= 0)
            return;

        nAmount += loot.nAmount;

        if (firstBlock < 0 || loot.firstBlock < firstBlock)
            firstBlock = loot.firstBlock;
        if (loot.lastBlock > lastBlock)
            lastBlock = loot.lastBlock;

        if (collectedFirstBlock < 0)
            collectedFirstBlock = nHeight;
        collectedLastBlock = nHeight;
    }

    /* Set the loot info to a state that means "this is a player refunding tx".
       They are used to give back coins if a player is killed for staying in
       the spawn area, and encoded differently in the game transactions.
       The block height is present to make the resulting tx unique.  */
    inline void
    SetRefund (CAmount refundAmount, int nHeight)
    {
      assert (nAmount == 0);
      assert (collectedFirstBlock == -1 && collectedLastBlock == -1);
      nAmount = refundAmount;
      collectedLastBlock = nHeight;
    }

    /* Check if this is a player refund tx.  */
    inline bool
    IsRefund () const
    {
      return (nAmount > 0 && collectedFirstBlock == -1);
    }

    /* When this is a refund, return the refund block height.  */
    inline int
    GetRefundHeight () const
    {
      assert (IsRefund ());
      return collectedLastBlock;
    }
};

// for FORK_TIMESAVE
constexpr int CHARACTER_MODE_NORMAL = 6;
// difference of 2 means we can walk over (and along) the player spawn strip without logout
constexpr int CHARACTER_MODE_LOGOUT = 8;
constexpr int CHARACTER_MODE_SPECTATOR_BEGIN = 9;
inline bool CharacterIsProtected(int s)
{
    return ((s < CHARACTER_MODE_NORMAL) || (s > CHARACTER_MODE_LOGOUT));
}
inline bool CharacterSpawnProtectionAlmostFinished(int s)
{
    return (s == CHARACTER_MODE_NORMAL - 1);
}
inline bool CharacterInSpectatorMode(int s)
{
    return (s > CHARACTER_MODE_LOGOUT);
}
inline bool CharacterNoLogout(int s)
{
    return ((s != CHARACTER_MODE_LOGOUT) && (s < CHARACTER_MODE_SPECTATOR_BEGIN + 15));
}

struct CharacterState
{
    Coord coord;                        // Current coordinate
    unsigned char dir;                  // Direction of last move (for nice sprite orientation). Encoding: as on numeric keypad.
    Coord from;                         // Straight-line pathfinding for current waypoint
    WaypointVector waypoints;           // Waypoints (stored in reverse so removal of the first waypoint is fast)
    CollectedLootInfo loot;             // Loot collected by player but not banked yet
    unsigned char stay_in_spawn_area;   // Auto-kill players who stay in the spawn area too long

    // SMC basic conversion -- part 10: extended character state
    unsigned char ai_npc_role;
    unsigned char ai_reason;
    unsigned char rpg_slot_armor;
    unsigned char rpg_slot_spell;
    unsigned char rpg_slot_cooldown;
    unsigned char ai_slot_amulet;
    unsigned char ai_slot_ring;
    unsigned char ai_poi;
    unsigned char ai_fav_harvest_poi;
    unsigned char ai_queued_harvest_poi;
    unsigned char ai_duty_harvest_poi;
    unsigned char ai_marked_harvest_poi; // for mark+recall spell
    unsigned char ai_state;
    unsigned char ai_state2;
    unsigned char ai_state3;
    unsigned char ai_chat;
    unsigned char ai_idle_time;
    unsigned char ai_mapitem_count;
    unsigned char ai_foe_count;
    unsigned char ai_foe_dist;
    //
    unsigned char ai_retreat;
    int rpg_survival_points;
    int rpg_rations;
    int rpg_range_for_display;
    int ai_recall_timer;
    int ai_regen_timer;
    int ai_order_time;
    //
    int64_t aux_age_active; // time spent on a dlevel that isn't frozen (could be int instead of int64)
    int64_t ai_reserve64_2;
    int64_t aux_storage_s1;
    int64_t aux_storage_s2;
    uint64_t aux_storage_u1;
    uint64_t aux_storage_u2;
    //
    int aux_spawn_block;
    int aux_last_sale_block;
    int aux_stasis_block;
    int aux_gather_block;
    // reserve
    unsigned char ch_reserve_uc1;
    unsigned char ch_reserve_uc2;
    unsigned char ch_reserve_uc3;
    unsigned char ch_reserve_uc4;
    unsigned char ch_reserve_uc5;
    int64_t ch_reserve_ll1;
    int64_t ch_reserve_ll2;
    int64_t ch_reserve_ll3;
    int64_t ch_reserve_ll4;
    int64_t ch_reserve_ll5;
    int ch_reserve1;
    int ch_reserve2;
    int ch_reserve3;
    int ch_reserve4;
    int ch_reserve5;

    CharacterState ()
      : coord(0, 0), dir(0), from(0, 0),
        stay_in_spawn_area(0)
      // SMC basic conversion -- part 10b: extended character state
      , ai_npc_role(0),
      ai_reason(0),
      rpg_slot_armor(0),
      rpg_slot_spell(0),
      rpg_slot_cooldown(0),
      ai_slot_amulet(0),
      ai_slot_ring(0),
      ai_poi(0),
      ai_fav_harvest_poi(0),
      ai_queued_harvest_poi(0),
      ai_duty_harvest_poi(0),
      ai_marked_harvest_poi(0),
      ai_state(0),
      ai_state2(0),
      ai_state3(0),
      ai_chat(0),
      ai_idle_time(0),
      ai_mapitem_count(0),
      ai_foe_count(0),
      ai_foe_dist(0),
      //
      ai_retreat(0),
      rpg_survival_points(0),
      rpg_rations(0),
      rpg_range_for_display(0),
      ai_recall_timer(0),
      ai_regen_timer(0),
      ai_order_time(0),
      //
      aux_age_active(0),
      ai_reserve64_2(0),
      aux_storage_s1(0),
      aux_storage_s2(0),
      aux_storage_u1(0),
      aux_storage_u2(0),
      //
      aux_spawn_block(0),
      aux_last_sale_block(0),
      aux_stasis_block(0),
      aux_gather_block(0),
      // reserve
      ch_reserve_uc1(0),
      ch_reserve_uc2(0),
      ch_reserve_uc3(0),
      ch_reserve_uc4(0),
      ch_reserve_uc5(0),
      ch_reserve_ll1(0),
      ch_reserve_ll2(0),
      ch_reserve_ll3(0),
      ch_reserve_ll4(0),
      ch_reserve_ll5(0),
      ch_reserve1(0),
      ch_reserve2(0),
      ch_reserve3(0),
      ch_reserve4(0),
      ch_reserve5(0)
    {}

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
      inline void SerializationOp (Stream& s, Operation ser_action,
                                   int nType, int nVersion)
    {
      READWRITE (coord);
      READWRITE (dir);
      READWRITE (from);
      READWRITE (waypoints);
      READWRITE (loot);
      READWRITE (stay_in_spawn_area);
      // SMC basic conversion -- part 10c: extended character state
      READWRITE(ai_npc_role);
      READWRITE(ai_reason);
      READWRITE(rpg_slot_armor);
      READWRITE(rpg_slot_spell);
      READWRITE(rpg_slot_cooldown);
      READWRITE(ai_slot_amulet);
      READWRITE(ai_slot_ring);
      READWRITE(ai_poi);
      READWRITE(ai_fav_harvest_poi);
      READWRITE(ai_queued_harvest_poi);
      READWRITE(ai_duty_harvest_poi);
      READWRITE(ai_marked_harvest_poi);
      READWRITE(ai_state);
      READWRITE(ai_state2);
      READWRITE(ai_state3);
      READWRITE(ai_chat);
      READWRITE(ai_idle_time);
      READWRITE(ai_mapitem_count);
      READWRITE(ai_foe_count);
      READWRITE(ai_foe_dist);
      //
      READWRITE(ai_retreat);
      READWRITE(rpg_survival_points);
      READWRITE(rpg_rations);
      READWRITE(rpg_range_for_display);
      READWRITE(ai_recall_timer);
      READWRITE(ai_regen_timer);
      READWRITE(ai_order_time);
      //
      READWRITE(aux_age_active);
      READWRITE(ai_reserve64_2);
      READWRITE(aux_storage_s1);
      READWRITE(aux_storage_s2);
      READWRITE(aux_storage_u1);
      READWRITE(aux_storage_u2);
      //
      READWRITE(aux_spawn_block);
      READWRITE(aux_last_sale_block);
      READWRITE(aux_stasis_block);
      READWRITE(aux_gather_block);
      // reserve
      READWRITE(ch_reserve_uc1);
      READWRITE(ch_reserve_uc2);
      READWRITE(ch_reserve_uc3);
      READWRITE(ch_reserve_uc4);
      READWRITE(ch_reserve_uc5);
      READWRITE(ch_reserve_ll1);
      READWRITE(ch_reserve_ll2);
      READWRITE(ch_reserve_ll3);
      READWRITE(ch_reserve_ll4);
      READWRITE(ch_reserve_ll5);
      READWRITE(ch_reserve1);
      READWRITE(ch_reserve2);
      READWRITE(ch_reserve3);
      READWRITE(ch_reserve4);
      READWRITE(ch_reserve5);
    }

    void Spawn(const GameState& state, int color, RandomGenerator &rnd);

    void StopMoving()
    {
        from = coord;
        waypoints.clear();
    }

    // SMC basic conversion -- part 11: extended version of MoveTowardsWaypoint
    void MoveTowardsWaypointX_Merchants(RandomGenerator &rnd, int color_of_moving_char, int out_height);
    void MoveTowardsWaypointX_Learn_From_WP(int out_height);
    void MoveTowardsWaypointX_Pathfinder(RandomGenerator &rnd, int color_of_moving_char, int out_height);

    void MoveTowardsWaypoint();
    WaypointVector DumpPath(const WaypointVector *alternative_waypoints = NULL) const;

    /**
     * Calculate total length (in the same L-infinity sense that gives the
     * actual movement time) of the outstanding path.
     * @param altWP Optionally provide alternative waypoints (for queued moves).
     * @return Time necessary to finish current path in blocks.
     */
    unsigned TimeToDestination(const WaypointVector *altWP = NULL) const;

    /* Collect loot by this character.  This takes the carrying capacity
       into account and only collects until this limit is reached.  All
       loot amount that *remains* will be returned.  */
    CAmount CollectLoot (LootInfo newLoot, int nHeight, CAmount carryCap);

    UniValue ToJsonValue(bool has_crown) const;
};

struct PlayerState
{
    /* Colour represents player team.  */
    unsigned char color;

    /* Value locked in the general's name on the blockchain.  This is the
       initial cost plus all "game fees" paid in the mean time.  It is compared
       to the new output value given by a move tx in order to compute
       the game fee as difference.  In that sense, it is a "cache" for
       the prevout.  */
    CAmount lockedCoins;
    /* Actual value of the general in the game state.  */
    CAmount value;

    std::map<int, CharacterState> characters;   // Characters owned by the player (0 is the main character)
    int next_character_index;                   // Index of the next spawned character

    /* Number of blocks the player still lives if poisoned.  If it is 1,
       the player will be killed during the next game step.  -1 means
       that there is no poisoning yet.  It should never be 0.  */
    int remainingLife;

    std::string message;      // Last message, can be shown as speech bubble
    int message_block;        // Block number. Game visualizer can hide messages that are too old
    std::string address;      // Address for receiving rewards. Empty means receive to the name address
    std::string addressLock;  // "Admin" address for player - reward address field can only be changed, if player is transferred to addressLock

    // SMC basic conversion -- part 12: bounties and voting
    std::string msg_token;
    std::string msg_vote;
    int msg_vote_block;
    std::string msg_request;
    int msg_request_block;
    std::string msg_fee;
    std::string msg_comment;
    int64_t coins_vote;
    int64_t coins_request;
    int64_t coins_fee;
    // Dungeon levels, or reserved for tokens
    std::string gw_name;
    std::string msg_dlevel;
    int msg_dlevel_block;
    std::string gw_addr_other;
    int64_t gw_amount_coins;
    int64_t gw_amount_other;
    int64_t gw_amount_auto;
    // alphatest -- reserved for high level player input
    std::string msg_area;
    int msg_area_block;
    std::string msg_merchant;
    int msg_merchant_block;
    // reserve
    std::string pl_reserve_s1;
    std::string pl_reserve_s2;
    std::string pl_reserve_s3;
    std::string pl_reserve_s4;
    std::string pl_reserve_s5;
    std::string pl_reserve_s6;
    std::string pl_reserve_s7;
    std::string pl_reserve_s8;
    std::string pl_reserve_s9;
    int64_t pl_reserve_ll1;
    int64_t pl_reserve_ll2;
    int64_t pl_reserve_ll3;
    int64_t pl_reserve_ll4;
    int64_t pl_reserve_ll5;
    int64_t pl_reserve_ll6;
    int64_t pl_reserve_ll7;
    int64_t pl_reserve_ll8;
    int64_t pl_reserve_ll9;
    int dlevel;
    int pl_reserve2;
    int pl_reserve3;
    int pl_reserve4;
    int pl_reserve5;
    int pl_reserve6;
    int pl_reserve7;
    int pl_reserve8;
    int pl_reserve9;

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
      inline void SerializationOp (Stream& s, Operation ser_action,
                                   int nType, int nVersion)
    {
      READWRITE (color);
      READWRITE (characters);
      READWRITE (next_character_index);
      READWRITE (remainingLife);

      READWRITE (message);
      READWRITE (message_block);
      READWRITE (address);
      READWRITE (addressLock);


      // SMC basic conversion -- part 12b: bounties and voting
      READWRITE(msg_token);
      READWRITE(msg_vote);
      READWRITE(msg_vote_block);
      READWRITE(msg_request);
      READWRITE(msg_request_block);
      READWRITE(msg_fee);
      READWRITE(msg_comment);
      READWRITE(coins_vote);
      READWRITE(coins_request);
      READWRITE(coins_fee);
      // Dungeon levels, or reserved for tokens
      READWRITE(gw_name);
      READWRITE(msg_dlevel);
      READWRITE(msg_dlevel_block);
      READWRITE(gw_addr_other);
      READWRITE(gw_amount_coins);
      READWRITE(gw_amount_other);
      READWRITE(gw_amount_auto);
      // alphatest -- reserved for high level player input
      READWRITE(msg_area);
      READWRITE(msg_area_block);
      READWRITE(msg_merchant);
      READWRITE(msg_merchant_block);
      // reserve
      READWRITE(pl_reserve_s1);
      READWRITE(pl_reserve_s2);
      READWRITE(pl_reserve_s3);
      READWRITE(pl_reserve_s4);
      READWRITE(pl_reserve_s5);
      READWRITE(pl_reserve_s6);
      READWRITE(pl_reserve_s7);
      READWRITE(pl_reserve_s8);
      READWRITE(pl_reserve_s9);
      READWRITE(pl_reserve_ll1);
      READWRITE(pl_reserve_ll2);
      READWRITE(pl_reserve_ll3);
      READWRITE(pl_reserve_ll4);
      READWRITE(pl_reserve_ll5);
      READWRITE(pl_reserve_ll6);
      READWRITE(pl_reserve_ll7);
      READWRITE(pl_reserve_ll8);
      READWRITE(pl_reserve_ll9);
      READWRITE(dlevel);
      READWRITE(pl_reserve2);
      READWRITE(pl_reserve3);
      READWRITE(pl_reserve4);
      READWRITE(pl_reserve5);
      READWRITE(pl_reserve6);
      READWRITE(pl_reserve7);
      READWRITE(pl_reserve8);
      READWRITE(pl_reserve9);


      READWRITE (lockedCoins);
      READWRITE (value);
    }

    PlayerState ()
      : color(0xFF), lockedCoins(0), value(-1),
        next_character_index(0), remainingLife(-1), message_block(0)

      // SMC basic conversion -- part 12c: bounties and voting
      , msg_vote_block(0), msg_request_block(0), coins_vote(0), coins_request(0), coins_fee(0)
      // Dungeon levels, or reserved for tokens
      , msg_dlevel_block(0), gw_amount_coins(0), gw_amount_other(0), gw_amount_auto(0)
      // alphatest -- reserved for high level player input
      , msg_area_block(0), msg_merchant_block(0)
      // reserve
      , pl_reserve_ll1(0), pl_reserve_ll2(0), pl_reserve_ll3(0), pl_reserve_ll4(0), pl_reserve_ll5(0)
      , pl_reserve_ll6(0), pl_reserve_ll7(0), pl_reserve_ll8(0), pl_reserve_ll9(0)
      , dlevel(0), pl_reserve2(0), pl_reserve3(0), pl_reserve4(0), pl_reserve5(0)
      , pl_reserve6(0), pl_reserve7(0), pl_reserve8(0), pl_reserve9(0)
    {}

    void SpawnCharacter(const GameState& state, RandomGenerator &rnd);
    bool CanSpawnCharacter() const;
    UniValue ToJsonValue(int crown_index, bool dead = false) const;
};

struct GameState
{
    GameState(const Consensus::Params& param);

    // Reference consensus parameters in effect.
    const Consensus::Params* param;

    // Player states
    PlayerStateMap players;

    // Last chat messages of dead players (only in the current block)
    // Minimum info is stored: color, message, message_block.
    // When converting to JSON, this array is concatenated with normal players.
    std::map<PlayerID, PlayerState> dead_players_chat;

    std::map<Coord, LootInfo> loot;
    std::set<Coord> hearts;

    /* Store banks together with their remaining life time.  */
    std::map<Coord, unsigned> banks;

    Coord crownPos;
    CharacterID crownHolder;

    /* Amount of coins in the "game fund" pool.  */
    CAmount gameFund;

    // Number of steps since the game start.
    // State with nHeight==i includes moves from i-th block
    // -1 = initial game state (before genesis block)
    // 0  = game state immediately after the genesis block
    int nHeight;

    /* Block height (as per nHeight) of the last state that had a disaster.
       I. e., for a game state where disaster has just happened,
       nHeight == nDisasterHeight.  It is -1 before the first disaster
       happens.  */
    int nDisasterHeight;

    // Hash of the last block, moves from which were included
    // into this game state. This is meta-information (i.e. used
    // mainly for managing game states rather than as part of game
    // state, though it can be used as a random seed)
    /* TODO: Can we get rid of this?  */
    uint256 hashBlock;

    // SMC basic conversion -- part 13: bounties and voting
    int64_t dao_BestFee;
    int64_t dao_BestFeeFinal; // for display only
    int64_t dao_BestRequest;
    int64_t dao_BestRequestFinal;
    std::string dao_BestName;
    std::string dao_BestNameFinal;
    std::string dao_BestComment;
    std::string dao_BestCommentFinal;
    int64_t dao_BountyPreviousWeek; // for display only
    std::string dao_NamePreviousWeek; // for display only
    std::string dao_CommentPreviousWeek;
    int64_t dao_AdjustUpkeep;
    int dao_AdjustPopulationLimit;
    int dao_MinVersion;
    // alphatest -- checkpoints
    int dcpoint_height1;
    int dcpoint_height2;
    uint256 dcpoint_hash1;
    uint256 dcpoint_hash2;
    // Dungeon levels
    int dao_DlevelMax;
    int dao_IntervalMonsterApocalypse;
    // reserve
    std::string gs_reserve_s1;
    std::string gs_reserve_s2;
    std::string gs_reserve_s3;
    std::string gs_reserve_s4;
    std::string gs_reserve_s5;
    std::string gs_reserve_s6;
    std::string gs_reserve_s7;
    std::string gs_reserve_s8;
    std::string gs_reserve_s9;
    int64_t gs_reserve_ll1;
    int64_t gs_reserve_ll2;
    int64_t gs_reserve_ll3;
    int64_t gs_reserve_ll4;
    int64_t gs_reserve_ll5;
    int64_t gs_reserve_ll6;
    int64_t gs_reserve_ll7;
    int64_t gs_reserve_ll8;
    int64_t gs_reserve_ll9;
    int gs_reserve1;
    int gs_reserve2;
    int gs_reserve3;
    int gs_reserve4;
    int gs_reserve5;
    int gs_reserve6;
    int gs_reserve7;
    int gs_reserve8;
    int gs_reserve9;

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
      inline void SerializationOp (Stream& s, Operation ser_action,
                                   int nType, int nVersion)
    {
      /* Should be only ever written to disk.  */
      assert (nType & SER_DISK);

      READWRITE (players);
      READWRITE (dead_players_chat);
      READWRITE (loot);
      READWRITE (hearts);
      READWRITE (banks);
      READWRITE (crownPos);
      READWRITE (crownHolder.player);
      if (!crownHolder.player.empty ())
        READWRITE (crownHolder.index);
      READWRITE (gameFund);

      READWRITE (nHeight);
      READWRITE (nDisasterHeight);
      READWRITE (hashBlock);

      // SMC basic conversion -- part 13b: bounties and voting
      READWRITE(dao_BestFee);
      READWRITE(dao_BestFeeFinal);
      READWRITE(dao_BestRequest);
      READWRITE(dao_BestRequestFinal);
      READWRITE(dao_BestName);
      READWRITE(dao_BestNameFinal);
      READWRITE(dao_BestComment);
      READWRITE(dao_BestCommentFinal);
      READWRITE(dao_BountyPreviousWeek);
      READWRITE(dao_NamePreviousWeek);
      READWRITE(dao_CommentPreviousWeek);
      READWRITE(dao_AdjustUpkeep);
      READWRITE(dao_AdjustPopulationLimit);
      READWRITE(dao_MinVersion);
      // alphatest -- checkpoints
      READWRITE(dcpoint_height1);
      READWRITE(dcpoint_height2);
      READWRITE(dcpoint_hash1);
      READWRITE(dcpoint_hash2);
      // Dungeon levels
      READWRITE(dao_DlevelMax);
      READWRITE(dao_IntervalMonsterApocalypse);
      // reserve
      READWRITE(gs_reserve_s1);
      READWRITE(gs_reserve_s2);
      READWRITE(gs_reserve_s3);
      READWRITE(gs_reserve_s4);
      READWRITE(gs_reserve_s5);
      READWRITE(gs_reserve_s6);
      READWRITE(gs_reserve_s7);
      READWRITE(gs_reserve_s8);
      READWRITE(gs_reserve_s9);
      READWRITE(gs_reserve_ll1);
      READWRITE(gs_reserve_ll2);
      READWRITE(gs_reserve_ll3);
      READWRITE(gs_reserve_ll4);
      READWRITE(gs_reserve_ll5);
      READWRITE(gs_reserve_ll6);
      READWRITE(gs_reserve_ll7);
      READWRITE(gs_reserve_ll8);
      READWRITE(gs_reserve_ll9);
      READWRITE(gs_reserve1);
      READWRITE(gs_reserve2);
      READWRITE(gs_reserve3);
      READWRITE(gs_reserve4);
      READWRITE(gs_reserve5);
      READWRITE(gs_reserve6);
      READWRITE(gs_reserve7);
      READWRITE(gs_reserve8);
      READWRITE(gs_reserve9);
    }

    UniValue ToJsonValue() const;

    inline bool
    ForkInEffect (Fork type) const
    {
      return param->rules->ForkInEffect (type, nHeight);
    }

    inline bool
    TestingRules () const
    {
      return param->rules->TestingRules ();
    }

    // Helper functions
    void AddLoot(Coord coord, CAmount nAmount);
    void DivideLootAmongPlayers();
    void CollectHearts(RandomGenerator &rnd);
    void UpdateCrownState(bool &respawn_crown);
    void CollectCrown(RandomGenerator &rnd, bool respawn_crown);
    void CrownBonus(CAmount nAmount);

    /**
     * Get the number of initial characters for players created in this
     * game state.  This was initially 3, and is changed in a hardfork
     * depending on the block height.
     * @return Number of initial characters to create (including general).
     */
    unsigned GetNumInitialCharacters () const;

    /**
     * Check if a given location is a banking spot.
     * @param c The coordinate to check.
     * @return True iff it is a banking spot.
     */
    bool IsBank (const Coord& c) const;

    /* Handle loot of a killed character.  Depending on the circumstances,
       it may be dropped (with or without miner tax), refunded in a bounty
       transaction or added to the game fund.  */
    void HandleKilledLoot (const PlayerID& pId, int chInd,
                           const KilledByInfo& info, StepResult& step);

    /* For a given list of killed players, kill all their characters
       and collect the tax amount.  The killed players are removed from
       the state's list of players.  */
    void FinaliseKills (StepResult& step);

    /* Check if a disaster should happen at the current state given
       the random numbers.  */
    bool CheckForDisaster (RandomGenerator& rng) const;

    /* Perform spawn deaths.  */
    void KillSpawnArea (StepResult& step);


    // SMC basic conversion -- part 14: ranged attacks
    void KillRangedAttacks (StepResult& step);
    void Pass0_CacheDataForGame ();
    void Pass1_DAO ();
    void Pass2_Melee ();
    void Pass3_PaymentAndHitscan ();
    void Pass4_Refund ();
    void PrintPlayerStats ();


    /* Apply poison disaster to the state.  */
    void ApplyDisaster (RandomGenerator& rng);
    /* Decrement poison life expectation and kill players whose has
       dropped to zero.  */
    void DecrementLife (StepResult& step);

    /* Special action at the life-steal fork height:  Remove all hearts
       on the map and kill all hearted players.  */
    void RemoveHeartedCharacters (StepResult& step);

    /* Update the banks randomly (eventually).  */
    void UpdateBanks (RandomGenerator& rng);

    /* Return total amount of coins on the map (in loot and hold by players,
       including also general values).  */
    CAmount GetCoinsOnMap () const;

};

/* Encode data for a banked bounty.  This includes also the payment address
   as per the player state (may be empty if no explicit address is set), so
   that the reward-paying game tx can be constructed even if the player
   is no longer alive (e. g., killed by a disaster).  */
struct CollectedBounty
{

  CharacterID character;
  CollectedLootInfo loot;
  std::string address;

  inline CollectedBounty (const PlayerID& p, int cInd,
                          const CollectedLootInfo& l,
                          const std::string& addr)
    : character(p, cInd), loot(l), address(addr)
  {}

  /* Look up the player in the given game state and if it is still
     there, update the address from the game state.  */
  void UpdateAddress (const GameState& state);

};

/* Encode data about why or by whom a player was killed.  Possibilities
   are a player (also self-destruct), staying too long in spawn area and
   due to poisoning after a disaster.  The information is used to
   construct the game transactions.  */
struct KilledByInfo
{

  /* Actual reason for death.  Since this is also used for ordering of
     the killed-by infos, the order here is crucial and determines
     how the killed-by info will be represented in the constructed game tx.  */
  enum Reason
  {
    KILLED_DESTRUCT = 1, /* Killed by destruct / some player.  */
    KILLED_SPAWN,        /* Staying too long in spawn area.  */
    KILLED_POISON        /* Killed by poisoning.  */
  } reason;

  /* The killing character, if killed by destruct.  */
  CharacterID killer;

  inline KilledByInfo (Reason why)
    : reason(why)
  {
    assert (why != KILLED_DESTRUCT);
  }

  inline KilledByInfo (const CharacterID& ch)
    : reason(KILLED_DESTRUCT), killer(ch)
  {}

  /* See if this killing reason pays out miner tax or not.  */
  bool HasDeathTax () const;

  /* See if this killing should drop the coins.  Otherwise (e. g., for poison)
     the coins are added to the game fund.  */
  bool DropCoins (const GameState& state, const PlayerState& victim) const;

  /* See if this killing allows a refund of the general cost to the player.
     This depends on the height, since poison death refunds only after
     the life-steal fork.  */
  bool CanRefund (const GameState& state, const PlayerState& victim) const;

  /* Comparison necessary for STL containers.  */

  friend inline bool
  operator== (const KilledByInfo& a, const KilledByInfo& b)
  {
    if (a.reason != b.reason)
      return false;

    switch (a.reason)
      {
      case KILLED_DESTRUCT:
        return a.killer == b.killer;
      default:
        return true;
      }
  }

  friend inline bool
  operator< (const KilledByInfo& a, const KilledByInfo& b)
  {
    if (a.reason != b.reason)
      return (a.reason < b.reason);

    switch (a.reason)
      {
      case KILLED_DESTRUCT:
        return a.killer < b.killer;
      default:
        return false;
      }
  }

};

class StepResult
{

private:

    // The following arrays only contain killed players
    // (i.e. the main character)
    PlayerSet killedPlayers;
    KilledByMap killedBy;

public:

    std::vector<CollectedBounty> bounties;

    CAmount nTaxAmount;

    StepResult() : nTaxAmount(0) { }

    /* Insert information about a killed player.  */
    inline void
    KillPlayer (const PlayerID& victim, const KilledByInfo& killer)
    {
      killedBy.insert (std::make_pair (victim, killer));
      killedPlayers.insert (victim);
    }

    /* Read-only access to the killed player maps.  */

    inline const PlayerSet&
    GetKilledPlayers () const
    {
      return killedPlayers;
    }

    inline const KilledByMap&
    GetKilledBy () const
    {
      return killedBy;
    }

};

// All moves happen simultaneously, so this function must work identically
// for any ordering of the moves, except non-critical cases (e.g. finding
// an empty cell to spawn new player)
bool PerformStep(const GameState &inState, const StepData &stepData, GameState &outState, StepResult &stepResult);


// SMC basic conversion -- part 15: variables declaration
#define ALTNAME_LEN_MAX 18
#define ALTNAME_ASCII_OK(C) ((C >= 32) && (C <= 126))
extern int Displaycache_devmode;
extern std::string Displaycache_devmode_npcname;

#define RPG_NUM_TEAM_COLORS 4
#define RPG_NPCROLE_MAX 103
extern int Rpg_PopulationCount[RPG_NPCROLE_MAX];
extern int64_t Rpg_WeightedPopulationCount[RPG_NPCROLE_MAX];
extern int Rpg_TotalPopulationCount;
extern int Rpg_InactivePopulationCount;
extern int Rpg_StrongestTeam;
extern int Rpg_WeakestTeam;
extern int Rpg_MonsterCount;
extern int64_t Rpg_WeightedMonsterCount;
extern bool Rpg_monsters_weaker_than_players;
extern bool Rpg_need_monsters_badly;
extern bool Rpg_hearts_spawn;
extern bool Rpg_berzerk_rules_in_effect;
extern int64_t Rpg_TeamBalanceCount[RPG_NUM_TEAM_COLORS];
extern std::string Rpg_TeamColorDesc[RPG_NUM_TEAM_COLORS];

// hunter messages
//#define ALLOW_H2H_PAYMENT
#define ALLOW_H2H_PAYMENT_NPCONLY
#define HUNTERMSG_CACHE_MAX 10000
extern std::string Huntermsg_pay_other[HUNTERMSG_CACHE_MAX];

extern int Gamecache_devmode;
extern int Gamecache_dyncheckpointheight1;
extern int Gamecache_dyncheckpointheight2;
extern uint256 Gamecache_dyncheckpointhash1;
extern uint256 Gamecache_dyncheckpointhash2;


// Dungeon levels part 2
//#define RPG_INTERVAL_MONSTERAPOCALYPSE (Gamecache_devmode == 8 ? 200 : 2000)
#define RPG_INTERVAL_MONSTERAPOCALYPSE (Cache_gameround_duration)
#define RPG_INTERVAL_ROGER_100_PERCENT (RPG_INTERVAL_MONSTERAPOCALYPSE / 2)
#define RPG_INTERVAL_TILL_AUTOMODE (RPG_INTERVAL_MONSTERAPOCALYPSE / 2)
// RPG_BLOCKS_SINCE_MONSTERAPOCALYPSE is used only in MoveTowardsWaypoint..., meaning is always "blocks since start of timeslot"
//#define RPG_BLOCKS_SINCE_MONSTERAPOCALYPSE(H) (H % RPG_INTERVAL_MONSTERAPOCALYPSE)
#define RPG_BLOCKS_SINCE_MONSTERAPOCALYPSE(H) (H % Cache_timeslot_duration)
#define RPG_BLOCKS_TILL_MONSTERAPOCALYPSE(H) (RPG_INTERVAL_MONSTERAPOCALYPSE - (H % RPG_INTERVAL_MONSTERAPOCALYPSE))
#define RPG_COMMAND_CHAMPION_REQUIRED_SP(H) ((RPG_INTERVAL_MONSTERAPOCALYPSE * 10) / (RPG_BLOCKS_SINCE_MONSTERAPOCALYPSE(H) + 1))
#define RPG_INTERVAL_BOUNTYCYCLE (Gamecache_devmode == 8 ? 1000 : 10000)
extern bool Cache_gamecache_good;
extern int Cache_timeslot_duration;
extern int Cache_timeslot_start;
extern int Cache_gameround_duration;
extern int Cache_gameround_start;
extern int nCalculatedActiveDlevel;
#define NUM_DUNGEON_LEVELS 255


extern int64_t Cache_adjusted_ration_price;
//extern int64 Cache_adjusted_population_limit;
extern int Cache_min_version;


#endif
