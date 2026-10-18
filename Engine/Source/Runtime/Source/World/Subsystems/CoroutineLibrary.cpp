#include "RuntimePCH.h"
#include "CoroutineLibrary.h"

#include "World/World.h"
#include "World/Subsystems/CoroutineManager.h"

namespace Lumina
{
    namespace
    {
        // Owns the callback's handle, so a wait dropped without firing still frees it.
        struct FOneShotCallback
        {
            SScriptCallback Callback;

            explicit FOneShotCallback(SScriptCallback In) : Callback(In) {}
            FOneShotCallback(FOneShotCallback&& Other) noexcept : Callback(Other.Callback) { Other.Callback = {}; }
            FOneShotCallback(const FOneShotCallback&) = delete;
            FOneShotCallback& operator=(const FOneShotCallback&) = delete;
            FOneShotCallback& operator=(FOneShotCallback&&) = delete;
            ~FOneShotCallback() { Scripting::ReleaseScriptCallback(Callback); }

            void Fire(bool bCompleted)
            {
                SScriptCallback Taken = Callback;
                Callback = {};
                Scripting::InvokeScriptCallback(Taken, bCompleted ? 1 : 0);
            }
        };

        uint64 Schedule(CWorld* World, CoroDetail::FLatentWaitSpec&& Spec, SScriptCallback OnDone)
        {
            if (World == nullptr || !OnDone.IsBound())
            {
                Scripting::ReleaseScriptCallback(OnDone);
                return 0;
            }
            return World->GetCoroutineManager().AddWait(std::move(Spec), [Owned = FOneShotCallback(OnDone)](bool bCompleted) mutable
            {
                Owned.Fire(bCompleted);
            });
        }
    }

    uint64 CCoroutineLibrary::WaitTicks(CWorld* World, int32 Ticks, SScriptCallback OnDone)
    {
        CoroDetail::FLatentWaitSpec Spec;
        Spec.Kind  = CoroDetail::FLatentWaitSpec::EKind::Ticks;
        Spec.Ticks = Ticks;
        return Schedule(World, std::move(Spec), OnDone);
    }

    uint64 CCoroutineLibrary::WaitSeconds(CWorld* World, double Seconds, ECoroutineClock Clock, SScriptCallback OnDone)
    {
        CoroDetail::FLatentWaitSpec Spec;
        Spec.Kind    = CoroDetail::FLatentWaitSpec::EKind::Time;
        Spec.Clock   = Clock;
        Spec.Seconds = Seconds;
        return Schedule(World, std::move(Spec), OnDone);
    }

    void CCoroutineLibrary::CancelWait(CWorld* World, uint64 Wait)
    {
        if (World != nullptr && Wait != 0)
        {
            World->GetCoroutineManager().CancelWait(Wait);
        }
    }

    int32 CCoroutineLibrary::GetPendingCount(CWorld* World)
    {
        if (World == nullptr)
        {
            return 0;
        }
        return World->GetCoroutineManager().NumWaiting();
    }
}
