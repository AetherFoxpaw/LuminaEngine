#include "RuntimePCH.h"

#include "TweenLibrary.h"

#include "Memory/SmartPtr.h"
#include "World/Subsystems/TweenManager.h"
#include "World/World.h"
#include "World/Entity/Components/Component.h"

namespace Lumina
{
    namespace
    {
        // Shared with whatever step captured it, so the managed handle frees when the tween is destroyed.
        using FTweenCallback = TSharedPtr<FScriptCallbackOwner>;

        FTweenCallback TweenLibraryMakeCallback(SScriptCallback Callback)
        {
            return MakeShared<FScriptCallbackOwner>(Callback);
        }

        // Null for a world that has not been initialized, which owns no tween manager yet.
        FTweenManager* FindTweenManager(CWorld* World)
        {
            return World != nullptr ? World->GetEntityRegistry().Ctx().Find<FTweenManager>() : nullptr;
        }

        // Rebuilt per call, since the builder is just a manager pointer plus a generational id.
        bool ResolveTween(CWorld* World, uint32 Id, FTween& Out)
        {
            FTweenManager* Manager = FindTweenManager(World);
            if (Manager == nullptr)
            {
                return false;
            }

            FTweenHandle Handle;
            Handle.Handle = ECS::FEntity::FromPacked(Id);
            Out = FTween(Manager, Handle);
            return true;
        }

        template <typename T>
        void AddValueByAddress(CWorld* World, uint32 Tween, const T& From, const T& To, float Duration, SScriptCallback OnValue)
        {
            FTween Builder;
            if (!ResolveTween(World, Tween, Builder))
            {
                Scripting::ReleaseScriptCallback(OnValue);
                return;
            }

            FTweenCallback Owner = TweenLibraryMakeCallback(OnValue);
            Builder.To(From, To, Duration, [Owner](const T& Value)
            {
                Owner->Invoke((uint64)(uintptr_t)&Value);
            });
        }

        template <typename T>
        void AddProperty(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, const T& To, float Duration)
        {
            FTween Builder;
            if (ResolveTween(World, Tween, Builder))
            {
                Builder.PropertyTo(Target, FindComponentStruct(Component), Path, To, Duration);
            }
        }
    }

    uint32 CTweenLibrary::Create(CWorld* World, ECS::FEntity Owner)
    {
        FTweenManager* Manager = FindTweenManager(World);
        if (Manager == nullptr)
        {
            return (uint32)ECS::NullEntity;
        }

        const FTween Tween = Owner != ECS::NullEntity ? Manager->CreateForEntity(Owner) : Manager->Create();
        return (uint32)Tween.GetHandle().Handle;
    }

