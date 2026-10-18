#pragma once

#include "Core/Math/Easing.h"
#include "Core/Math/Vector/VectorTypes.h"
#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "Scripting/ScriptCallback.h"
#include "Containers/String.h"
#include "World/ECS/Entity.h"
#include "World/Subsystems/TweenTypes.h"
#include "TweenLibrary.generated.h"

namespace Lumina
{
    class CCurveAsset;
    class CWorld;

    /** Building a tween a step at a time. Every call takes the id Create returned; a stale one is a no-op. */
    REFLECT()
    class RUNTIME_API CTweenLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        /** Starts an empty tween and returns its id. Owner ties its lifetime to that entity. */
        FUNCTION()
        static uint32 Create(CWorld* World, ECS::FEntity Owner);

        FUNCTION()
        static void MoveTo(CWorld* World, uint32 Tween, ECS::FEntity Target, FVector3 Destination, float Duration);

        FUNCTION()
        static void RotateTo(CWorld* World, uint32 Tween, ECS::FEntity Target, FQuat Destination, float Duration);

        FUNCTION()
        static void ScaleTo(CWorld* World, uint32 Tween, ECS::FEntity Target, FVector3 Destination, float Duration);

        // Relative to wherever Target is when the step begins. ScaleBy multiplies, so a factor of two doubles.
        FUNCTION()
        static void MoveBy(CWorld* World, uint32 Tween, ECS::FEntity Target, FVector3 Offset, float Duration);

        FUNCTION()
        static void RotateBy(CWorld* World, uint32 Tween, ECS::FEntity Target, FQuat Delta, float Duration);

        FUNCTION()
        static void ScaleBy(CWorld* World, uint32 Tween, ECS::FEntity Target, FVector3 Factor, float Duration);

        // A kick toward Punch that swings back and settles, rotation in degrees. Vibrato counts the swings.
        FUNCTION()
        static void Punch(CWorld* World, uint32 Tween, ECS::FEntity Target, ETweenTransformChannel Channel, FVector3 Punch, float Duration, int32 Vibrato, float Elasticity);

        // A decaying random wobble of up to Strength on each axis, rotation in degrees.
        FUNCTION()
        static void Shake(CWorld* World, uint32 Tween, ECS::FEntity Target, ETweenTransformChannel Channel, FVector3 Strength, float Duration, int32 Vibrato);

        /** Interpolates a bare number, reporting each value to OnValue until the tween is destroyed. */
        FUNCTION()
        static void ValueTo(CWorld* World, uint32 Tween, float From, float To, float Duration, SScriptCallback OnValue);

        // The payload is the address of the value, read by the callback before it returns.
        FUNCTION()
        static void ValueToVector2(CWorld* World, uint32 Tween, FVector2 From, FVector2 To, float Duration, SScriptCallback OnValue);

        FUNCTION()
        static void ValueToVector3(CWorld* World, uint32 Tween, FVector3 From, FVector3 To, float Duration, SScriptCallback OnValue);

        FUNCTION()
        static void ValueToVector4(CWorld* World, uint32 Tween, FVector4 From, FVector4 To, float Duration, SScriptCallback OnValue);

        // Slerps, so a wide turn follows the arc.
        FUNCTION()
        static void ValueToQuat(CWorld* World, uint32 Tween, FQuat From, FQuat To, float Duration, SScriptCallback OnValue);

        // Drives a reflected property on Target's component, named by type and a dotted path, with no script call per frame.
        FUNCTION()
        static void PropertyToFloat(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, float To, float Duration);

        FUNCTION()
        static void PropertyToVector2(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, FVector2 To, float Duration);

        FUNCTION()
        static void PropertyToVector3(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, FVector3 To, float Duration);

        FUNCTION()
        static void PropertyToVector4(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, FVector4 To, float Duration);

        FUNCTION()
        static void PropertyToQuat(CWorld* World, uint32 Tween, ECS::FEntity Target, const FString& Component, const FString& Path, FQuat To, float Duration);

        /** Dead time, for spacing steps apart. */
        FUNCTION()
        static void Interval(CWorld* World, uint32 Tween, float Duration);

        /** Runs OnCall when the sequence reaches this step. */
        FUNCTION()
        static void Call(CWorld* World, uint32 Tween, SScriptCallback OnCall);

        /** Runs OnFinished after the last step, including after the final loop. */
        FUNCTION()
        static void OnFinished(CWorld* World, uint32 Tween, SScriptCallback OnFinished);

        // Runs as each pass ends and another begins, with the passes completed so far as the payload.
        FUNCTION()
        static void OnLoop(CWorld* World, uint32 Tween, SScriptCallback OnLoop);

        // Runs as each step ends, with that step's index as the payload.
        FUNCTION()
        static void OnStep(CWorld* World, uint32 Tween, SScriptCallback OnStep);

        /** Transition, Ease and Delay all apply to the step that was added last. */
        FUNCTION()
        static void Trans(CWorld* World, uint32 Tween, EEaseTransition Transition);

        FUNCTION()
        static void Ease(CWorld* World, uint32 Tween, EEaseType Ease);

        // Shapes the last step with the curve over its keyed time range, replacing Trans and Ease.
        FUNCTION()
        static void EaseCurve(CWorld* World, uint32 Tween, CCurveAsset* Curve);

        FUNCTION()
        static void Delay(CWorld* World, uint32 Tween, float Seconds);

        /** Runs the last step alongside the one before it rather than after it. */
        FUNCTION()
        static void Parallel(CWorld* World, uint32 Tween);

        /** Zero or less loops forever. */
        FUNCTION()
        static void SetLoops(CWorld* World, uint32 Tween, int32 Count);

        FUNCTION()
        static void SetLoopMode(CWorld* World, uint32 Tween, ETweenLoopMode Mode);

        FUNCTION()
        static void SetSpeedScale(CWorld* World, uint32 Tween, float Scale);

        // Runs on real time, through world pause and time dilation.
        FUNCTION()
        static void SetIgnoreTimeScale(CWorld* World, uint32 Tween, bool bIgnore);

        FUNCTION()
        static void SetPaused(CWorld* World, uint32 Tween, bool bPaused);

        /** Stops where it is; whatever it was driving keeps its current value. */
        FUNCTION()
        static void Kill(CWorld* World, uint32 Tween);

        // Jumps to the end of the current pass, running every step and OnFinished on the way.
        FUNCTION()
        static void Complete(CWorld* World, uint32 Tween);

        FUNCTION()
        static bool IsRunning(CWorld* World, uint32 Tween);

        // Seconds into the current pass.
        FUNCTION()
        static float GetElapsed(CWorld* World, uint32 Tween);

        // The length of one pass.
        FUNCTION()
        static float GetDuration(CWorld* World, uint32 Tween);

        FUNCTION()
        static float GetProgress(CWorld* World, uint32 Tween);

        FUNCTION()
        static int32 GetCompletedLoops(CWorld* World, uint32 Tween);
    };
}
