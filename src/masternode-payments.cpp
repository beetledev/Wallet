// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2019 The BeetleCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternode-payments.h"
#include "addrman.h"
#include "masternode-budget.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "obfuscation.h"
#include "spork.h"
#include "sync.h"
#include "util.h"
#include "utilmoneystr.h"
#include <boost/filesystem.hpp>

/** Object for who's going to get paid on which blocks */
CMasternodePayments masternodePayments;

CCriticalSection cs_vecPayments;
CCriticalSection cs_mapMasternodeBlocks;
CCriticalSection cs_mapMasternodePayeeVotes;

//

// CMasternodePaymentDB
//

CMasternodePaymentDB::CMasternodePaymentDB()
{
    pathDB = GetDataDir() / "mnpayments.dat";
    strMagicMessage = "MasternodePayments";
}

bool CMasternodePaymentDB::Write(const CMasternodePayments& objToSave)
{
    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssObj(SER_DISK, CLIENT_VERSION);
    ssObj << strMagicMessage;                   // masternode cache file specific magic message
    ssObj << FLATDATA(Params().MessageStart()); // network specific magic number
    ssObj << objToSave;
    uint256 hash = Hash(ssObj.begin(), ssObj.end());
    ssObj << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fopen(pathDB.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathDB.string());

    // Write and commit header, data
    try {
        fileout << ssObj;
    } catch (std::exception& e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    fileout.fclose();

    LogPrint("masternode","Written info to mnpayments.dat  %dms\n", GetTimeMillis() - nStart);

    return true;
}

CMasternodePaymentDB::ReadResult CMasternodePaymentDB::Read(CMasternodePayments& objToLoad, bool fDryRun)
{
    int64_t nStart = GetTimeMillis();
    // open input file, and associate with CAutoFile
    FILE* file = fopen(pathDB.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        error("%s : Failed to open file %s", __func__, pathDB.string());
        return FileError;
    }

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(pathDB);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char*)&vchData[0], dataSize);
        filein >> hashIn;
    } catch (std::exception& e) {
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return HashReadError;
    }
    filein.fclose();

    CDataStream ssObj(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssObj.begin(), ssObj.end());
    if (hashIn != hashTmp) {
        error("%s : Checksum mismatch, data corrupted", __func__);
        return IncorrectHash;
    }

    unsigned char pchMsgTmp[4];
    std::string strMagicMessageTmp;
    try {
        // de-serialize file header (masternode cache file specific magic message) and ..
        ssObj >> strMagicMessageTmp;

        // ... verify the message matches predefined one
        if (strMagicMessage != strMagicMessageTmp) {
            error("%s : Invalid masternode payement cache magic message", __func__);
            return IncorrectMagicMessage;
        }


        // de-serialize file header (network specific magic number) and ..
        ssObj >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp))) {
            error("%s : Invalid network magic number", __func__);
            return IncorrectMagicNumber;
        }

        // de-serialize data into CMasternodePayments object
        ssObj >> objToLoad;
    } catch (std::exception& e) {
        objToLoad.Clear();
        error("%s : Deserialize or I/O error - %s", __func__, e.what());
        return IncorrectFormat;
    }

    LogPrint("masternode","Loaded info from mnpayments.dat  %dms\n", GetTimeMillis() - nStart);
    LogPrint("masternode","  %s\n", objToLoad.ToString());
    if (!fDryRun) {
        LogPrint("masternode","Masternode payments manager - cleaning....\n");
        objToLoad.CleanPaymentList();
        LogPrint("masternode","Masternode payments manager - result:\n");
        LogPrint("masternode","  %s\n", objToLoad.ToString());
    }

    return Ok;
}

void DumpMasternodePayments()
{
    int64_t nStart = GetTimeMillis();

    CMasternodePaymentDB paymentdb;
    CMasternodePayments tempPayments;

    LogPrint("masternode","Verifying mnpayments.dat format...\n");
    CMasternodePaymentDB::ReadResult readResult = paymentdb.Read(tempPayments, true);
    // there was an error and it was not an error on file opening => do not proceed
    if (readResult == CMasternodePaymentDB::FileError)
        LogPrint("masternode","Missing budgets file - mnpayments.dat, will try to recreate\n");
    else if (readResult != CMasternodePaymentDB::Ok) {
        LogPrint("masternode","Error reading mnpayments.dat: ");
        if (readResult == CMasternodePaymentDB::IncorrectFormat)
            LogPrint("masternode","magic is ok but data has invalid format, will try to recreate\n");
        else {
            LogPrint("masternode","file format is unknown or invalid, please fix it manually\n");
            return;
        }
    }
    LogPrint("masternode","Writting info to mnpayments.dat...\n");
    paymentdb.Write(masternodePayments);

    LogPrint("masternode","Budget dump finished  %dms\n", GetTimeMillis() - nStart);
}

