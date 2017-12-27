
// Copyright 2017 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "ExternalSystemIDGenerators.h"
#include "BTCIDGenerator.h"
#include "ETHIDGenerator.h"
#include "ledger/LedgerDelta.h"

namespace stellar
{
std::shared_ptr<Generator> ExternalSystemIDGenerators::getGeneratorForType(
    Application& app, Database& db,
    const ExternalSystemIDGeneratorType type) const
{
    auto root = "xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDMSgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB";
    switch (type)
    {
    case ExternalSystemIDGeneratorType::BITCOIN_BASIC:
        return std::make_shared<BTCIDGenerator>(app, db, root);
    case ExternalSystemIDGeneratorType::ETHEREUM_BASIC:
        return std::make_shared<ETHIDGenerator>(app, db, root);
    default:
    {
        CLOG(ERROR, Logging::OPERATION_LOGGER) <<
            "Unexpected external system generator type: " << xdr::xdr_traits<
                ExternalSystemIDGeneratorType>::enum_name(type);
        throw std::runtime_error("Unexpected external system generator type");
    }
    }
}

ExternalSystemIDGenerators::ExternalSystemIDGenerators(Application& app,
                                                       LedgerDelta& delta,
                                                       Database&
                                                       db): mDelta(delta)
{
    for (auto generatorType : mDelta.getHeaderFrame().mHeader.
                                     externalSystemIDGenerators)
    {
        mGenerators.push_back(getGeneratorForType(app, db, generatorType));
    }
}

ExternalSystemIDGenerators::~ExternalSystemIDGenerators() = default;

std::vector<ExternalSystemAccountIDFrame::pointer> ExternalSystemIDGenerators::generateNewIDs(AccountID const& accountID)
{
    const auto id = mDelta.getHeaderFrame().generateID(LedgerEntryType::EXTERNAL_SYSTEM_ACCOUNT_ID);
    std::vector<ExternalSystemAccountIDFrame::pointer> results;
    for (auto& generator : mGenerators)
    {
        const auto result = generator->tryGenerateNewID(accountID, id);
        if (!result)
        {
            continue;
        }
        results.push_back(result);
    }

    return results;
}
}
