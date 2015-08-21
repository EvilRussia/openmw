#ifndef CSM_TOOLS_MERGESTAGES_H
#define CSM_TOOLS_MERGESTAGES_H

#include <components/to_utf8/to_utf8.hpp>

#include "../doc/stage.hpp"

#include "../world/data.hpp"

#include "mergestate.hpp"

namespace CSMTools
{
    class FinishMergedDocumentStage : public CSMDoc::Stage
    {
            MergeState& mState;
            ToUTF8::Utf8Encoder mEncoder;

        public:

            FinishMergedDocumentStage (MergeState& state, ToUTF8::FromType encoding);

            virtual int setup();
            ///< \return number of steps

            virtual void perform (int stage, CSMDoc::Messages& messages);
            ///< Messages resulting from this stage will be appended to \a messages.
    };

    template<typename RecordType>
    class MergeIdCollectionStage : public CSMDoc::Stage
    {
            typedef typename CSMWorld::IdCollection<RecordType> Collection;

            MergeState& mState;
            Collection& (CSMWorld::Data::*mAccessor)();

        public:

            MergeIdCollectionStage (MergeState& state, Collection& (CSMWorld::Data::*accessor)());

            virtual int setup();
            ///< \return number of steps

            virtual void perform (int stage, CSMDoc::Messages& messages);
            ///< Messages resulting from this stage will be appended to \a messages.
    };

    template<typename RecordType>
    MergeIdCollectionStage<RecordType>::MergeIdCollectionStage (MergeState& state, Collection& (CSMWorld::Data::*accessor)())
    : mState (state), mAccessor (accessor)
    {}

    template<typename RecordType>
    int MergeIdCollectionStage<RecordType>::setup()
    {
        return 1;
    }

    template<typename RecordType>
    void MergeIdCollectionStage<RecordType>::perform (int stage, CSMDoc::Messages& messages)
    {
        const Collection& source = (mState.mSource.getData().*mAccessor)();
        Collection& target = (mState.mTarget->getData().*mAccessor)();

        int size = source.getSize();

        for (int i=0; i<size; ++i)
        {
            const CSMWorld::Record<RecordType>& record = source.getRecord (i);

            if (!record.isDeleted())
                target.appendRecord (CSMWorld::Record<RecordType> (CSMWorld::RecordBase::State_BaseOnly, &record.get()));
        }
    }
}

#endif
