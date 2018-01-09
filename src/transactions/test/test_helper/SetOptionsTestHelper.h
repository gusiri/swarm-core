//
// Created by volodymyr on 04.01.18.
//

#ifndef STELLAR_SETOPTIONSTESTHELPER_H
#define STELLAR_SETOPTIONSTESTHELPER_H

#include <transactions/test/TxTests.h>
#include "TxHelper.h"

namespace stellar
{

namespace txtest
{

    class SetOptionsTestHelper : TxHelper
    {
    private:
        void checkUpdateThresholds(ThresholdSetter *thresholds, AccountFrame::pointer accountAfterTx);

        bool checkUpdateSigner(Signer *expectedSigner, AccountFrame::pointer accountAfterTx);

    public:

        explicit SetOptionsTestHelper(TestManager::pointer testManager);

        TransactionFramePtr createSetOptionsTx(Account &source, ThresholdSetter *thresholds, Signer *signer, TrustData *trustData,
                                                       UpdateKYCData *updateKYCData);

        SetOptionsResult applySetOptionsTx(Account &source, ThresholdSetter *thresholds, Signer *signer,
                                               TrustData *trustData, UpdateKYCData* updateKYCData,
                                               SetOptionsResultCode expectedResult = SetOptionsResultCode::SUCCESS);
    };

}

}




#endif //STELLAR_SETOPTIONSTESTHELPER_H