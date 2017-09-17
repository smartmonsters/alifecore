// Copyright (c) 2014-2017 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "coins.h"
#include "game/db.h"
#include "game/move.h"
#include "game/state.h"
#include "game/tx.h"
#include "init.h"
#include "names/common.h"
#include "names/main.h"
#include "primitives/transaction.h"
#include "random.h"
#include "rpc/mining.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "script/names.h"
#include "txmempool.h"
#include "util.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"

#include <univalue.h>

// Maximum number of outputs that are checked for the NAME_NEW prevout.
constexpr unsigned MAX_NAME_PREVOUT_TRIALS = 1000;

/**
 * Helper routine to fetch the name output of a previous transaction.  This
 * is required for name_firstupdate.
 * @param txid Previous transaction ID.
 * @param txOut Set to the corresponding output.
 * @param txIn Set to the CTxIn to include in the new tx.
 * @return True if the output could be found.
 */
static bool
getNamePrevout (const uint256& txid, CTxOut& txOut, CTxIn& txIn)
{
  AssertLockHeld (cs_main);

  // Unfortunately, with the change of the txdb to be based on outputs rather
  // than full transactions, we can no longer just look up the txid and iterate
  // over all outputs.  Since this is only necessary for a corner case, we just
  // keep trying with indices until we find the output (up to a maximum number
  // of trials).

  for (unsigned i = 0; i < MAX_NAME_PREVOUT_TRIALS; ++i)
    {
      const COutPoint outp(txid, i);

      Coin coin;
      if (!pcoinsTip->GetCoin (outp, coin))
        continue;

      if (!coin.out.IsNull ()
          && CNameScript::isNameScript (coin.out.scriptPubKey))
        {
          txOut = coin.out;
          txIn = CTxIn (outp);
          return true;
        }
    }

  return false;
}

/**
 * Compute required game fee for a certain move.
 * @param name The name that is updated.
 * @param value The value encoding the move.
 * @return The required game fee for that move.
 */
static CAmount
GetRequiredGameFee (const valtype& name, const valtype& value)
{
  Move m;
  const bool ok = m.Parse (ValtypeToString (name), ValtypeToString (value));
  if (!ok)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid move");

  {
    LOCK (cs_main);
    return m.MinimumGameFee (Params ().GetConsensus (),
                             chainActive.Height () + 1);
  }
}

/* ************************************************************************** */

/**
 * Helper class for the implementation of name_list.  In Huntercoin, things
 * are more complicated due to kill transactions that might change multiple
 * names in a single tx.  To handle them, name_list uses this class
 * to track current heights of names and update them.
 */
class NameListBuilder
{
private:

  const valtype nameFilter;

  std::map<valtype, int> mapHeights;
  std::map<valtype, UniValue> mapObjects;

  /* Data for the current tx, which may be used for multiple changes
     if we handle kill transactions.  */
  int nHeight;
  bool isKillTx;

public:

  explicit inline NameListBuilder (const valtype& filter)
    : nameFilter(filter),
      mapHeights(), mapObjects()
  {}

  /* Returns false if the tx should be skipped (is unconfirmed).  */
  bool startTx (const CWalletTx& tx);

  inline int
  getHeight () const
  {
    return nHeight;
  }

  void add (const valtype& name, const UniValue& obj);

  UniValue build () const;

};

bool
NameListBuilder::startTx (const CWalletTx& tx)
{
  const CBlockIndex* pindex;
  const int depth = tx.GetDepthInMainChain (pindex);
  if (depth <= 0)
    return false;

  nHeight = pindex->nHeight;
  isKillTx = tx.IsKillTx ();

  return true;
}

void
NameListBuilder::add (const valtype& name, const UniValue& obj)
{
  if (!nameFilter.empty () && nameFilter != name)
    return;

  /* Kill transactions have precedence over the non-kill name_update that
     might be in the same block (when self-destructing).  */
  const std::map<valtype, int>::const_iterator mit = mapHeights.find (name);
  if (mit == mapHeights.end () || mit->second < nHeight
      || (mit->second == nHeight && isKillTx))
    {
      mapHeights[name] = nHeight;
      mapObjects[name] = obj;
    }
}

