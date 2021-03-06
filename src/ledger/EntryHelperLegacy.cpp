// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "ledger/EntryHelper.h"
#include "ledger/EntryHelperLegacy.h"
#include "LedgerManager.h"
#include "ledger/AccountFrame.h"
#include "ledger/AccountHelper.h"
#include "ledger/AccountRoleHelper.h"
#include "ledger/ReferenceFrame.h"
#include "ledger/ReferenceHelper.h"
#include "ledger/StatisticsFrame.h"
#include "ledger/StatisticsHelper.h"
#include "ledger/AccountTypeLimitsFrame.h"
#include "ledger/AccountTypeLimitsHelper.h"
#include "ledger/AccountLimitsFrame.h"
#include "ledger/AccountLimitsHelper.h"
#include "ledger/AssetFrame.h"
#include "ledger/AssetHelperLegacy.h"
#include "ledger/AssetPairFrame.h"
#include "ledger/AssetPairHelper.h"
#include "ledger/LedgerDelta.h"
#include "ledger/FeeFrame.h"
#include "ledger/FeeHelper.h"
#include "ledger/ReviewableRequestFrame.h"
#include "ledger/ReviewableRequestHelper.h"
#include "ledger/StorageHelperImpl.h"
#include "ledger/TrustFrame.h"
#include "ledger/TrustHelper.h"
#include "ledger/OfferFrame.h"
#include "ledger/OfferHelper.h"
#include "ledger/ExternalSystemAccountID.h"
#include "ledger/ExternalSystemAccountIDHelperLegacy.h"
#include "ledger/KeyValueEntryFrame.h"
#include "ledger/KeyValueHelperLegacy.h"
#include "ledger/ExternalSystemAccountIDPoolEntry.h"
#include "ledger/ExternalSystemAccountIDPoolEntryHelperLegacy.h"
#include "xdrpp/printer.h"
#include "xdrpp/marshal.h"
#include "crypto/Hex.h"
#include "database/Database.h"
#include "SaleHelper.h"
#include "AccountKYCHelper.h"
#include "LimitsV2Helper.h"
#include "StatisticsV2Helper.h"
#include "PendingStatisticsHelper.h"
#include "SaleAnteHelper.h"
#include "ContractHelper.h"
#include "BalanceHelperLegacy.h"

namespace stellar
{
	using xdr::operator==;

    static std::shared_ptr<EntryHelper> createHelper(LedgerEntryType entryType, StorageHelper& storageHelper)
    {
        switch (entryType)
        {
            case LedgerEntryType::ACCOUNT_ROLE:
                return std::make_shared<AccountRoleHelper>(storageHelper);
            case LedgerEntryType::ACCOUNT_ROLE_PERMISSION:
                return std::make_shared<AccountRolePermissionHelperImpl>(storageHelper);
            default:
                return nullptr;
        }
    }

    LedgerKey LedgerEntryKey(LedgerEntry const &e)
    {
        // TODO: move this to helpers somehow
        if (e.data.type() == LedgerEntryType::ACCOUNT_ROLE || e.data.type() == LedgerEntryType::ACCOUNT_ROLE_PERMISSION)
        {
            LedgerKey key;
            key.type(e.data.type());
            switch (e.data.type())
            {
                case LedgerEntryType::ACCOUNT_ROLE:
                {
                    key.accountRole().accountRoleID = e.data.accountRole().accountRoleID;
                    break;
                }
                case LedgerEntryType::ACCOUNT_ROLE_PERMISSION:
                {
                    auto& sourceData = e.data.accountRolePermission();
                    key.accountRolePermission().permissionID = sourceData.permissionID;
                    break;
                }
                default:
                    throw std::runtime_error("Unknown key type");
            }
            return key;
        }
        EntryHelperLegacy* helper = EntryHelperProvider::getHelper(e.data.type());
        if (helper == nullptr)
        {
            throw std::runtime_error("There's no legacy helper for this entry.");
        }
		return helper->getLedgerKey(e);
	}

	void EntryHelperLegacy::flushCachedEntry(LedgerKey const &key, Database &db)
	{
		auto s = binToHex(xdr::xdr_to_opaque(key));
		db.getEntryCache().erase_if_exists(s);
	}

	bool EntryHelperLegacy::cachedEntryExists(LedgerKey const &key, Database &db)
	{
		auto s = binToHex(xdr::xdr_to_opaque(key));
		return db.getEntryCache().exists(s);
	}

	std::shared_ptr<LedgerEntry const>
	EntryHelperLegacy::getCachedEntry(LedgerKey const &key, Database &db)
	{
		auto s = binToHex(xdr::xdr_to_opaque(key));
		return db.getEntryCache().get(s);
	}

	void EntryHelperLegacy::putCachedEntry(LedgerKey const &key,
	std::shared_ptr<LedgerEntry const> p, Database &db)
	{
		auto s = binToHex(xdr::xdr_to_opaque(key));
		db.getEntryCache().put(s, p);
	}

