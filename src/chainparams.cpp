// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2019 The BeetleCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "libzerocoin/Params.h"
#include "chainparams.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"
#include "net.h"
#include "base58.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

/**
 * Main network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress>& vSeedsOut, const SeedSpec6* data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

//   What makes a good checkpoint block?
// + Is surrounded by blocks with reasonable timestamps
//   (no blocks before with a timestamp after, none after with
//    timestamp before)
// + Contains no strange transactions
static Checkpoints::MapCheckpoints mapCheckpoints =
    boost::assign::map_list_of
    (0, uint256("00000c9d6ee5917dcd9e9d291f4b2283fce7d6b8525a653267bae3a1c5fbdd00"))
    (50000, uint256("211798922d84dd3a816fe251f5a2ec32910f6f226e759abdbb0ea629ab0f9c98"))
    (100000, uint256("6a7dc0e5cf7567a356624fb238742b9db1d575bae06c1bf914d20d91a55436ac"))
    (150000, uint256("2196baf2af3553b453842cb4598966d4d284ef88afebfe623f7fc7e30b992203"))
    (200000, uint256("1df5a144d0f9b7782f7bd272b08d33957fc0033f7c4b14210d0dc76d595e28cf"))
    (250000, uint256("73880d48ac6868e059da9340dc830b8b28d6649efbd46de6f6196e002c0751fa"))
    (300000, uint256("a7d09f0093e50268f01c2646c0f346b1d0ac37807a3abead0df34ff94386f0c4"))
    (350000, uint256("dd276b42f8efc8045f418d9f670d739738100c76c2e43d5acd1f29fdce5a2cb7"))
    (400000, uint256("95312e692c2b8ee9148dd55cd34d4f1215c75be90f7d6764dcfa063d180cfa8b"))
    (450000, uint256("c9ad4da1a469f337059c502806990f4918fa536e309ce51a6895489411f4f859"))
    (500000, uint256("8f8ae0a206f418a7a6a8f6f67219cd3761cf2616ddeb4780fcb8a6242bf95996"))
    (550000, uint256("fdc92657576685dc03016cc371c2099f5c791efcf883a39bf5fe5e38009fb969"))
    (600000, uint256("e508a4b604fcf6956b975706e7e0df6bd41c6d1cb6d9ecbeefa9cc2e9d63f74f"))
    (650000, uint256("b948291e40bc18b11339248fc5861803a9fc3286312cee08cb0b475364e93668"))
    (700000, uint256("be8a8e5fd45a4b912d729231edfdc3e96108260d81757068b203e011d2955f89"))
    (750000, uint256("db40df6362d4cec724ea9e7274a6b7e6765cf4583f0e66e55fad48dcd0e4162d"))
    (800000, uint256("96234e1990d81879517ccefaf80c2a4d002c648ce0baf96b4197cbc7c6569298"))
    (850000, uint256("01b7066b359c327ebbd5e0a43b5222ec93d6e30eff5d93e54351fa8cb6b9766c"))
    (900000, uint256("e88bc6d85922852761da1d261600f1071e019183a1d2d204e6b3680a1366a30f"))
    (950000, uint256("74c895ecb4d76202e086b78dce9c44415279aaecff207e5df89726f05ac7b769"))
    (1000000, uint256("89cd375ad40438fce0fab4f55be3a5c03db00e87de2d58f5ac00288c04408f60"))
    (1050000, uint256("2cb8d49c2f63ce1fc4188058bd9d9074e5a23e5eb64b159343e98727693d41be"))
    (1100000, uint256("e5b59a6b3819832930a709835201a59e313caa445dfea4b9ffd224cf1a605350"));
static const Checkpoints::CCheckpointData data = {
    &mapCheckpoints,
    1603040428, // * UNIX timestamp of last checkpoint block
    2473402,    // * total number of transactions between genesis and last checkpoint
                //   (the tx=... number in the SetBestChain debug.log lines)
    2000        // * estimated number of transactions per day after checkpoint
};

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
    boost::assign::map_list_of
    (0, uint256("0x001"));
static const Checkpoints::CCheckpointData dataTestnet = {
    &mapCheckpointsTestnet,
    1536981458,
    0,
    250};

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
    boost::assign::map_list_of
    (0, uint256("0x001"));
static const Checkpoints::CCheckpointData dataRegtest = {
    &mapCheckpointsRegtest,
    1454124731,
    0,
    100};

libzerocoin::ZerocoinParams* CChainParams::Zerocoin_Params(bool useModulusV1) const
{
    assert(this);
    static CBigNum bnHexModulus = 0;
    if (!bnHexModulus)
        bnHexModulus.SetHex(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsHex = libzerocoin::ZerocoinParams(bnHexModulus);
    static CBigNum bnDecModulus = 0;
    if (!bnDecModulus)
        bnDecModulus.SetDec(zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsDec = libzerocoin::ZerocoinParams(bnDecModulus);

    if (useModulusV1)
        return &ZCParamsHex;

    return &ZCParamsDec;
}

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0x10;
        pchMessageStart[1] = 0x48;
        pchMessageStart[2] = 0x09;
        pchMessageStart[3] = 0x18;
        vAlertPubKey = ParseHex("04DE3E3D0380A7359563B990F7AF701320F44CEB0FBC325CD7EB06A6C228FE57D8448AD2365E7F36B31591B9B3BFCE6A5FE9A01773215604CB9DD512470AFBB9BB");
        nDefaultPort = 3133;
        bnProofOfWorkLimit = ~uint256(0) >> 20; // 0x1e0fffff
        //nSubsidyHalvingInterval = 2100000;
        nMaxReorganizationDepth = 100;
        nEnforceBlockUpgradeMajority = 7560; // 70%
        nRejectBlockOutdatedMajority = 7560; // 70%
        nToCheckBlockUpgradeMajority = 10800; // Approximate expected amount of blocks in 7 days (1440*7.5)
        nMinerThreads = 0;
        nTargetTimespan = 1 * 24 * 60 * 60; // 1 day
        nTargetSpacing = 64; // 64 seconds
        nMaturity = 10;
        nMasternodeCountDrift = 20;
        nFirstSupplyReduction = 400000000 * COIN;
        nSecondSupplyReduction = 450000000 * COIN;
        nMaxMoneyOut = 500000000 * COIN;

        /** Height or Time Based Activations **/
        nLastPOWBlock = 200;
        nModifierUpdateBlock = 345000;
        nZerocoinStartHeight = 12000;
        //nZerocoinStartTime = 1508214600; // October 17, 2017 4:30:00 AM
        nBlockEnforceSerialRange = -1; //Enforce serial range starting this block
        nBlockRecalculateAccumulators = nZerocoinStartHeight + 10; //Trigger a recalculation of accumulators
        nBlockFirstFraudulent = nZerocoinStartHeight; //First block that bad serials emerged
        nBlockLastGoodCheckpoint = nZerocoinStartHeight; //Last valid accumulator checkpoint
        nBlockEnforceInvalidUTXO = -1; //Start enforcing the invalid UTXO's
        nInvalidAmountFiltered = 0*COIN; //Amount of invalid coins filtered through exchanges, that should be considered valid
        nBlockZerocoinV2 = nModifierUpdateBlock + 20; //!> The block that zerocoin v2 becomes active - roughly Tuesday, May 8, 2018 4:00:00 AM GMT
        nEnforceNewSporkKey = 1525158000; //!> Sporks signed after (GMT): Tuesday, May 1, 2018 7:00:00 AM GMT must use the new spork key
        nRejectOldSporkKey = 1527811200; //!> Fully reject old spork key after (GMT): Friday, June 1, 2018 12:00:00 AM
        vTreasuryRewardAddress="XaU63hVi3dPzCcgXMzbFWbqmSCvzcysgnC";
        nStartTreasuryBlock = nModifierUpdateBlock;
        nTreasuryBlockStep = 1 * 24 * 60 * 60 / nTargetSpacing; // Once per day
        nMasternodeTiersStartHeight = nStartTreasuryBlock;
        nSecondForkBlock = 1200000;

        /**
         * Build the genesis block. Note that the output of the genesis coinbase cannot
         * be spent as it did not originally exist in the database.
         *
         * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
         *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
         *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
         *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
         *   vMerkleTree: e0028e
         */
        const char* pszTimestamp = "Any foolish boy can stamp on a beetle, but all the professors in the world cannot make a beetle";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 1 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("040f54c5893d68f990bdba4c5b9bc8f9eae59bb6df5ecb1fde548446e2292a9b514915ba867b8a9edcdced258ba8d16c3cdaf274d8896a645088fd86e4d75112d9") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime = 1536981458;
        genesis.nBits = 0x1e0ffff0;
        genesis.nNonce = 1510561;

        uint256 hashTarget = uint256().SetCompact(genesis.nBits);
        assert(genesis.GetPoWHash() <= hashTarget);

        hashGenesisBlock = genesis.GetHash();
        //printf("Merkle hash main: %s\n", genesis.hashMerkleRoot.ToString().c_str());
        //printf("Block hash main: %s\n", hashGenesisBlock.ToString().c_str());
        assert(genesis.hashMerkleRoot == uint256("0x64a35990d03a0a06b73a4ec8524ec98f315f5a0ce6b0682743374789c5da6557"));
        assert(hashGenesisBlock == uint256("0x00000c9d6ee5917dcd9e9d291f4b2283fce7d6b8525a653267bae3a1c5fbdd00"));

        vSeeds.push_back(CDNSSeedData("seedereu.beetlecoin.io", "seedereu.beetlecoin.io"));
        vSeeds.push_back(CDNSSeedData("seederch.beetlecoin.io", "seederch.beetlecoin.io"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 75); // X
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 85); // b
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 127); // t
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x02)(0x2D)(0x25)(0x73).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x02)(0x21)(0x31)(0x2B).convert_to_container<std::vector<unsigned char> >();
        //  BIP44 coin type is from https://github.com/satoshilabs/slips/blob/master/slip-0044.md
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x1d)(0xfc).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;
        fHeadersFirstSyncingActive = false;

        nPoolMaxTransactions = 3;
        strSporkKey = "04DE3E3D0380A7359563B990F7AF701320F44CEB0FBC325CD7EB06A6C228FE57D8448AD2365E7F36B31591B9B3BFCE6A5FE9A01773215604CB9DD512470AFBB9BB";
        strSporkKeyOld = "04DE3E3D0380A7359563B990F7AF701320F44CEB0FBC325CD7EB06A6C228FE57D8448AD2365E7F36B31591B9B3BFCE6A5FE9A01773215604CB9DD512470AFBB9BB";
        strObfuscationPoolDummyAddress = "XKCwyEbFpa9xTC1ontK3v2JzLns2Zc6UCJ";
        nStartMasternodePayments = 1403728576; //Wed, 25 Jun 2014 20:36:16 GMT

        /** Zerocoin */
        zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";
        nMaxZerocoinSpendsPerTransaction = 7; // Assume about 20kb each
        nMinZerocoinMintFee = 1 * CENT; //high fee required for zerocoin mints
        nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        nRequiredAccumulation = 1;
        nDefaultSecurityLevel = 100; //full security level for accumulators
        nZerocoinHeaderVersion = 3; //Block headers must be this version once zerocoin is active
        nZerocoinRequiredStakeDepth = 200; //The required confirmations for a zbeet to be stakable

        nBudget_Fee_Confirmations = 6; // Number of confirmations for the finalization fee
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return data;
    }
};

