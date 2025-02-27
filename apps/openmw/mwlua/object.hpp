#ifndef MWLUA_OBJECT_H
#define MWLUA_OBJECT_H

#include <typeindex>

#include <components/esm3/cellref.hpp>
#include <components/esm/defs.hpp>
#include <components/esm/luascripts.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/ptr.hpp"

namespace MWLua
{
    namespace ObjectTypeName
    {
        // Names of object types in Lua.
        // These names are part of OpenMW Lua API.
        constexpr std::string_view Activator = "Activator";
        constexpr std::string_view Armor = "Armor";
        constexpr std::string_view Book = "Book";
        constexpr std::string_view Clothing = "Clothing";
        constexpr std::string_view Container = "Container";
        constexpr std::string_view Creature = "Creature";
        constexpr std::string_view Door = "Door";
        constexpr std::string_view Ingredient = "Ingredient";
        constexpr std::string_view Light = "Light";
        constexpr std::string_view MiscItem = "Miscellaneous";
        constexpr std::string_view NPC = "NPC";
        constexpr std::string_view Player = "Player";
        constexpr std::string_view Potion = "Potion";
        constexpr std::string_view Static = "Static";
        constexpr std::string_view Weapon = "Weapon";
        constexpr std::string_view Apparatus = "Apparatus";
        constexpr std::string_view Lockpick = "Lockpick";
        constexpr std::string_view Probe = "Probe";
        constexpr std::string_view Repair = "Repair";
    }

    // ObjectId is a unique identifier of a game object.
    // It can change only if the order of content files was change.
    using ObjectId = ESM::RefNum;
    inline const ObjectId& getId(const MWWorld::Ptr& ptr) { return ptr.getCellRef().getRefNum(); }
    std::string idToString(const ObjectId& id);
    std::string ptrToString(const MWWorld::Ptr& ptr);
    bool isMarker(const MWWorld::Ptr& ptr);
    std::string_view getLuaObjectTypeName(ESM::RecNameInts recordType, std::string_view fallback = "Unknown");
    std::string_view getLuaObjectTypeName(const MWWorld::Ptr& ptr);

    // Each script has a set of flags that controls to which objects the script should be
    // automatically attached. This function maps each object types to one of the flags. 
    ESM::LuaScriptCfg::Flags getLuaScriptFlag(const MWWorld::Ptr& ptr);

    // Holds a mapping ObjectId -> MWWord::Ptr.
    class ObjectRegistry
    {
    public:
        ObjectRegistry() { mLastAssignedId.unset(); }

        void update();  // Should be called every frame.
        void clear();  // Should be called before starting or loading a new game.

        ObjectId registerPtr(const MWWorld::Ptr& ptr);
        ObjectId deregisterPtr(const MWWorld::Ptr& ptr);

        // Returns Ptr by id. If object is not found, returns empty Ptr.
        // If local = true, returns non-empty ptr only if it can be used in local scripts
        // (i.e. is active or was active in the previous frame).
        MWWorld::Ptr getPtr(ObjectId id, bool local);

        // Needed only for saving/loading.
        const ObjectId& getLastAssignedId() const { return mLastAssignedId; }
        void setLastAssignedId(ObjectId id) { mLastAssignedId = id; }

    private:
        friend class Object;
        friend class LuaManager;

        bool mChanged = false;
        int64_t mUpdateCounter = 0;
        std::map<ObjectId, MWWorld::Ptr> mObjectMapping;
        ObjectId mLastAssignedId;
    };

    // Lua scripts can't use MWWorld::Ptr directly, because lifetime of a script can be longer than lifetime of Ptr.
    // `GObject` and `LObject` are intended to be passed to Lua as a userdata.
    // It automatically updates the underlying Ptr when needed.
    class Object
    {
    public:
        Object(ObjectId id, ObjectRegistry* reg) : mId(id), mObjectRegistry(reg) {}
        virtual ~Object() {}
        ObjectId id() const { return mId; }

        std::string toString() const;
        std::string_view type() const { return getLuaObjectTypeName(ptr()); }

        // Updates and returns the underlying Ptr. Throws an exception if object is not available.
        const MWWorld::Ptr& ptr() const;

        // Returns `true` if calling `ptr()` is safe.
        bool isValid() const;

    protected:
        virtual void updatePtr() const = 0;

        const ObjectId mId;
        ObjectRegistry* mObjectRegistry;

        mutable MWWorld::Ptr mPtr;
        mutable int64_t mLastUpdate = -1;
    };

    // Used only in local scripts
    class LObject : public Object
    {
        using Object::Object;
        void updatePtr() const final { mPtr = mObjectRegistry->getPtr(mId, true); }
    };

    // Used only in global scripts
    class GObject : public Object
    {
        using Object::Object;
        void updatePtr() const final { mPtr = mObjectRegistry->getPtr(mId, false); }
    };

    using ObjectIdList = std::shared_ptr<std::vector<ObjectId>>;
    template <typename Obj>
    struct ObjectList { ObjectIdList mIds; };
    using GObjectList = ObjectList<GObject>;
    using LObjectList = ObjectList<LObject>;

}

#endif  // MWLUA_OBJECT_H
