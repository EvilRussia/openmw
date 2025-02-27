#include "luabindings.hpp"

#include "luamanagerimp.hpp"

namespace sol
{
    template <>
    struct is_automagical<MWLua::AsyncPackageId> : std::false_type {};
}

namespace MWLua
{

    struct TimerCallback
    {
        AsyncPackageId mAsyncId;
        std::string mName;
    };

    sol::function getAsyncPackageInitializer(const Context& context)
    {
        using TimerType = LuaUtil::ScriptsContainer::TimerType;
        sol::usertype<AsyncPackageId> api = context.mLua->sol().new_usertype<AsyncPackageId>("AsyncPackage");
        api["registerTimerCallback"] = [](const AsyncPackageId& asyncId, std::string_view name, sol::function callback)
        {
            asyncId.mContainer->registerTimerCallback(asyncId.mScriptId, name, std::move(callback));
            return TimerCallback{asyncId, std::string(name)};
        };
        api["newSimulationTimer"] = [world=context.mWorldView](const AsyncPackageId&, double delay,
                                                               const TimerCallback& callback, sol::object callbackArg)
        {
            callback.mAsyncId.mContainer->setupSerializableTimer(
                TimerType::SIMULATION_TIME, world->getSimulationTime() + delay,
                callback.mAsyncId.mScriptId, callback.mName, std::move(callbackArg));
        };
        api["newGameTimer"] = [world=context.mWorldView](const AsyncPackageId&, double delay,
                                                         const TimerCallback& callback, sol::object callbackArg)
        {
            callback.mAsyncId.mContainer->setupSerializableTimer(
                TimerType::GAME_TIME, world->getGameTime() + delay,
                callback.mAsyncId.mScriptId, callback.mName, std::move(callbackArg));
        };
        api["newUnsavableSimulationTimer"] = [world=context.mWorldView](const AsyncPackageId& asyncId, double delay, sol::function callback)
        {
            asyncId.mContainer->setupUnsavableTimer(
                TimerType::SIMULATION_TIME, world->getSimulationTime() + delay, asyncId.mScriptId, std::move(callback));
        };
        api["newUnsavableGameTimer"] = [world=context.mWorldView](const AsyncPackageId& asyncId, double delay, sol::function callback)
        {
            asyncId.mContainer->setupUnsavableTimer(
                TimerType::GAME_TIME, world->getGameTime() + delay, asyncId.mScriptId, std::move(callback));
        };
        api["callback"] = [](const AsyncPackageId& asyncId, sol::function fn)
        {
            return LuaUtil::Callback{std::move(fn), asyncId.mHiddenData};
        };

        auto initializer = [](sol::table hiddenData)
        {
            LuaUtil::ScriptsContainer::ScriptId id = hiddenData[LuaUtil::ScriptsContainer::sScriptIdKey];
            return AsyncPackageId{id.mContainer, id.mIndex, hiddenData};
        };
        return sol::make_object(context.mLua->sol(), initializer);
    }

}
