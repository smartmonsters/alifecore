// Copyright (C) 2015 Crypto Realities Ltd

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

#include "game/tx.h"

#include "base58.h"
#include "coins.h"
#include "game/state.h"
#include "names/common.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "util.h"

#include <boost/foreach.hpp>

// Opcodes for scriptSig that acts as coinbase for game-generated transactions.
// They serve merely for information purposes, so the client can know why it got this transaction.
// In the future, for some really complex transactions, this data can be encoded in scriptPubKey
// followed by OP_DROPs.
enum
{

    // Syntax (scriptSig):
    //     victim GAMEOP_KILLED_BY killer1 killer2 ... killerN
    // Player can be killed simultaneously by multiple other players.
    // If N = 0, player was killed for staying too long in spawn area.
    GAMEOP_KILLED_BY = 1,

    // Syntax (scriptSig):
    //     player GAMEOP_COLLECTED_BOUNTY characterIndex firstBlock lastBlock collectedFirstBlock collectedLastBlock
    // vin.size() == vout.size(), they correspond to each other, i.e. a dummy input is used
    // to hold info about the corresponding output in its scriptSig
    // (alternatively we could add vout index to the scriptSig, to allow more complex transactions
    // with arbitrary input assignments, or store it in scriptPubKey of the tx-out instead)
    GAMEOP_COLLECTED_BOUNTY = 2,

    // Syntax (scriptSig):
    //     victim GAMEOP_KILLED_POISON
    // Player was killed due to poisoning
    GAMEOP_KILLED_POISON = 3,

    // Syntax (scriptSig):
    //     player GAMEOP_REFUND characterIndex height
    // This is a tx to refund a player's coins after staying long
    // in the spawn area.  characterIndex is usually 0, but keep it
    // here for future extensibility.
    GAMEOP_REFUND = 4,

};

bool
CreateGameTransactions (const CCoinsView& view, const StepResult& stepResult,
                        std::vector<CTransaction>& vGameTx)
{
  vGameTx.clear ();
  if (fDebug)
    LogPrintf ("Constructing game transactions...\n");

  /* Destroy name-coins of killed players.  */

  CMutableTransaction txKills;
  txKills.SetGameTx ();

  const PlayerSet& killedPlayers = stepResult.GetKilledPlayers ();
  const KilledByMap& killedBy = stepResult.GetKilledBy ();
  txKills.vin.reserve (killedPlayers.size ());
  BOOST_FOREACH(const PlayerID &victim, killedPlayers)
    {
      const valtype vchName = ValtypeFromString (victim);
      CNameData data;
      if (!view.GetName (vchName, data))
        return error ("Game engine killed a non-existing player %s",
                      victim.c_str ());

      if (fDebug)
        LogPrintf ("  killed: %s\n", victim.c_str ());

      CTxIn txin(data.getUpdateOutpoint ());

      /* List all killers, if player was simultaneously killed by several
         other players.  If the reason was not KILLED_DESTRUCT, handle
         it also.  If multiple reasons apply, the game tx is constructed
         for the first reason according to the ordering inside of KilledByMap.
         (Which in turn is determined by the enum values for KILLED_*.)  */

      typedef KilledByMap::const_iterator Iter;
      const std::pair<Iter, Iter> iters = killedBy.equal_range (victim);
      if (iters.first == iters.second)
        return error ("No reason for killed player %s", victim.c_str ());
      const KilledByInfo::Reason reason = iters.first->second.reason;

      /* Unless we have destruct, there should be exactly one entry with
         the "first" reason.  There may be multiple entries for different
         reasons, for instance, killed by poison and staying in spawn
         area at the same time.  */
      {
        Iter it = iters.first;
        ++it;
        if (reason != KilledByInfo::KILLED_DESTRUCT && it != iters.second
            && reason == it->second.reason)
          return error ("Multiple same-reason, non-destruct killed-by"
                        " entries for %s", victim.c_str ());
      }

      switch (reason)
        {
        case KilledByInfo::KILLED_DESTRUCT:
          txin.scriptSig << vchName << GAMEOP_KILLED_BY;
          for (Iter it = iters.first; it != iters.second; ++it)
            {
              if (it->second.reason != KilledByInfo::KILLED_DESTRUCT)
                {
                  assert (it != iters.first);
                  break;
                }
              txin.scriptSig
                << ValtypeFromString (it->second.killer.ToString ());
            }
          break;

        case KilledByInfo::KILLED_SPAWN:
          txin.scriptSig << vchName << GAMEOP_KILLED_BY;
          break;

        case KilledByInfo::KILLED_POISON:
          txin.scriptSig << vchName << GAMEOP_KILLED_POISON;
          break;

        default:
          assert (false);
        }

      txKills.vin.push_back (txin);
    }
  if (!txKills.vin.empty ())
    {
      vGameTx.push_back (txKills);
      if (fDebug)
        LogPrintf ("Game tx for killed players: %s\n",
                   txKills.GetHash ().GetHex ().c_str ());
    }

  /* Pay bounties to the players who collected them.  The transaction
     inputs are just "dummy" containing informational messages.  */

  CMutableTransaction txBounties;
  txBounties.SetGameTx ();

  txBounties.vin.reserve (stepResult.bounties.size ());
  txBounties.vout.reserve (stepResult.bounties.size ());

  BOOST_FOREACH(const CollectedBounty& bounty, stepResult.bounties)
    {
      const valtype vchName = ValtypeFromString (bounty.character.player);
      CNameData data;
      if (!view.GetName (vchName, data))
        return error ("Game engine created bounty for non-existing player");

      CTxOut txout;
      txout.nValue = bounty.loot.nAmount;

      if (!bounty.address.empty ())
        {
          /* Player-provided addresses are validated before accepting them,
             so failing here is ok.  */
          CBitcoinAddress addr(bounty.address);
          if (!addr.IsValid ())
            return error ("Failed to set player-provided address for bounty");
          txout.scriptPubKey = GetScriptForDestination (addr.Get ());
        }
      else
        txout.scriptPubKey = data.getAddress ();

      txBounties.vout.push_back (txout);

      CTxIn txin;
      if (bounty.loot.IsRefund ())
        txin.scriptSig
          << vchName << GAMEOP_REFUND
          << bounty.character.index << bounty.loot.GetRefundHeight ();
      else
        txin.scriptSig
          << vchName << GAMEOP_COLLECTED_BOUNTY
          << bounty.character.index
          << bounty.loot.firstBlock
          << bounty.loot.lastBlock
          << bounty.loot.collectedFirstBlock
          << bounty.loot.collectedLastBlock;
      txBounties.vin.push_back (txin);
    }
  if (!txBounties.vout.empty ())
    {
      vGameTx.push_back (txBounties);
      if (fDebug)
        LogPrintf ("Game tx for bounties: %s\n",
                   txBounties.GetHash ().GetHex ().c_str ());
    }

  return true;
}