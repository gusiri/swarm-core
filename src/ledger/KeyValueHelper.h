//
// Created by dmytriiev on 04.04.18.
//

#ifndef STELLAR_KEYENTRYHELPER_H
#define STELLAR_KEYENTRYHELPER_H

#include "ledger/EntryHelper.h"
#include "ledger/LedgerManager.h"
#include <functional>
#include <unordered_map>
#include "KeyValueEntryFrame.h"


namespace soci
{
    class session;
}

namespace stellar
{

    class StatementContext;

    class KeyValueHelper : public EntryHelper {

    public:
        KeyValueHelper(KeyValueHelper const &) = delete;

        KeyValueHelper &operator=(KeyValueHelper const &) = delete;

        static KeyValueHelper *Instance() {
            static KeyValueHelper singleton;
            return &singleton;
        }

        void dropAll(Database &db) override;

        void storeAdd(LedgerDelta &delta, Database &db, LedgerEntry const &entry) override;

        void storeChange(LedgerDelta &delta, Database &db, LedgerEntry const &entry) override;

        void storeDelete(LedgerDelta &delta, Database &db, LedgerKey const &key) override;

        bool exists(Database &db, LedgerKey const &key) override;

        LedgerKey getLedgerKey(LedgerEntry const &from) override;

        EntryFrame::pointer storeLoad(LedgerKey const &key, Database &db) override;

        EntryFrame::pointer fromXDR(LedgerEntry const &from) override;

        uint64_t countObjects(soci::session &sess) override;

        KeyValueEntryFrame::pointer
        loadKeyValue(string256 valueKey, Database &db, LedgerDelta *delta = nullptr);

        void loadKeyValues(StatementContext &prep, std::function<void(LedgerEntry const &)> keyValueProcessor);

    private:
        KeyValueHelper() { ; }

        ~KeyValueHelper() { ; }

        void storeUpdateHelper(LedgerDelta &delta, Database &db, bool insert, LedgerEntry const &entry);
    };

}
#endif //STELLAR_KEYENTRYHELPER_H