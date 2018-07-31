#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <xdr/Stellar-operation-manage-invoice-request.h>
#include "transactions/OperationFrame.h"

namespace stellar
{
class ManageContractRequestOpFrame : public OperationFrame
{

    ManageContractRequestResult&
    innerResult()
    {
        return mResult.tr().manageContractRequestResult();
    }

    ManageContractRequestOp const& mManageContractRequest;

    std::unordered_map<AccountID, CounterpartyDetails> getCounterpartyDetails(Database& db,
                                                                              LedgerDelta* delta) const override;
    SourceDetails getSourceAccountDetails(std::unordered_map<AccountID, CounterpartyDetails> counterpartiesDetails,
                                          int32_t ledgerVersion) const override;

    std::string getManageContractRequestReference(longstring const& details) const;

    bool createManageContractRequest(Application& app, LedgerDelta& delta, LedgerManager& ledgerManager);

public:
    ManageContractRequestOpFrame(Operation const& op, OperationResult& res, TransactionFrame& parentTx);

    bool doApply(Application& app, LedgerDelta& delta, LedgerManager& ledgerManager) override;
    bool doCheckValid(Application& app) override;

    static ManageContractRequestResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().manageContractRequestResult().code();
    }
};
}