bool IsBlockValueValid(const CBlock& block, CAmount nExpectedValue, CAmount nMinted)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL)
        return true;

    int nHeight = 0;
    if (pindexPrev->GetBlockHash() == block.hashPrevBlock) {
        nHeight = pindexPrev->nHeight + 1;
    } else { //out of order
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi != mapBlockIndex.end() && (*mi).second)
            nHeight = (*mi).second->nHeight + 1;
    }

    if (nHeight == 0) {
        LogPrint("masternode","IsBlockValueValid() : WARNING: Couldn't find previous block\n");
    }

    //LogPrintf("XX69----------> IsBlockValueValid(): nMinted: %d, nExpectedValue: %d\n", FormatMoney(nMinted), FormatMoney(nExpectedValue));

    //check if it's valid treasury block
    if (IsTreasuryBlock(nHeight)) {
        const CTransaction& txNew = block.IsProofOfStake() ? block.vtx[1] : block.vtx[0];
        CScript treasuryPayee = Params().GetTreasuryRewardScriptAtHeight(nHeight);
        //CAmount blockValue = GetBlockValue(nHeight-1);
        CAmount treasuryAmount = GetTreasuryAward(nHeight);

        bool bFound = false;

        for (CTxOut out : txNew.vout) {
            if (out.nValue == treasuryAmount && out.scriptPubKey == treasuryPayee) {
                bFound = true; //We found our treasury payment, let's end it here.
                break;
            }
        }

        if (!bFound) {
            LogPrint("masternode","Invalid treasury payment detected %s\n", txNew.ToString().c_str());
            if (block.nTime > GetSporkValue(SPORK_17_TREASURY_PAYMENT_ENFORCEMENT)) { //IsSporkActive(SPORK_17_TREASURY_PAYMENT_ENFORCEMENT)
                return false;
            } else {
                LogPrint("masternode","Treasury enforcement is not enabled, accept anyway\n");
            }
        } else {
            LogPrint("masternode","Valid treasury payment detected %s\n", txNew.ToString().c_str());
        }
    }

    if (!masternodeSync.IsSynced()) { //there is no budget data to use to check anything
        //super blocks will always be on these blocks, max 100 per budgeting
        if (nHeight % GetBudgetPaymentCycleBlocks() < 100) {
            return true;
        } else {
            if (nMinted > nExpectedValue) {
                return false;
            }
        }
    } else { // we're synced and have data so check the budget schedule

        //are these blocks even enabled
        if (!IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)) {
            return nMinted <= nExpectedValue;
        }

        if (budget.IsBudgetPaymentBlock(nHeight)) {
            //the value of the block is evaluated in CheckBlock
            return true;
        } else {
            if (nMinted > nExpectedValue) {
                return false;
            }
        }
    }

    return true;
}

bool IsBlockPayeeValid(const CBlock& block, int nBlockHeight)
{
    //static int64_t startTime = 0;
    //
    //if (startTime == 0)
    //    startTime = GetTime();

    TrxValidationStatus transactionStatus = TrxValidationStatus::InValid;

    if (!masternodeSync.IsSynced()) { //there is no budget data to use to check anything -- find the longest chain
        LogPrint("mnpayments", "Client not synced, skipping block payee checks\n");
        return true;
    }

    const CTransaction& txNew = block.IsProofOfStake() ? block.vtx[1] : block.vtx[0];

    //check if it's a budget block
    if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)) {
        if (budget.IsBudgetPaymentBlock(nBlockHeight)) {
            transactionStatus = budget.IsTransactionValid(txNew, nBlockHeight);
            if (transactionStatus == TrxValidationStatus::Valid) {
                return true;
            }

            if (transactionStatus == TrxValidationStatus::InValid) {
                LogPrint("masternode","Invalid budget payment detected %s\n", txNew.ToString().c_str());
                if (IsSporkActive(SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT))
                    return false;

                LogPrint("masternode","Budget enforcement is disabled, accepting block\n");
            }
        }
    }

    // If we end here the transaction was either TrxValidationStatus::InValid and Budget enforcement is disabled, or
    // a double budget payment (status = TrxValidationStatus::DoublePayment) was detected, or no/not enough masternode
    // votes (status = TrxValidationStatus::VoteThreshold) for a finalized budget were found
    // In all cases a masternode will get the payment for this block

    if (!IsTreasuryBlock(nBlockHeight)) {
        //check for masternode payee
        if (masternodePayments.IsTransactionValid(txNew, nBlockHeight))
            return true;

        LogPrint("masternode","Invalid mn payment detected %s\n", txNew.ToString().c_str());

        // If the Spork 8 is active I also check if is passed 1h from the node start
        // to prevent the prevent the block rejection when the mastermode list isn't complete at the node start.
        if (IsSporkActive(SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT) /*&& GetTime()-startTime > 3600*/)
            return false;

        LogPrint("masternode","Masternode payment enforcement is disabled, accepting block\n");
    }

    return true;
}


void FillBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake, bool fZBEETStake)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && budget.IsBudgetPaymentBlock(pindexPrev->nHeight + 1)) {
        budget.FillBlockPayee(txNew, nFees, fProofOfStake);
    } else if (IsTreasuryBlock(pindexPrev->nHeight + 1)) {
        budget.FillTreasuryBlockPayee(txNew, nFees, fProofOfStake);
    } else {
        masternodePayments.FillBlockPayee(txNew, nFees, fProofOfStake, fZBEETStake);
    }
}

std::string GetRequiredPaymentsString(int nBlockHeight)
{
    if (IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS) && budget.IsBudgetPaymentBlock(nBlockHeight)) {
        return budget.GetRequiredPaymentsString(nBlockHeight);
    } else {
        return masternodePayments.GetRequiredPaymentsString(nBlockHeight);
    }
}

void CMasternodePayments::FillBlockPayee(CMutableTransaction& txNew, int64_t nFees, bool fProofOfStake, bool fZBEETStake)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return;

    bool payNewTiers = IsSporkActive(SPORK_18_NEW_MASTERNODE_TIERS);
    unsigned int level = CMasternode::LevelValue::MIN; //1;
    CAmount mn_payments_total = 0;

    for (unsigned mnlevel = payNewTiers ? CMasternode::LevelValue::MIN : CMasternode::LevelValue::MAX; mnlevel <= CMasternode::LevelValue::MAX; ++mnlevel) {
        bool hasPayment = true;
        CScript payee;

        //spork
        if (!masternodePayments.GetBlockPayee(pindexPrev->nHeight + 1, mnlevel, payee)) {
            //no masternode detected
            CMasternode* winningNode = mnodeman.GetCurrentMasterNode(mnlevel, 1);
            if (winningNode) {
                payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
            } else {
                LogPrint("masternode","CreateNewBlock: Failed to detect masternode level %d to pay\n", mnlevel);
                hasPayment = false;
            }
        }

        CAmount blockValue = GetBlockValue(pindexPrev->nHeight);
        CAmount masternodePayment = GetMasternodePayment(pindexPrev->nHeight, mnlevel, blockValue);

        if (hasPayment) {
            if (fProofOfStake) {
                /**For Proof Of Stake vout[0] must be null
                 * Stake reward can be split into many different outputs, so we must
                 * use vout.size() to align with several different cases.
                 * An additional output is appended as the masternode payment
                 */
                unsigned int i = txNew.vout.size();
                txNew.vout.resize(i + 1);
                txNew.vout[i].scriptPubKey = payee;
                txNew.vout[i].nValue = masternodePayment;

                //subtract mn payment from the stake reward
                if (!txNew.vout[1].IsZerocoinMint())
                    txNew.vout[i - level].nValue -= masternodePayment;
            } else {
                txNew.vout.resize(1 + level);
                txNew.vout[level].scriptPubKey = payee;
                txNew.vout[level].nValue = masternodePayment;
                if (level == 1)
                    txNew.vout[0].nValue = blockValue - masternodePayment;
                else
                    txNew.vout[0].nValue -= masternodePayment;
            }

            mn_payments_total += masternodePayment;
            CTxDestination address1;
            ExtractDestination(payee, address1);
            CBitcoinAddress address2(address1);

            if (payNewTiers)
                level++;

            LogPrint("masternode","Masternode payment of %s to %s\n", FormatMoney(masternodePayment).c_str(), address2.ToString().c_str());
        } else {
            if (!fProofOfStake)
                txNew.vout[0].nValue = blockValue;
        }
    }
}

int CMasternodePayments::GetMinMasternodePaymentsProto()
{
    if (IsSporkActive(SPORK_10_MASTERNODE_PAY_UPDATED_NODES))
        return ActiveProtocol();                          // Allow only updated peers
    else
        return MIN_PEER_PROTO_VERSION_BEFORE_ENFORCEMENT; // Also allow old peers as long as they are allowed to run
}