UniValue
NameListBuilder::build () const
{
  UniValue res(UniValue::VARR);
  for (const auto& item : mapObjects)
    res.push_back (item.second);

  return res;
}

UniValue
name_list (const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp || request.params.size () > 1)
    throw std::runtime_error (
        "name_list (\"name\")\n"
        "\nShow status of names in the wallet.\n"
        "\nArguments:\n"
        "1. \"name\"          (string, optional) only include this name\n"
        "\nResult:\n"
        "[\n"
        + getNameInfoHelp ("  ", ",") +
        "  ...\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("name_list", "")
        + HelpExampleCli ("name_list", "\"myname\"")
        + HelpExampleRpc ("name_list", "")
      );

  ObserveSafeMode ();

  valtype nameFilter;
  if (request.params.size () == 1)
    nameFilter = ValtypeFromString (request.params[0].get_str ());

  NameListBuilder builder(nameFilter);

  {
  LOCK2 (cs_main, pwallet->cs_wallet);
  for (const auto& item : pwallet->mapWallet)
    {
      const CWalletTx& tx = item.second;
      if (!tx.tx->IsNamecoin () && !tx.IsKillTx ())
        continue;

      if (!builder.startTx (tx))
        continue;

      if (tx.IsKillTx ())
        {
          for (const auto& txIn : tx.tx->vin)
            {
              if (!pwallet->IsMine (txIn))
                continue;

              valtype name;
              if (!NameFromGameTransactionInput (txIn.scriptSig, name))
                {
                  LogPrintf ("ERROR: failed to get name from kill input");
                  continue;
                }

              UniValue obj = getNameInfo (name, valtype (), true,
                                          COutPoint (tx.GetHash (), 0),
                                          CScript (), builder.getHeight ());
              builder.add (name, obj);
            }

          continue;
        }

      CNameScript nameOp;
      int nOut = -1;
      for (unsigned i = 0; i < tx.tx->vout.size (); ++i)
        {
          const CNameScript cur(tx.tx->vout[i].scriptPubKey);
          if (cur.isNameOp ())
            {
              if (nOut != -1)
                LogPrintf ("ERROR: wallet contains tx with multiple"
                           " name outputs");
              else
                {
                  nameOp = cur;
                  nOut = i;
                }
            }
        }

      if (nOut == -1 || !nameOp.isAnyUpdate ())
        continue;

      const valtype& name = nameOp.getOpName ();
      UniValue obj
        = getNameInfo (name, nameOp.getOpValue (), false,
                       COutPoint (tx.GetHash (), nOut),
                       nameOp.getAddress (), builder.getHeight ());

      const bool mine = IsMine (*pwallet, nameOp.getAddress ());
      obj.pushKV ("transferred", !mine);

      builder.add (name, obj);
    }
  }

  return builder.build ();
}

/* ************************************************************************** */