    void CTweenLibrary::MoveTo(CWorld* World, uint32 Tween, ECS::FEntity Target, FVector3 Destination, float Duration)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.MoveTo(Target, Destination, Duration);
        }
    }

    void CTweenLibrary::RotateTo(CWorld* World, uint32 Tween, ECS::FEntity Target, FQuat Destination, float Duration)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.RotateTo(Target, Destination, Duration);
        }
    }

    void CTweenLibrary::ScaleTo(CWorld* World, uint32 Tween, ECS::FEntity Target, FVector3 Destination, float Duration)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.ScaleTo(Target, Destination, Duration);
        }
    }

    void CTweenLibrary::ValueTo(CWorld* World, uint32 Tween, float From, float To, float Duration, SScriptCallback OnValue)
    {
        FTween Builder;
        if (!ResolveTween(World, Tween, Builder))
        {
            Scripting::ReleaseScriptCallback(OnValue);
            return;
        }

        FTweenCallback Owner = TweenLibraryMakeCallback(OnValue);
        Builder.To(From, To, Duration, [Owner](const float& Value)
        {
            Owner->Invoke(Scripting::PackFloatPayload(Value));
        });
    }

    void CTweenLibrary::Interval(CWorld* World, uint32 Tween, float Duration)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Interval(Duration);
        }
    }

    void CTweenLibrary::Call(CWorld* World, uint32 Tween, SScriptCallback OnCall)
    {
        FTween Builder;
        if (!ResolveTween(World, Tween, Builder))
        {
            Scripting::ReleaseScriptCallback(OnCall);
            return;
        }

        FTweenCallback Owner = TweenLibraryMakeCallback(OnCall);
        Builder.Call([Owner] { Owner->Invoke(0); });
    }

    void CTweenLibrary::OnFinished(CWorld* World, uint32 Tween, SScriptCallback OnFinished)
    {
        FTween Builder;
        if (!ResolveTween(World, Tween, Builder))
        {
            Scripting::ReleaseScriptCallback(OnFinished);
            return;
        }

        FTweenCallback Owner = TweenLibraryMakeCallback(OnFinished);
        Builder.OnFinished([Owner] { Owner->Invoke(0); });
    }

    void CTweenLibrary::Trans(CWorld* World, uint32 Tween, EEaseTransition Transition)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Trans(Transition);
        }
    }

    void CTweenLibrary::Ease(CWorld* World, uint32 Tween, EEaseType Ease)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Ease(Ease);
        }
    }

    void CTweenLibrary::Delay(CWorld* World, uint32 Tween, float Seconds)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Delay(Seconds);
        }
    }

    void CTweenLibrary::Parallel(CWorld* World, uint32 Tween)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Parallel();
        }
    }

    void CTweenLibrary::SetLoops(CWorld* World, uint32 Tween, int32 Count)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.SetLoops(Count);
        }
    }

    void CTweenLibrary::SetSpeedScale(CWorld* World, uint32 Tween, float Scale)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.SetSpeedScale(Scale);
        }
    }

    void CTweenLibrary::SetPaused(CWorld* World, uint32 Tween, bool bPaused)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.SetPaused(bPaused);
        }
    }

    void CTweenLibrary::Kill(CWorld* World, uint32 Tween)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Kill();
        }
    }

    bool CTweenLibrary::IsRunning(CWorld* World, uint32 Tween)
    {
        FTween Builder;
        return ResolveTween(World, Tween, Builder) && Builder.IsRunning();
    }

    void CTweenLibrary::MoveBy(CWorld* World, uint32 Tween, ECS::FEntity Target, FVector3 Offset, float Duration)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.MoveBy(Target, Offset, Duration);
        }
    }

    void CTweenLibrary::RotateBy(CWorld* World, uint32 Tween, ECS::FEntity Target, FQuat Delta, float Duration)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.RotateBy(Target, Delta, Duration);
        }
    }

    void CTweenLibrary::ScaleBy(CWorld* World, uint32 Tween, ECS::FEntity Target, FVector3 Factor, float Duration)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.ScaleBy(Target, Factor, Duration);
        }
    }

    void CTweenLibrary::Punch(CWorld* World, uint32 Tween, ECS::FEntity Target, ETweenTransformChannel Channel, FVector3 Punch, float Duration, int32 Vibrato, float Elasticity)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Punch(Target, Channel, Punch, Duration, Vibrato, Elasticity);
        }
    }

    void CTweenLibrary::Shake(CWorld* World, uint32 Tween, ECS::FEntity Target, ETweenTransformChannel Channel, FVector3 Strength, float Duration, int32 Vibrato)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Shake(Target, Channel, Strength, Duration, Vibrato);
        }
    }

    void CTweenLibrary::ValueToVector2(CWorld* World, uint32 Tween, FVector2 From, FVector2 To, float Duration, SScriptCallback OnValue)
    {
        AddValueByAddress(World, Tween, From, To, Duration, OnValue);
    }

    void CTweenLibrary::ValueToVector3(CWorld* World, uint32 Tween, FVector3 From, FVector3 To, float Duration, SScriptCallback OnValue)
    {
        AddValueByAddress(World, Tween, From, To, Duration, OnValue);
    }

    void CTweenLibrary::ValueToVector4(CWorld* World, uint32 Tween, FVector4 From, FVector4 To, float Duration, SScriptCallback OnValue)
    {
        AddValueByAddress(World, Tween, From, To, Duration, OnValue);
    }

    void CTweenLibrary::ValueToQuat(CWorld* World, uint32 Tween, FQuat From, FQuat To, float Duration, SScriptCallback OnValue)
    {
        AddValueByAddress(World, Tween, From, To, Duration, OnValue);
    }

    void CTweenLibrary::PropertyToFloat(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, float To, float Duration)
    {
        AddProperty(World, Tween, Target, Component, Path, To, Duration);
    }

    void CTweenLibrary::PropertyToVector2(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, FVector2 To, float Duration)
    {
        AddProperty(World, Tween, Target, Component, Path, To, Duration);
    }

    void CTweenLibrary::PropertyToVector3(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, FVector3 To, float Duration)
    {
        AddProperty(World, Tween, Target, Component, Path, To, Duration);
    }

    void CTweenLibrary::PropertyToVector4(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, FVector4 To, float Duration)
    {
        AddProperty(World, Tween, Target, Component, Path, To, Duration);
    }

    void CTweenLibrary::PropertyToQuat(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, FQuat To, float Duration)
    {
        AddProperty(World, Tween, Target, Component, Path, To, Duration);
    }

    void CTweenLibrary::OnLoop(CWorld* World, uint32 Tween, SScriptCallback OnLoop)
    {
        FTween Builder;
        if (!ResolveTween(World, Tween, Builder))
        {
            Scripting::ReleaseScriptCallback(OnLoop);
            return;
        }

        FTweenCallback Owner = TweenLibraryMakeCallback(OnLoop);
        Builder.OnLoop([Owner](int32 Loops) { Owner->Invoke((uint64)(uint32)Loops); });
    }

    void CTweenLibrary::OnStep(CWorld* World, uint32 Tween, SScriptCallback OnStep)
    {
        FTween Builder;
        if (!ResolveTween(World, Tween, Builder))
        {
            Scripting::ReleaseScriptCallback(OnStep);
            return;
        }

        FTweenCallback Owner = TweenLibraryMakeCallback(OnStep);
        Builder.OnStep([Owner](int32 Step) { Owner->Invoke((uint64)(uint32)Step); });
    }

    void CTweenLibrary::EaseCurve(CWorld* World, uint32 Tween, CCurveAsset* Curve)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.EaseCurve(Curve);
        }
    }

    void CTweenLibrary::SetLoopMode(CWorld* World, uint32 Tween, ETweenLoopMode Mode)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.SetLoopMode(Mode);
        }
    }

    void CTweenLibrary::SetIgnoreTimeScale(CWorld* World, uint32 Tween, bool bIgnore)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.SetIgnoreTimeScale(bIgnore);
        }
    }

    void CTweenLibrary::Complete(CWorld* World, uint32 Tween)
    {
        FTween Builder;
        if (ResolveTween(World, Tween, Builder))
        {
            Builder.Complete();
        }
    }

    float CTweenLibrary::GetElapsed(CWorld* World, uint32 Tween)
    {
        FTween Builder;
        return ResolveTween(World, Tween, Builder) ? Builder.GetElapsed() : 0.0f;
    }

    float CTweenLibrary::GetDuration(CWorld* World, uint32 Tween)
    {
        FTween Builder;
        return ResolveTween(World, Tween, Builder) ? Builder.GetDuration() : 0.0f;
    }

    float CTweenLibrary::GetProgress(CWorld* World, uint32 Tween)
    {
        FTween Builder;
        return ResolveTween(World, Tween, Builder) ? Builder.GetProgress() : 0.0f;
    }

    int32 CTweenLibrary::GetCompletedLoops(CWorld* World, uint32 Tween)
    {
        FTween Builder;
        return ResolveTween(World, Tween, Builder) ? Builder.GetCompletedLoops() : 0;
    }
}
