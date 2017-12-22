// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/CreateSaleCreationRequestOpFrame.h"
#include "database/Database.h"
#include "main/Application.h"
#include "medida/metrics_registry.h"
#include "ledger/LedgerDelta.h"
#include "ledger/AccountHelper.h"
#include "ledger/BalanceHelper.h"
#include "ledger/AssetHelper.h"
#include "ledger/AssetPairHelper.h"
#include "ledger/ReviewableRequestFrame.h"
#include "xdrpp/printer.h"
#include "ledger/ReviewableRequestHelper.h"
#include "bucket/BucketApplicator.h"
#include "ledger/SaleFrame.h"

namespace stellar
{
using xdr::operator==;


std::unordered_map<AccountID, CounterpartyDetails>
CreateSaleCreationRequestOpFrame::getCounterpartyDetails(
    Database& db, LedgerDelta* delta) const
{
    // source account is only counterparty
    return {};
}

SourceDetails CreateSaleCreationRequestOpFrame::getSourceAccountDetails(
    std::unordered_map<AccountID, CounterpartyDetails> counterpartiesDetails)
const
{
    return SourceDetails({
                             AccountType::SYNDICATE,
                         }, mSourceAccount->getHighThreshold(),
                         static_cast<int32_t>(SignerType::ASSET_MANAGER));
}

bool CreateSaleCreationRequestOpFrame::isBaseAssetOrCreationRequestExists(
    SaleCreationRequest const& request,
    Database& db) const
{
    const auto assetFrame = AssetHelper::Instance()->loadAsset(request.baseAsset, getSourceID(), db);
    if (!!assetFrame)
    {
        return true;
    }

    auto assetCreationRequests = ReviewableRequestHelper::Instance()->loadRequests(getSourceID(), ReviewableRequestType::ASSET_CREATE, db);
    for (auto assetCreationRequestFrame : assetCreationRequests)
    {
        auto& assetCreationRequest = assetCreationRequestFrame->getRequestEntry().body.assetCreationRequest();
        if (assetCreationRequest.code == request.baseAsset)
        {
            return true;
        }
    }

    return false;
}

std::string CreateSaleCreationRequestOpFrame::getReference(SaleCreationRequest const& request) const
{
    const auto hash = sha256(xdr_to_opaque(ReviewableRequestType::SALE, request.baseAsset));
    return binToHex(hash);
}

ReviewableRequestFrame::pointer CreateSaleCreationRequestOpFrame::
createNewUpdateRequest(Application& app, Database& db, LedgerDelta& delta, time_t closedAt)
{
    if (mCreateSaleCreationRequest.requestID != 0)
    {
        const auto requestFrame = ReviewableRequestHelper::Instance()->loadRequest(mCreateSaleCreationRequest.requestID, getSourceID(), db, &delta);
        if (!requestFrame)
        {
            return nullptr;
        }
    }

    auto const& sale = mCreateSaleCreationRequest.request;
    auto reference = getReference(sale);
    const auto referencePtr = xdr::pointer<string64>(new string64(reference));
    auto request = ReviewableRequestFrame::createNew(mCreateSaleCreationRequest.requestID, getSourceID(), app.getMasterID(),
        referencePtr, closedAt);
    auto& requestEntry = request->getRequestEntry();
    requestEntry.body.type(ReviewableRequestType::SALE);
    requestEntry.body.saleCreationRequest() = sale;
    request->recalculateHashRejectReason();
    return request;
}

CreateSaleCreationRequestOpFrame::CreateSaleCreationRequestOpFrame(
    Operation const& op, OperationResult& res,
    TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mCreateSaleCreationRequest(mOperation.body.createSaleCreationRequestOp())
{
}


bool
CreateSaleCreationRequestOpFrame::doApply(Application& app, LedgerDelta& delta,
                                        LedgerManager& ledgerManager)
{
    auto const& sale = mCreateSaleCreationRequest.request;
    if (sale.endTime <= ledgerManager.getCloseTime())
    {
        innerResult().code(CreateSaleCreationRequestResultCode::INVALID_END);
        return false;
    }

    auto& db = ledgerManager.getDatabase();
    auto request = createNewUpdateRequest(app, db, delta, ledgerManager.getCloseTime());
    if (!request)
    {
        innerResult().code(CreateSaleCreationRequestResultCode::REQUEST_NOT_FOUND);
        return false;
    }

    if (ReviewableRequestHelper::Instance()->isReferenceExist(db, getSourceID(), getReference(sale), request->getRequestID()))
    {
        innerResult().code(CreateSaleCreationRequestResultCode::REQUEST_OR_SALE_ALREADY_EXISTS);
        return false;
    }


    if (!isBaseAssetOrCreationRequestExists(sale, db))
    {
        innerResult().code(CreateSaleCreationRequestResultCode::BASE_ASSET_OR_ASSET_REQUEST_NOT_FOUND);
        return false;
    }

    if (!AssetHelper::Instance()->exists(db, sale.quoteAsset))
    {
        innerResult().code(CreateSaleCreationRequestResultCode::QUOTE_ASSET_NOT_FOUND);
        return false;
    }

    if (request->getRequestID() == 0)
    {
        request->setRequestID(delta.getHeaderFrame().generateID(LedgerEntryType::REVIEWABLE_REQUEST));
        ReviewableRequestHelper::Instance()->storeAdd(delta, db, request->mEntry);
    } else
    {
        ReviewableRequestHelper::Instance()->storeChange(delta, db, request->mEntry);
    }

    innerResult().code(CreateSaleCreationRequestResultCode::SUCCESS);
    innerResult().success().requestID = request->getRequestID();
    return true;
}

bool CreateSaleCreationRequestOpFrame::doCheckValid(Application& app)
{
    const auto& request = mCreateSaleCreationRequest.request;
    if (!AssetFrame::isAssetCodeValid(request.baseAsset) || !AssetFrame::isAssetCodeValid(request.quoteAsset) || request.baseAsset == request.quoteAsset)
    {
        innerResult().code(CreateSaleCreationRequestResultCode::INVALID_ASSET_PAIR);
        return false;
    }

    if (request.endTime <= request.startTime)
    {
        innerResult().code(CreateSaleCreationRequestResultCode::START_END_INVALID);
        return false;
    }

    if (request.price == 0)
    {
        innerResult().code(CreateSaleCreationRequestResultCode::INVALID_PRICE);
        return false;
    }

    uint64_t requiredBaseAsset;
    if (!SaleFrame::calculateRequiredBaseAssetForSoftCap(request, requiredBaseAsset))
    {
        innerResult().code(CreateSaleCreationRequestResultCode::INVALID_PRICE);
        return false;
    }

    if (request.hardCap < request.softCap)
    {
        innerResult().code(CreateSaleCreationRequestResultCode::INVALID_CAP);
        return false;
    }

    return true;
}
}