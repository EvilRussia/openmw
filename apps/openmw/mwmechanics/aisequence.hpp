#ifndef GAME_MWMECHANICS_AISEQUENCE_H
#define GAME_MWMECHANICS_AISEQUENCE_H

#include <list>
#include <memory>

#include "aistate.hpp"
#include "aipackagetypeid.hpp"

#include <components/esm3/loadnpc.hpp>

namespace MWWorld
{
    class Ptr;
}

namespace ESM
{
    namespace AiSequence
    {
        struct AiSequence;
    }
}



namespace MWMechanics
{
    class AiPackage;
    class CharacterController;
    
    template< class Base > class DerivedClassStorage;
    struct AiTemporaryBase;
    typedef DerivedClassStorage<AiTemporaryBase> AiState;

    /// \brief Sequence of AI-packages for a single actor
    /** The top-most AI package is run each frame. When completed, it is removed from the stack. **/
    class AiSequence
    {
            ///AiPackages to run though
            std::list<std::shared_ptr<AiPackage>> mPackages;

            ///Finished with top AIPackage, set for one frame
            bool mDone;

            ///Copy AiSequence
            void copy (const AiSequence& sequence);

            /// The type of AI package that ran last
            AiPackageTypeId mLastAiPackage;
            AiState mAiState;

        public:
            ///Default constructor
            AiSequence();

            /// Copy Constructor
            AiSequence (const AiSequence& sequence);

            /// Assignment operator
            AiSequence& operator= (const AiSequence& sequence);

            virtual ~AiSequence();

            /// Iterator may be invalidated by any function calls other than begin() or end().
            std::list<std::shared_ptr<AiPackage>>::const_iterator begin() const { return mPackages.begin(); }
            std::list<std::shared_ptr<AiPackage>>::const_iterator end() const { return mPackages.end(); }

            void erase(std::list<std::shared_ptr<AiPackage>>::const_iterator package);

            std::list<std::shared_ptr<AiPackage>>& getUnderlyingList() { return mPackages; }

            /// Returns currently executing AiPackage type
            /** \see enum class AiPackageTypeId **/
            AiPackageTypeId getTypeId() const;

            /// Get the typeid of the Ai package that ran last
            /** NOT the currently "active" Ai package that will be run in the next frame.
                This difference is important when an Ai package has just finished and been removed.
                \see enum class AiPackageTypeId **/
            AiPackageTypeId getLastRunTypeId() const { return mLastAiPackage; }

            /// Return true and assign target if combat package is currently active, return false otherwise
            bool getCombatTarget (MWWorld::Ptr &targetActor) const;

            /// Return true and assign targets for all combat packages, or return false if there are no combat packages
            bool getCombatTargets(std::vector<MWWorld::Ptr> &targetActors) const;

            /// Is there any combat package?
            bool isInCombat () const;

            /// Are we in combat with any other actor, who's also engaging us?
            bool isEngagedWithActor () const;

            /// Does this AI sequence have the given package type?
            bool hasPackage(AiPackageTypeId typeId) const;

            /// Are we in combat with this particular actor?
            bool isInCombat (const MWWorld::Ptr& actor) const;

            bool canAddTarget(const ESM::Position& actorPos, float distToTarget) const;
            ///< Function assumes that actor can have only 1 target apart player

            /// Removes all combat packages until first non-combat or stack empty.
            void stopCombat();

            /// Removes all combat packages with the given targets
            void stopCombat(const std::vector<MWWorld::Ptr>& targets);

            /// Has a package been completed during the last update?
            bool isPackageDone() const;

            /// Removes all pursue packages until first non-pursue or stack empty.
            void stopPursuit();

            /// Execute current package, switching if needed.
            void execute (const MWWorld::Ptr& actor, CharacterController& characterController, float duration, bool outOfRange=false);

            /// Simulate the passing of time using the currently active AI package
            void fastForward(const MWWorld::Ptr &actor);

            /// Remove all packages.
            void clear();

            ///< Add \a package to the front of the sequence
            /** Suspends current package
                @param actor The actor that owns this AiSequence **/
            void stack (const AiPackage& package, const MWWorld::Ptr& actor, bool cancelOther=true);

            /// Return the current active package.
            /** If there is no active package, it will throw an exception **/
            const AiPackage& getActivePackage();

            /// Fills the AiSequence with packages
            /** Typically used for loading from the ESM
                \see ESM::AIPackageList **/
            void fill (const ESM::AIPackageList& list);

            bool isEmpty() const;

            void writeState (ESM::AiSequence::AiSequence& sequence) const;
            void readState (const ESM::AiSequence::AiSequence& sequence);
    };
}

#endif