void CMasternodePayments::ProcessMessageMasternodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (!masternodeSync.IsBlockchainSynced()) return;

    if (fLiteMode) return; //disable all Obfuscation/Masternode related functionality

    if (strCommand == "mnget") { //Masternode Payments Request Sync
        if (fLiteMode) return;   //disable all Obfuscation/Masternode related functionality

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (pfrom->HasFulfilledRequest("mnget")) {
                LogPrintf("CMasternodePayments::ProcessMessageMasternodePayments() : mnget - peer=%i ip=%s already asked me for the list\n", pfrom->GetId(), pfrom->addr.ToString().c_str());
                Misbehaving(pfrom->GetId(), 20);

                // The old node ban at 20% and don't answer at the request, but the new node always ban at 20% but reply at the request
                if( pfrom->nVersion < MIN_PEER_PROTO_VERSION_AFTER_ENFORCEMENT_4 )
                    return;
            }
        }

        pfrom->FulfilledRequest("mnget");
        masternodePayments.Sync(pfrom, nCountNeeded);
        LogPrint("mnpayments", "mnget - Sent Masternode winners to peer %i\n", pfrom->GetId());
    } else if (strCommand == "mnw") { //Masternode Payments Declare Winner
        //this is required in litemode
        CMasternodePaymentWinner winner;
        vRecv >> winner;

        if (pfrom->nVersion < ActiveProtocol()) return;

        int nHeight;
        {
            TRY_LOCK(cs_main, locked);
            if (!locked || chainActive.Tip() == NULL) return;
            nHeight = chainActive.Tip()->nHeight;
        }

        CTxDestination address1;
        ExtractDestination(winner.payee, address1);
        CBitcoinAddress payee_addr(address1);

        auto winner_mn = mnodeman.Find(winner.payee);

        if (!winner_mn) {
            LogPrint("mnpayments", "mnw - unknown payee from peer=%s ip=%s - %s\n", pfrom->GetId(), pfrom->addr.ToString().c_str(), payee_addr.ToString().c_str());

            // If I received an unknown payee I try to ask to the peer the updaded version of the masternode list
            // however the DsegUpdate function do that only 1 time every 3h
            mnodeman.DsegUpdate(pfrom);
            return;
        }

        winner.payeeLevel = winner_mn->Level();

        if (masternodePayments.mapMasternodePayeeVotes.count(winner.GetHash())) {
            LogPrint("mnpayments", "mnw - Already seen from peer=%s ip=%s - %s bestHeight %d\n", pfrom->GetId(), pfrom->addr.ToString().c_str(), winner.GetHash().ToString().c_str(), nHeight);
            masternodeSync.AddedMasternodeWinner(winner.GetHash());
            return;
        }

        int nFirstBlock = nHeight - (mnodeman.CountEnabled(winner.payeeLevel) * 1.25);
        if (winner.nBlockHeight < nFirstBlock || winner.nBlockHeight > nHeight + 20) {
            LogPrint("mnpayments", "mnw - winner out of range from peer=%s ip=%s - Addr %s FirstBlock %d Height %d bestHeight %d\n", pfrom->GetId(), pfrom->addr.ToString().c_str(), payee_addr.ToString().c_str(), nFirstBlock, winner.nBlockHeight, nHeight);
            return;
        }

        std::string strError = "";
        if (!winner.IsValid(pfrom, strError)) {
            if(strError != "") LogPrint("mnpayments", "mnw - invalid message from peer=%s ip=%s - %s\n", pfrom->GetId(), pfrom->addr.ToString().c_str(), strError);
            return;
        }

        if (!masternodePayments.CanVote(winner.vinMasternode.prevout, winner.nBlockHeight, winner.payeeLevel)) {
            LogPrint("mnpayments", "mnw - masternode already voted from peer=%s ip=%s - %s\n", pfrom->GetId(), pfrom->addr.ToString().c_str(), winner.vinMasternode.prevout.ToStringShort());
            return;
        }

        if (!winner.SignatureValid()) {
            if (masternodeSync.IsSynced()) {
                LogPrintf("CMasternodePayments::ProcessMessageMasternodePayments() : mnw - invalid signature from peer=%s ip=%s\n", pfrom->GetId(), pfrom->addr.ToString().c_str());
                Misbehaving(pfrom->GetId(), 20);
            }
            // it could just be a non-synced masternode
            mnodeman.AskForMN(pfrom, winner.vinMasternode);
            return;
        }

        LogPrint("mnpayments", "mnw - winning vote from peer=%s ip=%s v=%s - Addr %s Height %d bestHeight %d - %s\n", pfrom->GetId(), pfrom->addr.ToString().c_str(), pfrom->nVersion, payee_addr.ToString().c_str(), winner.nBlockHeight, nHeight, winner.vinMasternode.prevout.ToStringShort());

        if (masternodePayments.AddWinningMasternode(winner)) {
            winner.Relay();
            masternodeSync.AddedMasternodeWinner(winner.GetHash());
        }
    }
}

