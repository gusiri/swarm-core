#include "ManageKeyValueTestHelper.h"
#include "test/test_marshaler.h"

namespace stellar {
    namespace txtest {
        txtest::ManageKeyValueTestHelper::ManageKeyValueTestHelper(txtest::TestManager::pointer testManager) :
                TxHelper(testManager)
        {
        }

        txtest::ManageKeyValueTestHelper txtest::ManageKeyValueTestHelper::copy()
        {
            return *this;
        }

        txtest::ManageKeyValueTestHelper* txtest::ManageKeyValueTestHelper::setKey(string256 key)
        {
            this->key = key;
            return this;
        }

        ManageKeyValueTestHelper* ManageKeyValueTestHelper::setResult(ManageKeyValueResultCode resultCode)
        {
            this->expectedResult = resultCode;
            return  this;
        }

        void ManageKeyValueTestHelper::doAply(Application &app, LedgerDelta &delta, LedgerManager &ledgerManager,
                                                      ManageKVAction action, bool require)
        {
            ManageKeyValueTestBuilder builder(key, mTestManager, action);
            REQUIRE(builder.kvManager->doApply(app,delta,ledgerManager) == require);
            REQUIRE(builder.kvManager->getInnerCode(builder.kvManager->getResult()) == expectedResult);
        }

        Operation ManageKeyValueTestBuilder::buildOp()
        {
            Operation op;
            op.body.type(OperationType::MANAGE_KEY_VALUE);
            op.body.manageKeyValueOp() = ManageKeyValueOp();
            op.body.manageKeyValueOp().key = key;
            op.body.manageKeyValueOp().action.action(kvAction);

            if(kvAction == ManageKVAction::PUT)
            {
                op.body.manageKeyValueOp().action.value().value.type(KeyValueEntryType::KYC_SETTINGS);
                op.body.manageKeyValueOp().action.value().value.kycSettings() = KYCSettings();
                op.body.manageKeyValueOp().action.value().key = key;
            }
            return op;
        }

        ManageKeyValueTestBuilder::ManageKeyValueTestBuilder(string256 key, TestManager::pointer &testManager,
                                                             ManageKVAction action)
                :key(key),
                 kvAction(action)
        {
            auto txFrame = this->buildTx(testManager);
            tx = txFrame.get();
            op = buildOp();
            res = OperationResult(OperationResultCode::opINNER);
            res.tr().type(OperationType::MANAGE_KEY_VALUE);
            kvManager = new ManageKeyValueOpFrame(op,res,*tx);
        }

    }
}