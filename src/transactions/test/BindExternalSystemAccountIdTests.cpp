#include <exsysidgen/ETHIDGenerator.h>
#include "main/Application.h"
#include "util/Timer.h"
#include "main/Config.h"
#include "overlay/LoopbackPeer.h"
#include "main/test.h"
#include "TxTests.h"
#include "ledger/BalanceHelperLegacy.h"
#include "ledger/LedgerDeltaImpl.h"
#include "test_helper/BindExternalSystemAccountIdTestHelper.h"
#include "test_helper/CreateAccountTestHelper.h"
#include "test_helper/ManageExternalSystemAccountIDPoolEntryTestHelper.h"
#include "test/test_marshaler.h"

using namespace stellar;
using namespace stellar::txtest;

typedef std::unique_ptr<Application> appPtr;


TEST_CASE("bind external system account_id", "[tx][bind_external_system_account_id]")
{
    auto const ERC20_TokenExternalSystemType = 4;
    Config const& cfg = getTestConfig(0, Config::TESTDB_POSTGRESQL);
    VirtualClock clock;
    auto const appPtr = Application::create(clock, cfg);
    auto& app = *appPtr;
    app.start();
    auto testManager = TestManager::make(app);

    LedgerDeltaImpl delta(app.getLedgerManager().getCurrentLedgerHeader(),
                          app.getDatabase());

    auto root = Account{ getRoot(), Salt(0) };

    BindExternalSystemAccountIdTestHelper bindExternalSystemAccountIdTestHelper(testManager);
    CreateAccountTestHelper createAccountTestHelper(testManager);
    ManageExternalSystemAccountIDPoolEntryTestHelper manageExternalSystemAccountIDPoolEntryTestHelper(testManager);

    auto account = Account { SecretKey::random(), Salt(0) };
    createAccountTestHelper.applyCreateAccountTx(root, account.key.getPublicKey(), AccountType::GENERAL);

    testManager->advanceToTime(BindExternalSystemAccountIdOpFrame::dayInSeconds);

    SECTION("Happy path")
    {
        manageExternalSystemAccountIDPoolEntryTestHelper.createExternalSystemAccountIdPoolEntry(root,
                                                                                                ERC20_TokenExternalSystemType,
                                                                                            "Some data");

        bindExternalSystemAccountIdTestHelper.applyBindExternalSystemAccountIdTx(account, ERC20_TokenExternalSystemType);

        SECTION("Prolongation of external system account id")
        {
            testManager->advanceToTime(BindExternalSystemAccountIdOpFrame::dayInSeconds * 2);

            bindExternalSystemAccountIdTestHelper.applyBindExternalSystemAccountIdTx(account, ERC20_TokenExternalSystemType);
        }
        SECTION("Account trying to bind autogenerated external system account id")
        {
            bindExternalSystemAccountIdTestHelper.applyBindExternalSystemAccountIdTx(account,
                                                                                     EthereumExternalSystemType,
            BindExternalSystemAccountIdResultCode::AUTO_GENERATED_TYPE_NOT_ALLOWED);
        }
    }

    SECTION("No external system account ids of this type in pool")
    {
        bindExternalSystemAccountIdTestHelper.applyBindExternalSystemAccountIdTx(account, ERC20_TokenExternalSystemType,
                                                             BindExternalSystemAccountIdResultCode::NO_AVAILABLE_ID);
    }

    SECTION("All external system account ids of this type are bound")
    {
        auto binder = Account { SecretKey::random(), Salt(0) };
        createAccountTestHelper.applyCreateAccountTx(root, binder.key.getPublicKey(), AccountType::GENERAL);

        manageExternalSystemAccountIDPoolEntryTestHelper.createExternalSystemAccountIdPoolEntry(root,
                                                                                                ERC20_TokenExternalSystemType,
                                                                                            "Some data");

        bindExternalSystemAccountIdTestHelper.applyBindExternalSystemAccountIdTx(binder, ERC20_TokenExternalSystemType);

        bindExternalSystemAccountIdTestHelper.applyBindExternalSystemAccountIdTx(account, ERC20_TokenExternalSystemType,
                                                             BindExternalSystemAccountIdResultCode::NO_AVAILABLE_ID);
    }
    SECTION("Bind expired external system account id")
    {
        auto binder = Account {SecretKey::random(), Salt(0)};
        createAccountTestHelper.applyCreateAccountTx(root, binder.key.getPublicKey(), AccountType::GENERAL);

        manageExternalSystemAccountIDPoolEntryTestHelper.createExternalSystemAccountIdPoolEntry(root,
                                                                                                ERC20_TokenExternalSystemType,
                                                                                                "Some data");

        bindExternalSystemAccountIdTestHelper.applyBindExternalSystemAccountIdTx(binder, ERC20_TokenExternalSystemType);

        testManager->advanceToTime(BindExternalSystemAccountIdOpFrame::dayInSeconds * 3);

        bindExternalSystemAccountIdTestHelper.applyBindExternalSystemAccountIdTx(account, ERC20_TokenExternalSystemType);
    }
}