bool CMasternodePaymentWinner::Sign(CKey& keyMasternode, CPubKey& pubKeyMasternode)
{
    std::string errorMessage;
    std::string strMasterNodeSignMessage;

    std::string strMessage = vinMasternode.prevout.ToStringShort() + std::to_string(nBlockHeight) + payee.ToString();

    if (!obfuScationSigner.SignMessage(strMessage, errorMessage, vchSig, keyMasternode)) {
        LogPrint("masternode","CMasternodePing::Sign() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    if (!obfuScationSigner.VerifyMessage(pubKeyMasternode, vchSig, strMessage, errorMessage)) {
        LogPrint("masternode","CMasternodePing::Sign() - Error: %s\n", errorMessage.c_str());
        return false;
    }

    return true;
}

bool CMasternodePayments::GetBlockPayee(int nBlockHeight, unsigned mnlevel, CScript& payee)
{
    if (mapMasternodeBlocks.count(nBlockHeight)) {
        return mapMasternodeBlocks[nBlockHeight].GetPayee(mnlevel, payee);
    }

    return false;
}

// Is this masternode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 winners
bool CMasternodePayments::IsScheduled(CMasternode& mn, int nSameLevelMNCount, int nNotBlockHeight)
{
    LOCK(cs_mapMasternodeBlocks);

    int64_t nHeight;
    {
        TRY_LOCK(cs_main, locked);

        if (!locked)
            return false;

        auto chain_tip = chainActive.Tip();

        if(!chain_tip)
            return false;

        nHeight = chain_tip->nHeight;
    }

    CScript mnpayee = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());

    for(int64_t h = nHeight; h <= nHeight + 8; ++h) {

        if(h == nNotBlockHeight)
            continue;

        auto block_payees = mapMasternodeBlocks.find(h);

        if(block_payees == mapMasternodeBlocks.cend())
            continue;

        CScript payee;

        if(!block_payees->second.GetPayee(mn.Level(), payee))
            continue;

        if(mnpayee == payee)
            return true;
    }

    return false;
}

bool CMasternodePayments::AddWinningMasternode(CMasternodePaymentWinner& winnerIn)
{
    uint256 blockHash = 0;

    if (!GetBlockHash(blockHash, winnerIn.nBlockHeight - 100)) {
        return false;
    }

    {
        LOCK2(cs_mapMasternodePayeeVotes, cs_mapMasternodeBlocks);

        if (mapMasternodePayeeVotes.count(winnerIn.GetHash())) {
            return false;
        }

        mapMasternodePayeeVotes[winnerIn.GetHash()] = winnerIn;

        if (!mapMasternodeBlocks.count(winnerIn.nBlockHeight)) {
            CMasternodeBlockPayees blockPayees(winnerIn.nBlockHeight);
            mapMasternodeBlocks[winnerIn.nBlockHeight] = blockPayees;
        }
    }

    mapMasternodeBlocks[winnerIn.nBlockHeight].AddPayee(winnerIn.payeeLevel, winnerIn.payee, 1);

    return true;
}

