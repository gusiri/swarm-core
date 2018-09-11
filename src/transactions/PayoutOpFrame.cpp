#include "util/asio.h"
#include "transactions/PayoutOpFrame.h"
#include "ledger/AccountHelper.h"
#include "ledger/AssetHelper.h"
#include "ledger/BalanceHelper.h"
#include "ledger/LedgerDelta.h"
#include "main/Application.h"

namespace stellar {
using xdr::operator==;

PayoutOpFrame::PayoutOpFrame(Operation const &op, OperationResult &res,
                             TransactionFrame &parentTx)
        : OperationFrame(op, res, parentTx), mPayout(mOperation.body.payoutOp())
{
}

std::unordered_map<AccountID, CounterpartyDetails>
PayoutOpFrame::getCounterpartyDetails(Database &db, LedgerDelta *delta) const
{
    // source account is only counterparty
    return {};
}

SourceDetails
PayoutOpFrame::getSourceAccountDetails(
        std::unordered_map<AccountID, CounterpartyDetails>
        counterpartiesDetails) const
{
    return SourceDetails({AccountType::SYNDICATE},
                         mSourceAccount->getMediumThreshold(),
                         static_cast<int32_t>(SignerType::ASSET_MANAGER));
}

bool
PayoutOpFrame::isFeeMatches(AccountManager &accountManager,
                            BalanceFrame::pointer balance) const
{
    return accountManager.isFeeMatches(mSourceAccount, mPayout.fee,
                                       FeeType::PAYOUT_FEE,
                                       FeeFrame::SUBTYPE_ANY,
                                       balance->getAsset(),
                                       mPayout.maxPayoutAmount);
}

bool
PayoutOpFrame::processBalanceChange(Application &app,
                                    AccountManager::Result balanceChangeResult)
{
    if (balanceChangeResult == AccountManager::Result::UNDERFUNDED) {
        innerResult().code(PayoutResultCode::UNDERFUNDED);
        return false;
    }

    if (balanceChangeResult == AccountManager::Result::STATS_OVERFLOW) {
        innerResult().code(PayoutResultCode::STATS_OVERFLOW);
        return false;
    }

    if (balanceChangeResult == AccountManager::Result::LIMITS_EXCEEDED) {
        innerResult().code(PayoutResultCode::LIMITS_EXCEEDED);
        return false;
    }
    return true;
}

void
PayoutOpFrame::addShareAmount(BalanceFrame::pointer const &holder)
{
    uint64_t shareAmount = 0;

    if (!bigDivide(shareAmount,
                   static_cast<uint64_t>(holder->getAmount() + holder->getLocked()),
                   mPayout.maxPayoutAmount,
                   mAsset->getIssued(),
                   ROUND_DOWN)) {
        CLOG(ERROR, Logging::OPERATION_LOGGER)
                << "Unexpected state: share amount overflows UINT64_MAX, balance id: "
                << BalanceKeyUtils::toStrKey(holder->getBalanceID());
    }

    if (shareAmount > ONE) {
        mShareAmounts.emplace(holder->getAccountID(), shareAmount);
        mActualPayoutAmount += shareAmount;
    }
}

void
PayoutOpFrame::addReceiver(AccountID const &shareholderID, Database &db,
                           LedgerDelta &delta)
{
    auto receiverBalance = BalanceHelper::Instance()->loadBalance(shareholderID,
                                                                  mSourceBalance->getAsset(),
                                                                  db, &delta);
    if (!receiverBalance)
        receiverBalance = BalanceHelper::Instance()->createNewBalance(shareholderID, mSourceBalance->getAsset(),
                                                                      db, delta);

    mReceivers.emplace_back(receiverBalance);
}

bool
PayoutOpFrame::doApply(Application &app, LedgerDelta &delta,
                       LedgerManager &ledgerManager)
{
    Database &db = app.getDatabase();
    innerResult().code(PayoutResultCode::SUCCESS);

    auto assetFrame = AssetHelper::Instance()->loadAsset(mPayout.asset,
            getSourceID(), db);
    if (!assetFrame)
    {
        innerResult().code(PayoutResultCode::ASSET_NOT_FOUND);
        return false;
    }

    if (!assetFrame->isPolicySet(AssetPolicy::TRANSFERABLE))
    {
        innerResult().code(PayoutResultCode::NOT_ALLOWED_BY_ASSET_POLICY);
        return false;
    }

    mSourceBalance = BalanceHelper::Instance()->loadBalance(mPayout.sourceBalanceID, db, &delta);
    if (!mSourceBalance) {
        innerResult().code(PayoutResultCode::BALANCE_NOT_FOUND);
        return false;
    }

    if (!(mSourceBalance->getAccountID() == getSourceID())) {
        innerResult().code(PayoutResultCode::BALANCE_ACCOUNT_MISMATCHED);
        return false;
    }

    AccountManager accountManager(app, db, delta, ledgerManager);
    if (!isFeeMatches(accountManager, mSourceBalance)) {
        innerResult().code(PayoutResultCode::FEE_MISMATCHED);
        return false;
    }

    auto balanceHelper = BalanceHelper::Instance();
    auto assetHolderBalances = balanceHelper->loadAssetHolders(mPayout.asset,
            getSourceID(), db);
    if (mHolders.empty()) {
        innerResult().code(PayoutResultCode::HOLDERS_NOT_FOUND);
        return false;
    }

    for (auto const &holder : mHolders) {
        addShareAmount(holder);
    }

    innerResult().payoutSuccessResult().actualPayoutAmount = mActualPayoutAmount;

    for (auto const &shareAmount : mShareAmounts) {
        addReceiver(shareAmount.first, db, delta);
    }

    auto totalFee = mPayout.fee.percent + mPayout.fee.fixed;
    auto totalAmount = mActualPayoutAmount + totalFee;

    int64 sourceSentUniversal;
    auto transferResult = accountManager.processTransfer(mSourceAccount, mSourceBalance,
                                                         totalAmount, sourceSentUniversal);
    if (!processBalanceChange(app, transferResult))
        return false;

    for (auto const &receiver : mReceivers) {
        auto receiverID = receiver->getAccountID();
        if (!receiver->addBalance(mShareAmounts[receiverID])) {
            innerResult().code(PayoutResultCode::LINE_FULL);
            return false;
        }

        EntryHelperProvider::storeChangeEntry(delta, db, receiver->mEntry);

        PayoutResponse payoutResponse;
        payoutResponse.receiverID = receiverID;
        payoutResponse.receiverBalanceID = receiver->getBalanceID();
        payoutResponse.receivedAmount = mShareAmounts[receiverID];
        innerResult().payoutSuccessResult().payoutResponses.emplace_back(payoutResponse);
    }

    if (totalFee > 0) {
        auto commissionBalanceFrame = BalanceHelper::Instance()->loadBalance(app.getCommissionID(),
                                                                             mSourceBalance->getAsset(),
                                                                             db, &delta);
        if (!commissionBalanceFrame)
            commissionBalanceFrame = BalanceHelper::Instance()->createNewBalance(app.getCommissionID(),
                                                                                 mSourceBalance->getAsset(),
                                                                                 db, delta);
        if (!commissionBalanceFrame->addBalance(totalFee)) {
            innerResult().code(PayoutResultCode::LINE_FULL);
            return false;
        }

        EntryHelperProvider::storeChangeEntry(delta, db, commissionBalanceFrame->mEntry);
    }

    EntryHelperProvider::storeChangeEntry(delta, db, mSourceBalance->mEntry);

    return true;
}

bool PayoutOpFrame::doCheckValid(Application &app) {
    if (mPayout.maxPayoutAmount == 0) {
        innerResult().code(PayoutResultCode::MALFORMED);
        return false;
    }

    if (!AssetFrame::isAssetCodeValid(mPayout.asset)) {
        innerResult().code(PayoutResultCode::MALFORMED);
        return false;
    }

    return true;
}

}