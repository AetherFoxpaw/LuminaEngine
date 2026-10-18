#pragma once

#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "Scripting/ScriptCallback.h"
#include "TaskSystem/Coroutine.h"
#include "CoroutineLibrary.generated.h"

namespace Lumina
{
    class CWorld;

    // Script waits on the world's coroutine scheduler. OnDone receives 1 once the wait elapses, or 0 when the world went away first.
    REFLECT()
    class RUNTIME_API CCoroutineLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        // Resumes after Ticks unpaused frames.
        FUNCTION()
        static uint64 WaitTicks(CWorld* World, int32 Ticks, SScriptCallback OnDone);

        // Resumes after Seconds measured on Clock.
        FUNCTION()
        static uint64 WaitSeconds(CWorld* World, double Seconds, ECoroutineClock Clock, SScriptCallback OnDone);

        // Drops a wait without calling it.
        FUNCTION()
        static void CancelWait(CWorld* World, uint64 Wait);

        // How many waits the world is holding, for leak checks.
        FUNCTION()
        static int32 GetPendingCount(CWorld* World);
    };
}
