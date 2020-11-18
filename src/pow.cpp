// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018-2020 The BeetleCoin developers
// Copyright (c) 2018-2020 John "ComputerCraftr" Studnicka
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "chain.h"
#include "chainparams.h"
#include "main.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"

#include <math.h>


unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pblock)
{
    /* current difficulty formula, pivx - DarkGravity v3, written by Evan Duffield - evan@dashpay.io */
    const CBlockIndex* BlockLastSolved = pindexLast;
    const CBlockIndex* BlockReading = pindexLast;
    int64_t nActualTimespan = 0;
    int64_t LastBlockTime = 0;
    int64_t PastBlocksMin = 24;
    int64_t PastBlocksMax = 24;
    int64_t CountBlocks = 0;
    uint256 PastDifficultyAverage;
    uint256 PastDifficultyAveragePrev;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin) {
        return Params().ProofOfWorkLimit().GetCompact();
    }

    const int nHeight = pindexLast->nHeight + 1;
    // This if statement had an off by one error which caused the outside DGW calculation to mistakenly be used for the first PoS block
    if (nHeight > Params().LAST_POW_BLOCK() || nHeight >= Params().SecondForkBlock()) {
        uint256 bnTargetLimit = nHeight < Params().SecondForkBlock() ? ~uint256(0) >> 24 : Params().ProofOfWorkLimit();

        if (pindexLast == nullptr)
            return bnTargetLimit.GetCompact(); // genesis block

        if (pindexLast->pprev == nullptr)
            return bnTargetLimit.GetCompact(); // first block

        if (pindexLast->pprev->pprev == nullptr)
            return bnTargetLimit.GetCompact(); // second block

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            // Account for the first PoS block
            if (nHeight == 201 && pblock->nTime == 1536997636 && pindexLast->GetBlockHash() == uint256("0x000000632f1519f1cb77740707b7efab42bd947adfa3d72b9bc527a99f149e61")) return 0x1e00b943;
            // Difficulty reset for first fork
            if (nHeight == 345000 && pblock->nTime == 1557783102 && pindexLast->GetBlockHash() == uint256("0xf347f550ac55ba62e44b4e6a99bdde0e5e4cabed08d33d04966e6ad37d709a26")) return 0x1d059e8c;
            if (nHeight == 345001 && pblock->nTime == 1557783117 && pindexLast->GetBlockHash() == uint256("0x69c0a8d13ea706b047acada8ba5f67c608d2bd6bf44eea9400be7e128f78363a")) return 0x1e00ffff;
            if (nHeight == 345002 && pblock->nTime == 1557783127 && pindexLast->GetBlockHash() == uint256("0xf335a8fa78eed0430292e052b60e26ca1b349b114ce300447e60242443548804")) return 0x1e00ffff;
            if (nHeight == 345003 && pblock->nTime == 1557783135 && pindexLast->GetBlockHash() == uint256("0x0fd093031c7e9354e8102692d9921e02a6b4e1b01387d5ecb4e164fc4121b452")) return 0x1e00ffff;
            if (nHeight == 345004 && pblock->nTime == 1557783146 && pindexLast->GetBlockHash() == uint256("0x8198b4589cc30bfae5de27035ebc9c21c2b0a86fb8224be1f16b8fe9a8dd1a81")) return 0x1e00ffff;
            if (nHeight == 345005 && pblock->nTime == 1557783154 && pindexLast->GetBlockHash() == uint256("0x9de65e3db76dc3a6c6163c64415e9b46c09f8dd684cd4b0a6028507102665d77")) return 0x1e00ffff;
            if (nHeight == 345006 && pblock->nTime == 1557783167 && pindexLast->GetBlockHash() == uint256("0xa5c69feec49ea12632c23bdfb89a1652497ae31896a80b15396b42df5a7ef5ab")) return 0x1e00ffff;
            if (nHeight == 345007 && pblock->nTime == 1557783176 && pindexLast->GetBlockHash() == uint256("0x88550b0b8633ebdf672e5f43af97e4a32e3531fb76924984e6bd0c12b00d2985")) return 0x1e00ffff;
            if (nHeight == 345008 && pblock->nTime == 1557783188 && pindexLast->GetBlockHash() == uint256("0x3d5836627f058b814e3f399d97e7dcbdf0696ec18792ae1a93d56d4a8c1c16dc")) return 0x1e00ffff;
            if (nHeight == 345009 && pblock->nTime == 1557783198 && pindexLast->GetBlockHash() == uint256("0x1e26bd657acf9150ff0c4ad155bdb7d38e5ef941f343e239ac01a3088bca39d9")) return 0x1e00ffff;
        }

        int nActualSpacing = pindexLast->GetBlockTime() - pindexLast->pprev->GetBlockTime();

        const int nTargetSpacing = nHeight >= Params().SecondForkBlock() ? Params().TargetSpacing() : 60;
        const uint32_t nTargetTimespan = nHeight >= Params().SecondForkBlock() ? Params().TargetTimespan() : (40 * 60);
        const int nInterval = nTargetTimespan / nTargetSpacing;

        if (nHeight >= Params().SecondForkBlock()) {
            // If nActualSpacing is very, very negative (close to but greater than -nTargetTimespan/2), it would cause the difficulty calculation to result
            // in zero or a negative number, so we have no choice but to limit the solvetime to the lowest value this method can handle. Ideally, this if
            // statement would become impossible to trigger by requiring sequential timestamps or MTP enforcement and a large enough nTargetTimespan. There
            // may be a way to extend this calculation to appropriately handle even lower negative numbers, but the difficulty already increases significantly
            // for small negative solvetimes and the next solvetime would have to be many times larger than this negative value in order to simply return
            // to the same difficulty as before, which can be modeled by the following equation where x is previous solvetime and f(x) is the next solvetime:
            // f(x) = ((nInterval + 1) * nTargetSpacing / 2)^2 / ((nInterval - 1) * nTargetSpacing / 2 + x) - ((nInterval - 1) * nTargetSpacing / 2)
            if (nActualSpacing <= -((nInterval - 1) * nTargetSpacing / 2))
                nActualSpacing = -((nInterval - 1) * nTargetSpacing / 2) + 1;
        } else {
            // WARNING: Limiting the solvetime and how much the difficulty can rise here allows attackers to drop the difficulty to zero using timestamps in the past
            if (nActualSpacing < 0)
                nActualSpacing = 1;
        }

        // ppcoin: target change every block
        // ppcoin: retarget with exponential moving toward target spacing
        uint256 bnNew;
        bnNew.SetCompact(pindexLast->nBits);

        const uint32_t numerator = (nInterval - 1) * nTargetSpacing + 2 * nActualSpacing;
        const uint32_t denominator = (nInterval + 1) * nTargetSpacing;

        bnNew = bnNew * numerator / denominator;

        if (bnNew <= 0 || bnNew > bnTargetLimit)
            bnNew = bnTargetLimit;

        return bnNew.GetCompact();
    }

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax) {
            break;
        }
        CountBlocks++;

        if (CountBlocks <= PastBlocksMin) {
            if (CountBlocks == 1) {
                PastDifficultyAverage.SetCompact(BlockReading->nBits);
            } else {
                PastDifficultyAverage = ((PastDifficultyAveragePrev * CountBlocks) + (uint256().SetCompact(BlockReading->nBits))) / (CountBlocks + 1);
            }
            PastDifficultyAveragePrev = PastDifficultyAverage;
        }

        if (LastBlockTime > 0) {
            int64_t Diff = (LastBlockTime - BlockReading->GetBlockTime());
            nActualTimespan += Diff;
        }
        LastBlockTime = BlockReading->GetBlockTime();

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    uint256 bnNew(PastDifficultyAverage);

    int64_t _nTargetTimespan = CountBlocks * Params().TargetSpacing();

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= _nTargetTimespan;

    if (bnNew > Params().ProofOfWorkLimit()) {
        bnNew = Params().ProofOfWorkLimit();
    }

    return bnNew.GetCompact();
}

bool CheckProofOfWork(const uint256& hash, unsigned int nBits)
{
    bool fNegative;
    bool fOverflow;
    uint256 bnTarget;

    if (Params().SkipProofOfWorkCheck())
        return true;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > Params().ProofOfWorkLimit())
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (hash > bnTarget) {
        if (Params().MineBlocksOnDemand())
            return false;
        else
            return error("CheckProofOfWork() : hash doesn't match nBits");
    }

    return true;
}

uint256 GetBlockProof(const CBlockIndex& block)
{
    uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}