UniValue
name_new (const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp || request.params.size () != 1)
    throw std::runtime_error (
        "name_new \"name\"\n"
        "\nStart registration of the given name.  Must be followed up with"
        " name_firstupdate to finish the registration.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"name\"          (string, required) the name to register\n"
        "\nResult:\n"
        "[\n"
        "  xxxxx,   (string) the txid, required for name_firstupdate\n"
        "  xxxxx    (string) random value for name_firstupdate\n"
        "]\n"
        "\nExamples:\n"
        + HelpExampleCli ("name_new", "\"myname\"")
        + HelpExampleRpc ("name_new", "\"myname\"")
      );

  ObserveSafeMode ();

  const std::string nameStr = request.params[0].get_str ();
  const valtype name = ValtypeFromString (nameStr);
  if (name.size () > MAX_NAME_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is too long");
  if (!Move::IsValidPlayerName (nameStr))
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is not valid");

  valtype rand(20);
  GetRandBytes (&rand[0], rand.size ());

  valtype toHash(rand);
  toHash.insert (toHash.end (), name.begin (), name.end ());
  const uint160 hash = Hash160 (toHash);

  /* No explicit locking should be necessary.  CReserveKey takes care
     of locking the wallet, and CommitTransaction (called when sending
     the tx) locks cs_main as necessary.  */

  EnsureWalletIsUnlocked (pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKey;
  const bool ok = keyName.GetReservedKey (pubKey, true);
  assert (ok);
  const CScript addrName = GetScriptForDestination (pubKey.GetID ());
  const CScript newScript = CNameScript::buildNameNew (addrName, hash);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript (pwallet, newScript, nullptr,
                     NAMENEW_COIN_AMOUNT, false, wtx, coinControl);

  keyName.KeepKey ();

  const std::string randStr = HexStr (rand);
  const std::string txid = wtx.GetHash ().GetHex ();
  LogPrintf ("name_new: name=%s, rand=%s, tx=%s\n",
             nameStr.c_str (), randStr.c_str (), txid.c_str ());

  UniValue res(UniValue::VARR);
  res.push_back (txid);
  res.push_back (randStr);

  return res;
}

/* ************************************************************************** */

UniValue
name_firstupdate (const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  /* There is an undocumented sixth argument that can be used to disable
     the check for already existing names here (it will still be checked
     by the mempool and tx validation logic, of course).  This is used
     by the regtests to catch a bug that was previously present but
     has presumably no other use.  */

  if (request.fHelp || request.params.size () < 4 || request.params.size () > 6)
    throw std::runtime_error (
        "name_firstupdate \"name\" \"rand\" \"tx\" \"value\" (\"toaddress\")\n"
        "\nFinish the registration of a name.  Depends on name_new being"
        " already issued.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"name\"          (string, required) the name to register\n"
        "2. \"rand\"          (string, required) the rand value of name_new\n"
        "3. \"tx\"            (string, required) the name_new txid\n"
        "4. \"value\"         (string, required) value for the name\n"
        "5. \"toaddress\"     (string, optional) address to send the name to\n"
        "\nResult:\n"
        "\"txid\"             (string) the name_firstupdate's txid\n"
        "\nExamples:\n"
        + HelpExampleCli ("name_firstupdate", "\"myname\", \"555844f2db9c7f4b25da6cb8277596de45021ef2\" \"a77ceb22aa03304b7de64ec43328974aeaca211c37dd29dcce4ae461bb80ca84\", \"my-value\"")
        + HelpExampleCli ("name_firstupdate", "\"myname\", \"555844f2db9c7f4b25da6cb8277596de45021ef2\" \"a77ceb22aa03304b7de64ec43328974aeaca211c37dd29dcce4ae461bb80ca84\", \"my-value\", \"NEX4nME5p3iyNK3gFh4FUeUriHXxEFemo9\"")
        + HelpExampleRpc ("name_firstupdate", "\"myname\", \"555844f2db9c7f4b25da6cb8277596de45021ef2\" \"a77ceb22aa03304b7de64ec43328974aeaca211c37dd29dcce4ae461bb80ca84\", \"my-value\"")
      );

  ObserveSafeMode ();

  const std::string nameStr = request.params[0].get_str ();
  const valtype name = ValtypeFromString (nameStr);
  if (name.size () > MAX_NAME_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is too long");
  if (!Move::IsValidPlayerName (nameStr))
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is not valid");

  const valtype rand = ParseHexV (request.params[1], "rand");
  if (rand.size () > 20)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "invalid rand value");

  const uint256 prevTxid = ParseHashV (request.params[2], "txid");

  const std::string valueStr = request.params[3].get_str ();
  const valtype value = ValtypeFromString (valueStr);
  if (value.size () > MAX_VALUE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the value is too long");

  {
    LOCK (mempool.cs);
    if (mempool.registersName (name))
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "this name is already being registered");
  }

  if (request.params.size () < 6 || !request.params[5].get_bool ())
    {
      LOCK (cs_main);
      CNameData oldData;
      if (pcoinsTip->GetName (name, oldData) && !oldData.isDead ())
        throw JSONRPCError (RPC_TRANSACTION_ERROR,
                            "this name is already active");
    }

  CTxOut prevOut;
  CTxIn txIn;
  {
    LOCK (cs_main);
    if (!getNamePrevout (prevTxid, prevOut, txIn))
      throw JSONRPCError (RPC_TRANSACTION_ERROR, "previous txid not found");
  }

  const CNameScript prevNameOp(prevOut.scriptPubKey);
  assert (prevNameOp.isNameOp ());
  if (prevNameOp.getNameOp () != OP_NAME_NEW)
    throw JSONRPCError (RPC_TRANSACTION_ERROR, "previous tx is not name_new");

  valtype toHash(rand);
  toHash.insert (toHash.end (), name.begin (), name.end ());
  if (uint160 (prevNameOp.getOpHash ()) != Hash160 (toHash))
    throw JSONRPCError (RPC_TRANSACTION_ERROR, "rand value is wrong");

  /* No more locking required, similarly to name_new.  */

  EnsureWalletIsUnlocked (pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKeyReserve;
  const bool ok = keyName.GetReservedKey (pubKeyReserve, true);
  assert (ok);
  bool usedKey = false;

  CScript addrName;
  if (request.params.size () >= 5)
    {
      keyName.ReturnKey ();
      const CTxDestination dest
        = DecodeDestination(request.params[4].get_str ());
      if (!IsValidDestination(dest))
        throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, "invalid address");

      addrName = GetScriptForDestination (dest);
    }
  else
    {
      usedKey = true;
      addrName = GetScriptForDestination (pubKeyReserve.GetID ());
    }

  const CScript nameScript
    = CNameScript::buildNameFirstupdate (addrName, name, value, rand);
  const CAmount amount = GetRequiredGameFee (name, value);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript (pwallet, nameScript, &txIn,
                     amount, false, wtx, coinControl);

  if (usedKey)
    keyName.KeepKey ();

  return wtx.GetHash ().GetHex ();
}

/* ************************************************************************** */

UniValue
name_update (const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp
      || (request.params.size () != 2 && request.params.size () != 3))
    throw std::runtime_error (
        "name_update \"name\" \"value\" (\"toaddress\")\n"
        "\nUpdate a name and possibly transfer it.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"name\"          (string, required) the name to update\n"
        "4. \"value\"         (string, required) value for the name\n"
        "5. \"toaddress\"     (string, optional) address to send the name to\n"
        "\nResult:\n"
        "\"txid\"             (string) the name_update's txid\n"
        "\nExamples:\n"
        + HelpExampleCli ("name_update", "\"myname\", \"new-value\"")
        + HelpExampleCli ("name_update", "\"myname\", \"new-value\", \"NEX4nME5p3iyNK3gFh4FUeUriHXxEFemo9\"")
        + HelpExampleRpc ("name_update", "\"myname\", \"new-value\"")
      );

  ObserveSafeMode ();

  const std::string nameStr = request.params[0].get_str ();
  const valtype name = ValtypeFromString (nameStr);
  if (name.size () > MAX_NAME_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is too long");

  const std::string valueStr = request.params[1].get_str ();
  const valtype value = ValtypeFromString (valueStr);
  if (value.size () > MAX_VALUE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the value is too long");

  /* Reject updates to a name for which the mempool already has
     a pending update.  This is not a hard rule enforced by network
     rules, but it is necessary with the current mempool implementation.  */
  {
    LOCK (mempool.cs);
    if (mempool.updatesName (name))
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "there is already a pending update for this name");
  }

  CNameData oldData;
  GameState gameState(Params ().GetConsensus ());
  {
    LOCK (cs_main);
    if (!pcoinsTip->GetName (name, oldData) || oldData.isDead ())
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "this name can not be updated");
    if (!pgameDb->get (pcoinsTip->GetBestBlock (), gameState))
      throw JSONRPCError (RPC_INTERNAL_ERROR, "failed to load game state");
  }

  const COutPoint outp = oldData.getUpdateOutpoint ();
  const CTxIn txIn(outp);

  /* No more locking required, similarly to name_new.  */

  EnsureWalletIsUnlocked (pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKeyReserve;
  const bool ok = keyName.GetReservedKey (pubKeyReserve, true);
  assert (ok);
  bool usedKey = false;

  CScript addrName;
  if (request.params.size () == 3)
    {
      keyName.ReturnKey ();
      const CTxDestination dest
        = DecodeDestination (request.params[2].get_str ());
      if (!IsValidDestination (dest))
        throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, "invalid address");

      addrName = GetScriptForDestination (dest);
    }
  else
    {
      usedKey = true;
      addrName = GetScriptForDestination (pubKeyReserve.GetID ());
    }

  const CScript nameScript
    = CNameScript::buildNameUpdate (addrName, name, value);

  /* Find amount locked in the name and add required game fee.  */
  const PlayerStateMap::const_iterator mi = gameState.players.find (nameStr);
  if (mi == gameState.players.end ())
    throw JSONRPCError (RPC_INTERNAL_ERROR,
                        "failed to find player in game state");
  CAmount amount = mi->second.lockedCoins;
  amount += GetRequiredGameFee (name, value);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript (pwallet, nameScript, &txIn,
                     amount, false, wtx, coinControl);

  if (usedKey)
    keyName.KeepKey ();

  return wtx.GetHash ().GetHex ();
}