std::string CChainParams::GetTreasuryRewardAddressAtHeight(int nHeight) const {
    return vTreasuryRewardAddress;
}

CScript CChainParams::GetTreasuryRewardScriptAtHeight(int nHeight) const {
    CBitcoinAddress address(GetTreasuryRewardAddressAtHeight(nHeight).c_str());
    assert(address.IsValid());

    CScript script = GetScriptForDestination(address.Get());
    return script;
}

static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        pchMessageStart[0] = 0x43;
        pchMessageStart[1] = 0x76;
        pchMessageStart[2] = 0x65;
        pchMessageStart[3] = 0xba;
        vAlertPubKey = ParseHex("03c6a3b3881692505afeab25b0fa3e52e0f13109f51f94abd58fdd022d96a23f1f");
        nDefaultPort = 51434;
        bnProofOfWorkLimit = ~uint256(0) >> 12;
        nEnforceBlockUpgradeMajority = 4032; // 70%
        nRejectBlockOutdatedMajority = 4032; // 70%
        nToCheckBlockUpgradeMajority = 5760; // 4 days
        nMinerThreads = 0;
        nTargetTimespan = 1 * 24 * 60 * 60; // 1 day
        nTargetSpacing = 64; // 64 seconds
        nLastPOWBlock = 200;
        nMaturity = 15;
        nMasternodeCountDrift = 4;
        nModifierUpdateBlock = -1; //approx Mon, 17 Apr 2017 04:00:00 GMT
        nFirstSupplyReduction = 400000000 * COIN;
        nSecondSupplyReduction = 450000000 * COIN;
        nMaxMoneyOut = 500000000 * COIN;
        vTreasuryRewardAddress="yEz2MNkNQnBBNVzYiJJpNawMhH3yn7NY5p";
        nStartTreasuryBlock = 10;
        nTreasuryBlockStep = 20; //24 * 6 * 60 / nTargetSpacing; // Ten times per day
        nMasternodeTiersStartHeight = -1;
        nSecondForkBlock = 0;
        nZerocoinStartHeight = 2100000000;
        nBlockEnforceSerialRange = -1; //Enforce serial range starting this block
        nBlockRecalculateAccumulators = nZerocoinStartHeight + 10; //Trigger a recalculation of accumulators
        nBlockFirstFraudulent = nZerocoinStartHeight; //First block that bad serials emerged
        nBlockLastGoodCheckpoint = nZerocoinStartHeight; //Last valid accumulator checkpoint
        nBlockEnforceInvalidUTXO = -1; //Start enforcing the invalid UTXO's
        nInvalidAmountFiltered = 0; //Amount of invalid coins filtered through exchanges, that should be considered valid
        nBlockZerocoinV2 = nZerocoinStartHeight + 20; //!> The block that zerocoin v2 becomes active
        nEnforceNewSporkKey = 1521604800; //!> Sporks signed after Wednesday, March 21, 2018 4:00:00 AM GMT must use the new spork key
        nRejectOldSporkKey = 1522454400; //!> Reject old spork key after Saturday, March 31, 2018 12:00:00 AM GMT

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1536981458;
        genesis.nBits = bnProofOfWorkLimit.GetCompact(); //0x1f0fffff;
        genesis.nNonce = 2871;

        uint256 hashTarget = uint256().SetCompact(genesis.nBits);
        assert(genesis.GetPoWHash() <= hashTarget);

        hashGenesisBlock = genesis.GetHash();
        //printf("Merkle hash test: %s\n", genesis.hashMerkleRoot.ToString().c_str());
        //printf("Block hash test: %s\n", hashGenesisBlock.ToString().c_str());
        assert(genesis.hashMerkleRoot == uint256("0x64a35990d03a0a06b73a4ec8524ec98f315f5a0ce6b0682743374789c5da6557"));
        assert(hashGenesisBlock == uint256("0x000ecdc69e08dbe94ed34b7ac2eba035234ee12082ca2f764246f23a6e3e3af1"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // vSeeds.push_back(CDNSSeedData("45.76.61.28", "207.148.0.129"));         // Single node address
        // vSeeds.push_back(CDNSSeedData("209.250.240.94", "45.77.239.30"));       // Single node address
        // vSeeds.push_back(CDNSSeedData("45.77.176.204", "45.76.226.204"));       // Single node address

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 139); // Testnet beetlecoin addresses start with 'x' or 'y'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 19);  // Testnet beetlecoin script addresses start with '8' or '9'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);     // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        // Testnet beetlecoin BIP32 pubkeys start with 'DRKV'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x3a)(0x80)(0x61)(0xa0).convert_to_container<std::vector<unsigned char> >();
        // Testnet beetlecoin BIP32 prvkeys start with 'DRKP'
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x3a)(0x80)(0x58)(0x37).convert_to_container<std::vector<unsigned char> >();
        // Testnet beetlecoin BIP44 coin type is '1' (All coin's testnet default)
        base58Prefixes[EXT_COIN_TYPE] = boost::assign::list_of(0x80)(0x00)(0x00)(0x01).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 2;
        strSporkKey = "03c6a3b3881692505afeab25b0fa3e52e0f13109f51f94abd58fdd022d96a23f1f";
        strSporkKeyOld = "03c6a3b3881692505afeab25b0fa3e52e0f13109f51f94abd58fdd022d96a23f1f";
        strObfuscationPoolDummyAddress = "yBToNUFGJUSHKxiZkUMZc3dYrYbvWXgLEp";
        nStartMasternodePayments = 1420837558; //Fri, 09 Jan 2015 21:05:58 GMT
        nBudget_Fee_Confirmations = 3; // Number of confirmations for the finalization fee. We have to make this very short
                                       // here because we only have a 8 block finalization window on testnet
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        pchMessageStart[0] = 0x69;
        pchMessageStart[1] = 0xcf;
        pchMessageStart[2] = 0x7e;
        pchMessageStart[3] = 0xac;
        nDefaultPort = 51436;
        //nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 20 * 60; // 20 minutes
        nTargetSpacing = 48; // 48 seconds
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        genesis.nVersion = 5;
        genesis.nTime = 1454124731;
        genesis.nBits = bnProofOfWorkLimit.GetCompact(); //0x207fffff;
        genesis.nNonce = 1;

        uint256 hashTarget = uint256().SetCompact(genesis.nBits);
        /*while (true) {
            uint256 hash = genesis.GetPoWHash();
            if (hash <= hashTarget) {
                // Found a solution
                printf("genesis block found\n   hash: %s\n target: %s\n   bits: %08x\n  nonce: %u\n", hash.ToString().c_str(), hashTarget.ToString().c_str(), genesis.nBits, genesis.nNonce);
                break;
            }
            genesis.nNonce += 1;
            if ((genesis.nNonce & 0x1ffff) == 0)
                printf("testing nonce: %u\n", genesis.nNonce);
        }*/
        assert(genesis.GetPoWHash() <= hashTarget);

        hashGenesisBlock = genesis.GetHash();
        //printf("Merkle hash reg: %s\n", genesis.hashMerkleRoot.ToString().c_str());
        //printf("Block hash reg: %s\n", hashGenesisBlock.ToString().c_str());
        assert(genesis.hashMerkleRoot == uint256("0x64a35990d03a0a06b73a4ec8524ec98f315f5a0ce6b0682743374789c5da6557"));
        assert(hashGenesisBlock == uint256("0x1916619e80782a2c6c345cbe4c794ff43565eedc57e1d0b047c4cc0d4234b449"));

        vFixedSeeds.clear(); //! Testnet mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Testnet mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

