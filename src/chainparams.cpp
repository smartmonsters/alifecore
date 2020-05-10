// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "names/common.h"
#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include "chainparamsseeds.h"

bool CChainParams::IsHistoricBug(const uint256& txid, unsigned nHeight, BugType& type) const
{
    const std::pair<unsigned, uint256> key(nHeight, txid);
    std::map<std::pair<unsigned, uint256>, BugType>::const_iterator mi;

    mi = mapHistoricBugs.find (key);
    if (mi != mapHistoricBugs.end ())
    {
        type = mi->second;
        return true;
    }

    return false;
}

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << ValtypeFromString(std::string(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block.
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char *pszTimestamp =
            "\n"
            "Huntercoin genesis timestamp\n"
            "31/Jan/2014 20:10 GMT\n"
            "Bitcoin block 283440: 0000000000000001795d3c369b0746c0b5d315a6739a7410ada886de5d71ca86\n"
            "Litecoin block 506479: 77c49384e6e8dd322da0ebb32ca6c8f047d515d355e9f22b116430a888fffd38\n"
        ;
    const CScript genesisOutputScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("fe2435b201d25290533bdaacdfe25dc7548b3058") << OP_EQUALVERIFY << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Build genesis block for testnet.  In Huntercoin, it has a changed timestamp
 * and output script.
 */
static CBlock CreateTestnetGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    // SMC basic conversion -- testnet
//    const char* pszTimestamp = "\nHuntercoin test net\n";
//    const CScript genesisOutputScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("7238d2df990b8e333ed28a84a8df8408f6dbcd57") << OP_EQUALVERIFY << OP_CHECKSIG;
    const char* pszTimestamp = "\n"
                               "SmartMonsters beta testnet timestamp\n"
                               "July 28, 2018 15:00 GMT\n";
    const CScript genesisOutputScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("2e1d00911a6f125e1dd190d932e19bfcf3157670") << OP_EQUALVERIFY << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 2100000;
        /* FIXME: Set to activate the forks.  */
        consensus.BIP34Height = 1000000000;
        consensus.BIP65Height = 1000000000;
        consensus.BIP66Height = 1000000000;
        consensus.powLimit[ALGO_SHA256D] = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimit[ALGO_SCRYPT] = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 60 * NUM_ALGOS;
        consensus.nPowTargetTimespan = consensus.nPowTargetSpacing * 2016;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 0; // Not yet enabled

        // Deployment of SegWit (BIP141 and BIP143)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 0; // Not yet enabled

        consensus.nAuxpowChainId[ALGO_SHA256D] = 0x0006;
        consensus.nAuxpowChainId[ALGO_SCRYPT] = 0x0002;
        consensus.fStrictChainId = true;

        consensus.rules.reset(new Consensus::MainNetConsensus());

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xf9;
        pchMessageStart[1] = 0xbe;
        pchMessageStart[2] = 0xb4;
        pchMessageStart[3] = 0xfe;
        nDefaultPort = 8398;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1391199780, 1906435634u, 486604799, 1, 85000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000000db7eb7a9e1a06cf995363dcdc4c28e8ae04827a961942657db9a1631"));
        assert(genesis.hashMerkleRoot == uint256S("0xc4ee946ffcb0bffa454782432d530bbeb8562b09594c1fbc8ceccd46ce34a754"));

        /* FIXME: Add DNS seeds.  */
        //vSeeds.push_back(CDNSSeedData("quisquis.de", "nmc.seed.quisquis.de"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,40);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,13); // FIXME: Update.
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,168);
        /* FIXME: Update these below.  */
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();

        // FIXME: Set seeds for Huntercoin.
        //vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (     0, uint256S("00000000db7eb7a9e1a06cf995363dcdc4c28e8ae04827a961942657db9a1631")),
            0, // * UNIX timestamp of last checkpoint block
            0, // * total number of transactions between genesis and last checkpoint
               //   (the tx=... number in the SetBestChain debug.log lines)
            0  // * estimated number of transactions per day after checkpoint
        };
    }

    int DefaultCheckNameDB () const
    {
        return -1;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 2100000;
        /* FIXME: Set to activate the forks.  */
        consensus.BIP34Height = 1000000000;
        consensus.BIP65Height = 1000000000;
        consensus.BIP66Height = 1000000000;
        consensus.powLimit[ALGO_SHA256D] = uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimit[ALGO_SCRYPT] = uint256S("000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 60 * NUM_ALGOS;
        consensus.nPowTargetTimespan = consensus.nPowTargetSpacing * 2016;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 0; // Not yet enabled

        // Deployment of SegWit (BIP141 and BIP143)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 0; // Not yet enabled

        // SMC basic conversion -- testnet
//        consensus.nAuxpowChainId[ALGO_SHA256D] = 0x0006;
//        consensus.nAuxpowChainId[ALGO_SCRYPT] = 0x0002;
        consensus.nAuxpowChainId[ALGO_SHA256D] = 555;
        consensus.nAuxpowChainId[ALGO_SCRYPT] = 555;
        consensus.fStrictChainId = false;

        consensus.rules.reset(new Consensus::TestNetConsensus());

        // SMC basic conversion -- testnet
//        pchMessageStart[0] = 0xfa;
//        pchMessageStart[1] = 0xbf;
//        pchMessageStart[2] = 0xb5;
//        pchMessageStart[3] = 0xfe;
//        nDefaultPort = 18398;
//        nPruneAfterHeight = 1000;

//        genesis = CreateTestnetGenesisBlock(1391193136, 1997599826u, 503382015, 1, 100 * COIN);
//        consensus.hashGenesisBlock = genesis.GetHash();
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xc5; // changed
        pchMessageStart[3] = 0xfe;
        nDefaultPort = 18396;
        nPruneAfterHeight = 1000;

        genesis = CreateTestnetGenesisBlock(1532790938, 537787730u, 0x1e00ffff, 1, 850000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // SMC basic conversion -- testnet
//        assert(consensus.hashGenesisBlock == uint256S("000000492c361a01ce7558a3bfb198ea3ff2f86f8b0c2e00d26135c53f4acbf7"));
//        assert(genesis.hashMerkleRoot == uint256S("28da665eada1b006bb9caf83e7541c6f995e0681debfc2540507bbfdf2d4ac84"));
        assert(consensus.hashGenesisBlock == uint256S("00000063c87e68fcefc5907d4b4ce8441295b7e34a5e91a98cae872d8e8f1e92"));
        assert(genesis.hashMerkleRoot == uint256S("1034706b901b8beae369534146f6c8211997446752eab752486c4c7c20280acb"));

        vFixedSeeds.clear();
        vSeeds.clear();
        /* FIXME: Testnet seeds?  */
        //vSeeds.push_back(CDNSSeedData("webbtc.com", "dnsseed.test.namecoin.webbtc.com"));

        // SMC basic conversion -- testnet
//        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,100);
//        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // FIXME: Update
//        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,228);
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // FIXME: Update
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        /* FIXME: Update these below.  */
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        // FIXME: Set seeds for Huntercoin.
        //vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            // SMC basic conversion -- testnet
//            (     0, uint256S("000000492c361a01ce7558a3bfb198ea3ff2f86f8b0c2e00d26135c53f4acbf7")),
            (     0, uint256S("00000063c87e68fcefc5907d4b4ce8441295b7e34a5e91a98cae872d8e8f1e92")),
            0,
            0,
            0
        };

        assert(mapHistoricBugs.empty());
    }

    int DefaultCheckNameDB () const
    {
        return -1;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit[ALGO_SHA256D] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimit[ALGO_SCRYPT] = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 60 * NUM_ALGOS;
        consensus.nPowTargetTimespan = consensus.nPowTargetSpacing * 2016;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 999999999999ULL;

        consensus.nAuxpowChainId[ALGO_SHA256D] = 0x0006;
        consensus.nAuxpowChainId[ALGO_SCRYPT] = 0x0002;
        consensus.fStrictChainId = true;

        consensus.rules.reset(new Consensus::RegTestConsensus());

        // SMC basic conversion -- testnet
        //        pchMessageStart[0] = 0xfa;
        //        pchMessageStart[1] = 0xbf;
        //        pchMessageStart[2] = 0xb5;
        //        pchMessageStart[3] = 0xda;
//        nDefaultPort = 18398;
//        nPruneAfterHeight = 1000;

        //        genesis = CreateTestnetGenesisBlock(1296688602, 2, 0x207fffff, 1, 50 * COIN);
//        consensus.hashGenesisBlock = genesis.GetHash();
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xc5; // changed
        pchMessageStart[3] = 0xfe; // changed
        nDefaultPort = 18396;
        nPruneAfterHeight = 1000;

        genesis = CreateTestnetGenesisBlock(1532790938, 537787730u, 0x1e00ffff, 1, 850000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // SMC basic conversion -- testnet
        //        assert(consensus.hashGenesisBlock == uint256S("0x3867dcd08712d9b49de33d4ab145d57ad14a78c7843c51f8c5d782d5f102fb4a"));
        //        assert(genesis.hashMerkleRoot == uint256S("0x71c88ed0560ee7d644deba07485c4eff571e3f86f9485692ed3966e4f0f3a59c"));
        assert(consensus.hashGenesisBlock == uint256S("00000063c87e68fcefc5907d4b4ce8441295b7e34a5e91a98cae872d8e8f1e92"));
        assert(genesis.hashMerkleRoot == uint256S("1034706b901b8beae369534146f6c8211997446752eab752486c4c7c20280acb"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            // SMC basic conversion -- testnet
//            ( 0, uint256S("3867dcd08712d9b49de33d4ab145d57ad14a78c7843c51f8c5d782d5f102fb4a")),
            (     0, uint256S("00000063c87e68fcefc5907d4b4ce8441295b7e34a5e91a98cae872d8e8f1e92")),
            0,
            0,
            0
        };
        // SMC basic conversion -- testnet
//        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,100);
//        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // FIXME: Update
//        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,228);
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // FIXME: Update
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        /* FIXME: Update below.  */
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        assert(mapHistoricBugs.empty());
    }

    int DefaultCheckNameDB () const
    {
        return 0;
    }

    void UpdateBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
            return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
            return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
            return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

void UpdateRegtestBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    regTestParams.UpdateBIP9Parameters(d, nStartTime, nTimeout);
}
 