/* ************************************************************************** */

UniValue
name_register (const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;

  if (request.fHelp || request.params.size () < 2 || request.params.size () > 3)
    throw std::runtime_error (
        "name_register \"name\" \"value\" (\"toaddress\")\n"
        "\nRegister a new player name according to the 'new-style rules'.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"name\"          (string, required) the name to register\n"
        "2. \"value\"         (string, required) value for the name\n"
        "3. \"toaddress\"     (string, optional) address to send the name to\n"
        "\nResult:\n"
        "\"txid\"             (string) the name_firstupdate's txid\n"
        "\nExamples:\n"
        + HelpExampleCli ("name_register", "\"myname\", \"my-value\"")
        + HelpExampleCli ("name_register", "\"myname\", \"my-value\", \"NEX4nME5p3iyNK3gFh4FUeUriHXxEFemo9\"")
        + HelpExampleRpc ("name_register", "\"myname\", \"my-value\"")
      );

  ObserveSafeMode ();

  const std::string nameStr = request.params[0].get_str ();
  const valtype name = ValtypeFromString (nameStr);
  if (name.size () > MAX_NAME_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is too long");
  if (!Move::IsValidPlayerName (nameStr))
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the name is not valid");

  const std::string valueStr = request.params[1].get_str ();
  const valtype value = ValtypeFromString (valueStr);
  if (value.size () > MAX_VALUE_LENGTH)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "the value is too long");

  {
    LOCK (mempool.cs);
    if (mempool.registersName (name))
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "this name is already being registered");
  }

  {
    LOCK (cs_main);
    CNameData oldData;
    if (pcoinsTip->GetName (name, oldData) && !oldData.isDead ())
      throw JSONRPCError (RPC_TRANSACTION_ERROR,
                          "this name is already active");
  }

  /* No more locking required, similarly to name_new.  */

  EnsureWalletIsUnlocked (pwallet);

  CReserveKey keyName(pwallet);
  CPubKey pubKeyReserve;
  const bool ok = keyName.GetReservedKey (pubKeyReserve, true);
  assert (ok);
  bool usedKey = false;

  CScript addrName;
  if (request.params.size () >= 3)
    {
      keyName.ReturnKey ();
      const CTxDestination dest
        = DecodeDestination (request.params[2].get_str ());
      if (!IsValidDestination (dest))
        throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, "invalid address");

      addrName = GetScriptForDestination (dest);
    }
  else
    {
      usedKey = true;
      addrName = GetScriptForDestination (pubKeyReserve.GetID ());
    }

  const CScript nameScript
    = CNameScript::buildNameRegister (addrName, name, value);
  const CAmount amount = GetRequiredGameFee (name, value);

  CCoinControl coinControl;
  CWalletTx wtx;
  SendMoneyToScript (pwallet, nameScript, nullptr,
                     amount, false, wtx, coinControl);

  if (usedKey)
    keyName.KeepKey ();

  return wtx.GetHash ().GetHex ();
}