/**
 * Unit test
 */
class CUnitTestParams : public CMainParams, public CModifiableParams
{
public:
    CUnitTestParams()
    {
        networkID = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        nDefaultPort = 51478;
        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();      //! Unit test mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fAllowMinDifficultyBlocks = false;
        fMineBlocksOnDemand = true;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const
    {
        // UnitTest share the same checkpoints as MAIN
        return data;
    }

    //! Published setters to allow changing values in unit test cases
    //virtual void setSubsidyHalvingInterval(int anSubsidyHalvingInterval) { nSubsidyHalvingInterval = anSubsidyHalvingInterval; }
    virtual void setEnforceBlockUpgradeMajority(int anEnforceBlockUpgradeMajority) { nEnforceBlockUpgradeMajority = anEnforceBlockUpgradeMajority; }
    virtual void setRejectBlockOutdatedMajority(int anRejectBlockOutdatedMajority) { nRejectBlockOutdatedMajority = anRejectBlockOutdatedMajority; }
    virtual void setToCheckBlockUpgradeMajority(int anToCheckBlockUpgradeMajority) { nToCheckBlockUpgradeMajority = anToCheckBlockUpgradeMajority; }
    virtual void setDefaultConsistencyChecks(bool afDefaultConsistencyChecks) { fDefaultConsistencyChecks = afDefaultConsistencyChecks; }
    virtual void setAllowMinDifficultyBlocks(bool afAllowMinDifficultyBlocks) { fAllowMinDifficultyBlocks = afAllowMinDifficultyBlocks; }
    virtual void setSkipProofOfWorkCheck(bool afSkipProofOfWorkCheck) { fSkipProofOfWorkCheck = afSkipProofOfWorkCheck; }
};
static CUnitTestParams unitTestParams;


static CChainParams* pCurrentParams = 0;

CModifiableParams* ModifiableParams()
{
    assert(pCurrentParams);
    assert(pCurrentParams == &unitTestParams);
    return (CModifiableParams*)&unitTestParams;
}

const CChainParams& Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        return mainParams;
    case CBaseChainParams::TESTNET:
        return testNetParams;
    case CBaseChainParams::REGTEST:
        return regTestParams;
    case CBaseChainParams::UNITTEST:
        return unitTestParams;
    default:
        assert(false && "Unimplemented network");
        return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
