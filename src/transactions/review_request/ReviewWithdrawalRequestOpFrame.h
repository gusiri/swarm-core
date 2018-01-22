#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "ReviewRequestOpFrame.h"
#include "ledger/ReviewableRequestFrame.h"
#include "ReviewTwoStepWithdrawalRequestOpFrame.h"

namespace stellar
{
class ReviewWithdrawalRequestOpFrame : public ReviewTwoStepWithdrawalRequestOpFrame
{
protected:
	bool handleApprove(Application& app, LedgerDelta& delta, LedgerManager& ledgerManager, ReviewableRequestFrame::pointer request) override;
	bool handleReject(Application& app, LedgerDelta& delta, LedgerManager& ledgerManager, ReviewableRequestFrame::pointer request) override;
public:

	  ReviewWithdrawalRequestOpFrame(Operation const& op, OperationResult& res,
                       TransactionFrame& parentTx);
protected:
    bool handlePermanentReject(Application& app, LedgerDelta& delta,
        LedgerManager& ledgerManager,
        ReviewableRequestFrame::pointer request) override;
};
}