/* ************************************************************************** */

UniValue
sendtoname (const JSONRPCRequest& request)
{
  CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable (pwallet, request.fHelp))
    return NullUniValue;
  
  if (request.fHelp || request.params.size () < 2 || request.params.size () > 8)
    throw std::runtime_error (
        "sendtoname \"name\" amount ( \"comment\" \"comment_to\" subtractfeefromamount replaceable conf_target \"estimate_mode\")\n"
        "\nSend an amount to the owner of a name. "
        " The amount is a real and is rounded to the nearest 0.00000001.\n"
        + HelpRequiringPassphrase (pwallet) +
        "\nArguments:\n"
        "1. \"name\"        (string, required) The name to send to.\n"
        "2. \"amount\"      (numeric, required) The amount in nmc to send. eg 0.1\n"
        "3. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
        "                             This is not part of the transaction, just kept in your wallet.\n"
        "4. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
        "                             to which you're sending the transaction. This is not part of the \n"
        "                             transaction, just kept in your wallet.\n"
        "5. subtractfeefromamount  (boolean, optional, default=false) The fee will be deducted from the amount being sent.\n"
        "                             The recipient will receive less namecoins than you enter in the amount field.\n"
        "6. replaceable            (boolean, optional) Allow this transaction to be replaced by a transaction with higher fees via BIP 125\n"
        "7. conf_target            (numeric, optional) Confirmation target (in blocks)\n"
        "8. \"estimate_mode\"      (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
        "       \"UNSET\"\n"
        "       \"ECONOMICAL\"\n"
        "       \"CONSERVATIVE\"\n"
        "\nResult:\n"
        "\"transactionid\"  (string) The transaction id.\n"
        "\nExamples:\n"
        + HelpExampleCli ("sendtoname", "\"id/foobar\" 0.1")
        + HelpExampleCli ("sendtoname", "\"id/foobar\" 0.1 \"donation\" \"seans outpost\"")
        + HelpExampleCli ("sendtoname", "\"id/foobar\" 0.1 \"\" \"\" true")
        + HelpExampleRpc ("sendtoname", "\"id/foobar\", 0.1, \"donation\", \"seans outpost\"")
      );

  if (IsInitialBlockDownload ())
    throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD,
                       "Namecoin is downloading blocks...");

  ObserveSafeMode ();
  LOCK2 (cs_main, pwallet->cs_wallet);

  const std::string nameStr = request.params[0].get_str ();
  const valtype name = ValtypeFromString (nameStr);

  CNameData data;
  if (!pcoinsTip->GetName (name, data))
    {
      std::ostringstream msg;
      msg << "name not found: '" << nameStr << "'";
      throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, msg.str ());
    }
  /* FIXME: Check for dead player?  */

  /* The code below is strongly based on sendtoaddress.  Make sure to
     keep it in sync.  */

  // Amount
  CAmount nAmount = AmountFromValue(request.params[1]);
  if (nAmount <= 0)
      throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");

  // Wallet comments
  CWalletTx wtx;
  if (request.params.size() > 2 && !request.params[2].isNull()
        && !request.params[2].get_str().empty())
      wtx.mapValue["comment"] = request.params[2].get_str();
  if (request.params.size() > 3 && !request.params[3].isNull()
        && !request.params[3].get_str().empty())
      wtx.mapValue["to"]      = request.params[3].get_str();

  bool fSubtractFeeFromAmount = false;
  if (!request.params[4].isNull())
      fSubtractFeeFromAmount = request.params[4].get_bool();

  CCoinControl coin_control;
  if (!request.params[5].isNull()) {
      coin_control.signalRbf = request.params[5].get_bool();
  }

  if (!request.params[6].isNull()) {
      coin_control.m_confirm_target = ParseConfirmTarget(request.params[6]);
  }

  if (!request.params[7].isNull()) {
      if (!FeeModeFromString(request.params[7].get_str(),
          coin_control.m_fee_mode)) {
          throw JSONRPCError(RPC_INVALID_PARAMETER,
                             "Invalid estimate_mode parameter");
      }
  }

  EnsureWalletIsUnlocked(pwallet);

  SendMoneyToScript (pwallet, data.getAddress (), nullptr,
                     nAmount, fSubtractFeeFromAmount, wtx, coin_control);

  return wtx.GetHash ().GetHex ();
}