    void
    EntryHelperProvider::checkAgainstDatabase(LedgerEntry const& entry, Database& db)
    {
        auto key = LedgerEntryKey(entry);
        EntryFrame::pointer fromDb;
        auto helper = getHelper(entry.data.type());
        if (!helper)
        {
            StorageHelperImpl storageHelper(db, nullptr);
            auto helper = createHelper(entry.data.type(), storageHelper);
            if (!helper)
            {
                throw std::runtime_error("There's no legacy helper for this entry, "
                                         "and no helper can be created.");
            }
            helper->flushCachedEntry(key);
            fromDb = helper->storeLoad(key);
        }
        else
        {
            helper->flushCachedEntry(key, db);
            fromDb = helper->storeLoad(key, db);
        }
        if (!fromDb || !(fromDb->mEntry == entry))
        {
            std::string s;
            s = "Inconsistent state between objects: ";
            s += !!fromDb ? xdr::xdr_to_string(fromDb->mEntry, "db") : "db: nullptr\n";
            s += xdr::xdr_to_string(entry, "live");
            throw std::runtime_error(s);
        }
    }

	void
	EntryHelperProvider::storeAddEntry(LedgerDelta& delta, Database& db, LedgerEntry const& entry)
	{
		EntryHelperLegacy* helper = getHelper(entry.data.type());
		return helper->storeAdd(delta, db, entry);
	}

	void
	EntryHelperProvider::storeChangeEntry(LedgerDelta& delta, Database& db, LedgerEntry const& entry)
	{
		EntryHelperLegacy* helper = getHelper(entry.data.type());
		return helper->storeChange(delta, db, entry);
	}

	void
	EntryHelperProvider::storeAddOrChangeEntry(LedgerDelta &delta, Database &db, LedgerEntry const& entry)
	{
		auto key = LedgerEntryKey(entry);
		if (existsEntry(db, key))
		{
			storeChangeEntry(delta, db, entry);
		}
		else
		{
			storeAddEntry(delta, db, entry);
		}
	}

	void
	EntryHelperProvider::storeDeleteEntry(LedgerDelta& delta, Database& db, LedgerKey const& key)
	{
		EntryHelperLegacy* helper = getHelper(key.type());
		helper->storeDelete(delta, db, key);
	}

	bool
	EntryHelperProvider::existsEntry(Database& db, LedgerKey const& key)
	{
		EntryHelperLegacy* helper = getHelper(key.type());
		if (helper)
        {
            return helper->exists(db, key);
        }
        StorageHelperImpl storageHelper(db, nullptr);
        auto createdHelper = createHelper(key.type(), storageHelper);
        if (!createdHelper)
        {
            throw std::runtime_error("There's no legacy helper for this entry, "
                                     "and no helper can be created.");
        }
        return createdHelper->exists(key);
    }

	EntryFrame::pointer
	EntryHelperProvider::storeLoadEntry(LedgerKey const& key, Database& db)
	{
		EntryHelperLegacy* helper = getHelper(key.type());
		return helper->storeLoad(key, db);
	}

	EntryFrame::pointer
	EntryHelperProvider::fromXDREntry(LedgerEntry const& from)
	{
		EntryHelperLegacy* helper = getHelper(from.data.type());
		return helper->fromXDR(from);
	}

	uint64_t
	EntryHelperProvider::countObjectsEntry(soci::session& sess, LedgerEntryType const& type)
	{
		EntryHelperLegacy* helper = getHelper(type);
		return helper->countObjects(sess);
	}

	void EntryHelperProvider::dropAll(Database& db)
	{
		for (auto &it : helpers) {
			it.second->dropAll(db);
		}
	}

	EntryHelperProvider::helperMap EntryHelperProvider::helpers = {
		{ LedgerEntryType::ACCOUNT, AccountHelper::Instance() },
		{ LedgerEntryType::ACCOUNT_LIMITS, AccountLimitsHelper::Instance() },
		{ LedgerEntryType::ACCOUNT_TYPE_LIMITS, AccountTypeLimitsHelper::Instance() },
		{ LedgerEntryType::ASSET, AssetHelperLegacy::Instance() },
		{ LedgerEntryType::ASSET_PAIR, AssetPairHelper::Instance() },
		{ LedgerEntryType::BALANCE, BalanceHelperLegacy::Instance() },
		{ LedgerEntryType::EXTERNAL_SYSTEM_ACCOUNT_ID, ExternalSystemAccountIDHelperLegacy::Instance() },
		{ LedgerEntryType::FEE, FeeHelper::Instance() },
		{ LedgerEntryType::OFFER_ENTRY, OfferHelper::Instance() },
		{ LedgerEntryType::REFERENCE_ENTRY, ReferenceHelper::Instance() },
		{ LedgerEntryType::REVIEWABLE_REQUEST, ReviewableRequestHelper::Instance() },
		{ LedgerEntryType::STATISTICS, StatisticsHelper::Instance() },
		{ LedgerEntryType::TRUST, TrustHelper::Instance() },
		{ LedgerEntryType::KEY_VALUE, KeyValueHelperLegacy::Instance()},
        { LedgerEntryType::ACCOUNT_KYC, AccountKYCHelper::Instance()},
		{ LedgerEntryType::SALE, SaleHelper::Instance() },
		{ LedgerEntryType::EXTERNAL_SYSTEM_ACCOUNT_ID_POOL_ENTRY, ExternalSystemAccountIDPoolEntryHelperLegacy::Instance() },
        { LedgerEntryType::LIMITS_V2, LimitsV2Helper::Instance() },
		{ LedgerEntryType::STATISTICS_V2, StatisticsV2Helper::Instance() },
		{ LedgerEntryType::PENDING_STATISTICS, PendingStatisticsHelper::Instance() },
		{ LedgerEntryType::SALE_ANTE, SaleAnteHelper::Instance() },
		{ LedgerEntryType::CONTRACT, ContractHelper::Instance() }
	};
}