bool CMasternodeBlockPayees::IsTransactionValid(const CTransaction& txNew)
{
    LOCK(cs_vecPayments);

    std::map<unsigned, int> max_signatures;
    bool payNewTiers = IsSporkActive(SPORK_18_NEW_MASTERNODE_TIERS);

    // require at least 6 signatures
    for (CMasternodePayee& payee : vecPayments) {
        if (payee.nVotes < MNPAYMENTS_SIGNATURES_REQUIRED || (!payNewTiers && payee.mnlevel != CMasternode::LevelValue::MAX))
            continue;

        auto ins_res = max_signatures.emplace(payee.mnlevel, payee.nVotes);

        if (ins_res.second)
            continue;

        if (payee.nVotes > ins_res.first->second)
            ins_res.first->second = payee.nVotes;
    }

    // if we don't have at least 6 signatures on a payee, approve whichever is the longest chain
    if (!max_signatures.size()) {
        LogPrint("mnpayments","CMasternodePayments::IsTransactionValid - No enougth signature, accepting\n");
        return true;
    }

    CAmount nReward = GetBlockValue(nBlockHeight);

    std::string strPayeesPossible;

    for (const CMasternodePayee& payee : vecPayments) {
        auto requiredMasternodePayment = GetMasternodePayment(nBlockHeight, payee.mnlevel, nReward);

        if (strPayeesPossible != "")
            strPayeesPossible += ",";

        CTxDestination address1;
        ExtractDestination(payee.scriptPubKey, address1);

        strPayeesPossible += std::to_string(payee.mnlevel) + ":" + CBitcoinAddress(address1).ToString() + "(" + std::to_string(payee.nVotes) + ")=" + FormatMoney(requiredMasternodePayment).c_str();

        if (payee.nVotes < MNPAYMENTS_SIGNATURES_REQUIRED || (!payNewTiers && payee.mnlevel != CMasternode::LevelValue::MAX)) {
            LogPrint("mnpayments","CMasternodePayments::IsTransactionValid - Payment level %d found to %s vote=%d **\n", payee.mnlevel, CBitcoinAddress(address1).ToString(), payee.nVotes);
            continue;
        }

        auto payee_out = std::find_if(txNew.vout.cbegin(), txNew.vout.cend(), [&payee, &requiredMasternodePayment](const CTxOut& out){
            auto is_payee          = payee.scriptPubKey == out.scriptPubKey;
            auto is_value_required = out.nValue >= requiredMasternodePayment;

            if (is_payee && !is_value_required)
                LogPrintf("Masternode payment is out of drift range. Paid=%s Min=%s\n", FormatMoney(out.nValue).c_str(), FormatMoney(requiredMasternodePayment).c_str());

            return is_payee && is_value_required;
        });

        if (payee_out != txNew.vout.cend()) {
            auto it = max_signatures.find(payee.mnlevel);
            if (it != max_signatures.end())
                max_signatures.erase(payee.mnlevel);

            LogPrint("mnpayments","CMasternodePayments::IsTransactionValid - Payment level %d found to %s vote=%d\n", payee.mnlevel, CBitcoinAddress(address1).ToString(), payee.nVotes);

            if (max_signatures.size())
                continue;

            LogPrint("mnpayments","CMasternodePayments::IsTransactionValid - Payment accepted to %s\n", strPayeesPossible.c_str());
            return true;
        }

        LogPrint("mnpayments","CMasternodePayments::IsTransactionValid - Payment level %d NOT found to %s vote=%d\n", payee.mnlevel, CBitcoinAddress(address1).ToString(), payee.nVotes);
    }

    LogPrint("mnpayments","CMasternodePayments::IsTransactionValid - Missing required payment to %s\n", strPayeesPossible.c_str());
    LogPrint("mnpayments","CMasternodePayments::IsTransactionValid - TX Contents:\n");
    for (auto it = std::begin(txNew.vout); it != std::end(txNew.vout); ++it) {
        CTxDestination address1;
        ExtractDestination((*it).scriptPubKey, address1);
        LogPrint("mnpayments","    Address %s Value %s\n", CBitcoinAddress(address1).ToString(), FormatMoney((*it).nValue).c_str());
    }

    // If I haven't found the valid winners I try to ask to the other peers the list of updated masternode winners list using the "mnget" message
    // this is done only if is passed at least 15min from the last attempt to prevent the flooding of the messages
    // and only to the updated nodes, if I already asked the same before, or to all nodes that I haven't asked before
    // to avoid to be banned from the old nodes. The new nodes permit that for max 5 times before the ban
    static int64_t lastAttempt = 0;

    if (GetTime()-lastAttempt > (60 * 15)) {
        TRY_LOCK(cs_vNodes, lockRecv);

        if (lockRecv) {
            lastAttempt = GetTime();

            for (CNode* pnode : vNodes) {
                if( pnode->nVersion < MIN_PEER_PROTO_VERSION_AFTER_ENFORCEMENT_4 && pnode->HasFulfilledRequest("mnget") )
                    continue;

                int needed = mnodeman.CountEnabled();

                pnode->ClearFulfilledRequest("mnget");

                LogPrint("mnpayments","Sending mnget: peer=%d ip=%s needed=%d\n", pnode->id, pnode->addr.ToString().c_str(), needed);
                pnode->PushMessage("mnget", needed); //sync payees
            }
        }
    }

    return (IsSporkActive(SPORK_21_NEW_PROTOCOL_ENFORCEMENT_4) ? false : true);
}

std::string CMasternodeBlockPayees::GetRequiredPaymentsString()
{
    LOCK(cs_vecPayments);

    std::string ret = "Unknown";

    for(CMasternodePayee& payee : vecPayments) {
        CTxDestination address1;
        ExtractDestination(payee.scriptPubKey, address1);
        CBitcoinAddress address2(address1);

        std::string payee_str = address2.ToString() + ":"
                              + std::to_string(payee.mnlevel) + ":"
                              + std::to_string(payee.nVotes);

        if (ret != "Unknown") {
            ret += "," + payee_str;
        } else {
            ret = payee_str;
        }
    }

    return ret;
}

std::string CMasternodePayments::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs_mapMasternodeBlocks);

    if (mapMasternodeBlocks.count(nBlockHeight)) {
        return mapMasternodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CMasternodePayments::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs_mapMasternodeBlocks);

    if (mapMasternodeBlocks.count(nBlockHeight)) {
        return mapMasternodeBlocks[nBlockHeight].IsTransactionValid(txNew);
    }

    return true;
}

void CMasternodePayments::CleanPaymentList()
{
    LOCK2(cs_mapMasternodePayeeVotes, cs_mapMasternodeBlocks);

    int nHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked || !chainActive.Tip()) return;
        nHeight = chainActive.Tip()->nHeight;
    }

    //keep up to five cycles for historical sake
    int nLimit = std::max((int)(mnodeman.size() * 1.25), 1000);

    std::map<uint256, CMasternodePaymentWinner>::iterator it = mapMasternodePayeeVotes.begin();
    while (it != mapMasternodePayeeVotes.end()) {
        CMasternodePaymentWinner winner = (*it).second;

        if (nHeight - winner.nBlockHeight > nLimit) {
            LogPrint("mnpayments", "CMasternodePayments::CleanPaymentList - Removing old Masternode payment - block %d\n", winner.nBlockHeight);
            masternodeSync.mapSeenSyncMNW.erase((*it).first);
            mapMasternodePayeeVotes.erase(it++);
            mapMasternodeBlocks.erase(winner.nBlockHeight);
        } else {
            ++it;
        }
    }
}

bool CMasternodePaymentWinner::IsValid(CNode* pnode, std::string& strError)
{
    CMasternode* pmn = mnodeman.Find(vinMasternode);

    if (!pmn) {
        strError = strprintf("Unknown Masternode %s", vinMasternode.prevout.hash.ToString());
        LogPrint("masternode","CMasternodePaymentWinner::IsValid - %s\n", strError);
        mnodeman.AskForMN(pnode, vinMasternode);
        return false;
    }

    if (pmn->protocolVersion < ActiveProtocol()) {
        strError = strprintf("Masternode protocol too old %d - req %d", pmn->protocolVersion, ActiveProtocol());
        LogPrint("masternode","CMasternodePaymentWinner::IsValid - %s\n", strError);
        return false;
    }

    int n = mnodeman.GetMasternodeRank(vinMasternode, nBlockHeight - 100, ActiveProtocol());

    if (n == -1) {
        strError = strprintf("Unknown Masternode (rank==-1) %s", vinMasternode.prevout.hash.ToString());
        LogPrint("masternode","CMasternodePaymentWinner::IsValid - %s\n", strError);
        return false;
    }

    if (n > MNPAYMENTS_SIGNATURES_TOTAL) {
        //It's common to have masternodes mistakenly think they are in the top 10
        // We don't want to print all of these messages, or punish them unless they're way off
        if (n > MNPAYMENTS_SIGNATURES_TOTAL * 2) {
            strError = strprintf("Masternode not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL * 2, n);
            LogPrint("masternode","CMasternodePaymentWinner::IsValid - %s\n", strError);

            if (IsSporkActive(SPORK_21_NEW_PROTOCOL_ENFORCEMENT_4) && masternodeSync.IsSynced())
                Misbehaving(pnode->GetId(), 20);
        }
        return false;
    }

    return true;
}

bool CMasternodePayments::ProcessBlock(int nBlockHeight)
{
    if (!fMasterNode)
        return false;

    //reference node - hybrid mode

    if (nBlockHeight <= nLastBlockHeight)
        return false;

    int n = mnodeman.GetMasternodeRank(activeMasternode.vin, nBlockHeight - 100, ActiveProtocol());

    if (n == -1) {
        LogPrint("mnpayments", "CMasternodePayments::ProcessBlock - Unknown Masternode\n");
        return false;
    }

    if (n > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrint("mnpayments", "CMasternodePayments::ProcessBlock - Masternode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, n);
        return false;
    }

    LogPrintf("CMasternodePayments::ProcessBlock() Start nHeight %d - vin %s. \n", nBlockHeight, activeMasternode.vin.prevout.hash.ToString());
    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough

    std::string errorMessage;
    CPubKey pubKeyMasternode;
    CKey keyMasternode;

    if (!obfuScationSigner.SetKey(strMasterNodePrivKey, errorMessage, keyMasternode, pubKeyMasternode)) {
        LogPrintf("CMasternodePayments::ProcessBlock() - Error upon calling SetKey: %s\n", errorMessage.c_str());
        return false;
    }

    std::vector<CMasternodePaymentWinner> winners;

    if (budget.IsBudgetPaymentBlock(nBlockHeight)) {
        //is budget payment block -- handled by the budgeting software
    } else {
        for (unsigned mnlevel = CMasternode::LevelValue::MIN; mnlevel <= CMasternode::LevelValue::MAX; ++mnlevel) {
            CMasternodePaymentWinner newWinner(activeMasternode.vin);

            unsigned nCount = 0;

            CMasternode* pmn = mnodeman.GetNextMasternodeInQueueForPayment(nBlockHeight, mnlevel, true, nCount);

            if (!pmn) {
                LogPrintf("CMasternodePayments::ProcessBlock() Failed to find masternode level %d to pay\n", mnlevel);
                continue;
            }

            CScript payee = GetScriptForDestination(pmn->pubKeyCollateralAddress.GetID());

            newWinner.nBlockHeight = nBlockHeight;
            newWinner.AddPayee(payee, mnlevel);

            CTxDestination address1;
            ExtractDestination(payee, address1);
            CBitcoinAddress address2(address1);

            LogPrintf("CMasternodePayments::ProcessBlock() Winner payee %s nHeight %d level %d. \n", address2.ToString().c_str(), newWinner.nBlockHeight, mnlevel);

            LogPrintf("CMasternodePayments::ProcessBlock() - Signing Winner level %d\n", mnlevel);

            if (!newWinner.Sign(keyMasternode, pubKeyMasternode))
                continue;

            LogPrintf("CMasternodePayments::ProcessBlock() - AddWinningMasternode level %d\n", mnlevel);

            if (!AddWinningMasternode(newWinner))
                continue;

            winners.emplace_back(newWinner);
        }
    }

    if (winners.empty())
        return false;

    for (auto& winner : winners) {
        winner.Relay();
    }

    nLastBlockHeight = nBlockHeight;

    return true;
}

void CMasternodePaymentWinner::Relay()
{
    //LogPrint("mnpayments", "CMasternodePayments::Relay - %s\n", ToString().c_str());

    CInv inv(MSG_MASTERNODE_WINNER, GetHash());
    RelayInv(inv);
}

bool CMasternodePaymentWinner::SignatureValid()
{
    CMasternode* pmn = mnodeman.Find(vinMasternode);

    if (pmn != NULL) {
        std::string strMessage = vinMasternode.prevout.ToStringShort() +
                                 std::to_string(nBlockHeight) +
                                 payee.ToString();

        std::string errorMessage = "";
        if (!obfuScationSigner.VerifyMessage(pmn->pubKeyMasternode, vchSig, strMessage, errorMessage)) {
            return error("CMasternodePaymentWinner::SignatureValid() - Got bad Masternode address signature %s\n", vinMasternode.prevout.hash.ToString());
        }

        return true;
    }

    return false;
}

void CMasternodePayments::Sync(CNode* node, int nCountNeeded)
{
    LOCK(cs_mapMasternodePayeeVotes);

    int nHeight;
    {
        TRY_LOCK(cs_main, locked);
        if (!locked || !chainActive.Tip())
            return;

        nHeight = chainActive.Tip()->nHeight;
    }

    auto mn_counts = mnodeman.CountEnabledByLevels();

    for(auto& count : mn_counts)
        count.second = std::min(nCountNeeded, (int)(count.second * 1.25));

    int nInvCount = 0;

    for(const auto& vote : mapMasternodePayeeVotes) {
        const auto& winner = vote.second;

        bool push =  winner.nBlockHeight >= nHeight - mn_counts[winner.payeeLevel]
                  && winner.nBlockHeight <= nHeight + 20;

        if(!push)
            continue;

        node->PushInventory(CInv(MSG_MASTERNODE_WINNER, winner.GetHash()));
        ++nInvCount;
    }

    node->PushMessage("ssc", MASTERNODE_SYNC_MNW, nInvCount);
}

std::string CMasternodePayments::ToString() const
{
    std::ostringstream info;

    info << "Votes: " << (int)mapMasternodePayeeVotes.size() << ", Blocks: " << (int)mapMasternodeBlocks.size();

    return info.str();
}


int CMasternodePayments::GetOldestBlock()
{
    LOCK(cs_mapMasternodeBlocks);

    int nOldestBlock = std::numeric_limits<int>::max();

    std::map<int, CMasternodeBlockPayees>::iterator it = mapMasternodeBlocks.begin();
    while (it != mapMasternodeBlocks.end()) {
        if ((*it).first < nOldestBlock) {
            nOldestBlock = (*it).first;
        }
        it++;
    }

    return nOldestBlock;
}


int CMasternodePayments::GetNewestBlock()
{
    LOCK(cs_mapMasternodeBlocks);

    int nNewestBlock = 0;

    std::map<int, CMasternodeBlockPayees>::iterator it = mapMasternodeBlocks.begin();
    while (it != mapMasternodeBlocks.end()) {
        if ((*it).first > nNewestBlock) {
            nNewestBlock = (*it).first;
        }
        it++;
    }

    return nNewestBlock;
}
