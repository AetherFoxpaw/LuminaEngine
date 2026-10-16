#include <gtest/gtest.h>

#include "Assets/AssetTypes/PhysicsMaterial/PhysicsMaterial.h"
#include "Assets/AssetTypes/Sequence/Sequence.h"
#include "Assets/AssetTypes/SpriteSheet/SpriteSheet.h"
#include "Assets/AssetTypes/Weather/WeatherPreset.h"
#include "Core/Object/Class.h"
#include "Core/Object/ObjectCore.h"
#include "UI/Tools/EditorAssetDropHandlers.h"
#include "World/World.h"
#include "World/Entity/Components/AnimatedSprite3DComponent.h"
#include "World/Entity/Components/NameComponent.h"
#include "World/Entity/Components/PhysicsComponent.h"
#include "World/Entity/Components/SequencePlayerComponent.h"
#include "World/Entity/Components/Sprite3DComponent.h"
#include "World/Entity/Components/TransformComponent.h"
#include "World/Entity/Components/WeatherComponent.h"

using namespace Lumina;

namespace
{
    CWorld* NewDropTestWorld()
    {
        STransformComponent::StaticStruct();
        SNameComponent::StaticStruct();
        SBoxColliderComponent::StaticStruct();
        SSphereColliderComponent::StaticStruct();
        RegisterPendingTypes();

        CWorld* World = NewObject<CWorld>(nullptr, NAME_None, FGuid::New(), OF_Transient);
        ECS::GetWorldRegistry(*World).Ctx().Emplace<CWorld*>(World);
        return World;
    }

    template<typename T>
    T* NewTransientAsset(const char* Name)
    {
        return NewObject<T>(nullptr, FName(Name), FGuid::New(), OF_Transient);
    }

    ECS::FEntity Drop(CWorld* World, CObject* Asset, ECS::FEntity Target)
    {
        return FEditorAssetDropRegistry::Get().Drop(World, Asset, FTransform(), Target, /*bAttachToTarget*/ false);
    }
}

// With no handler for the class, the asset goes into the one field on the target that takes it.
TEST(EditorAssetDrop, FieldFallbackSetsTheOnlyMatchingField)
{
    CWorld* World = NewDropTestWorld();
    ECS::FRegistry& Registry = ECS::GetWorldRegistry(*World);
    const ECS::FEntity Entity = World->ConstructEntity("Crate");
    Registry.Emplace<SBoxColliderComponent>(Entity);

    CPhysicsMaterial* Ice = NewTransientAsset<CPhysicsMaterial>("DropIce");
    EXPECT_EQ(Drop(World, Ice, Entity), Entity);
    EXPECT_EQ(Registry.Get<SBoxColliderComponent>(Entity).PhysicsMaterial.Get(), Ice);

    CPhysicsMaterial* Rubber = NewTransientAsset<CPhysicsMaterial>("DropRubber");
    EXPECT_EQ(Drop(World, Rubber, Entity), Entity);
    EXPECT_EQ(Registry.Get<SBoxColliderComponent>(Entity).PhysicsMaterial.Get(), Rubber);
}

// Several fields fit, so drops fill the empty ones and then refuse rather than guess which to replace.
TEST(EditorAssetDrop, FieldFallbackFillsEmptyFieldsThenRefuses)
{
    CWorld* World = NewDropTestWorld();
    ECS::FRegistry& Registry = ECS::GetWorldRegistry(*World);
    const ECS::FEntity Entity = World->ConstructEntity("Compound");
    Registry.Emplace<SBoxColliderComponent>(Entity);
    Registry.Emplace<SSphereColliderComponent>(Entity);

    CPhysicsMaterial* First  = NewTransientAsset<CPhysicsMaterial>("DropFirst");
    CPhysicsMaterial* Second = NewTransientAsset<CPhysicsMaterial>("DropSecond");
    CPhysicsMaterial* Third  = NewTransientAsset<CPhysicsMaterial>("DropThird");

    EXPECT_EQ(Drop(World, First, Entity), Entity);
    EXPECT_EQ(Drop(World, Second, Entity), Entity);

    CObject* Box    = Registry.Get<SBoxColliderComponent>(Entity).PhysicsMaterial.Get();
    CObject* Sphere = Registry.Get<SSphereColliderComponent>(Entity).PhysicsMaterial.Get();
    EXPECT_TRUE((Box == First && Sphere == Second) || (Box == Second && Sphere == First));

    EXPECT_EQ(Drop(World, Third, Entity), ECS::NullEntity);
    EXPECT_NE(Registry.Get<SBoxColliderComponent>(Entity).PhysicsMaterial.Get(), Third);
    EXPECT_NE(Registry.Get<SSphereColliderComponent>(Entity).PhysicsMaterial.Get(), Third);
}

TEST(EditorAssetDrop, FieldFallbackWithNoTargetDoesNothing)
{
    CWorld* World = NewDropTestWorld();
    CPhysicsMaterial* Ice = NewTransientAsset<CPhysicsMaterial>("DropLoneIce");
    EXPECT_EQ(Drop(World, Ice, ECS::NullEntity), ECS::NullEntity);

    const ECS::FEntity Bare = World->ConstructEntity("Bare");
    EXPECT_EQ(Drop(World, Ice, Bare), ECS::NullEntity);
}

// A sequence spawns a player into empty space and retargets the player it lands on.
TEST(EditorAssetDrop, SequenceSpawnsThenRetargets)
{
    CWorld* World = NewDropTestWorld();
    ECS::FRegistry& Registry = ECS::GetWorldRegistry(*World);

    CSequence* Intro = NewTransientAsset<CSequence>("DropIntro");
    const ECS::FEntity Player = Drop(World, Intro, ECS::NullEntity);
    ASSERT_NE(Player, ECS::NullEntity);
    EXPECT_EQ(Registry.Get<SSequencePlayerComponent>(Player).Sequence.Get(), Intro);

    CSequence* Outro = NewTransientAsset<CSequence>("DropOutro");
    EXPECT_EQ(Drop(World, Outro, Player), Player);
    EXPECT_EQ(Registry.Get<SSequencePlayerComponent>(Player).Sequence.Get(), Outro);
}

// Weather is one per world, so a second preset swaps onto the first weather entity wherever it was dropped.
TEST(EditorAssetDrop, WeatherSwapsTheExistingPreset)
{
    CWorld* World = NewDropTestWorld();
    ECS::FRegistry& Registry = ECS::GetWorldRegistry(*World);

    CWeatherPreset* Rain = NewTransientAsset<CWeatherPreset>("DropRain");
    const ECS::FEntity Weather = Drop(World, Rain, ECS::NullEntity);
    ASSERT_NE(Weather, ECS::NullEntity);

    const ECS::FEntity Elsewhere = World->ConstructEntity("Elsewhere");
    CWeatherPreset* Snow = NewTransientAsset<CWeatherPreset>("DropSnow");
    EXPECT_EQ(Drop(World, Snow, Elsewhere), Weather);
    EXPECT_EQ(Registry.Get<SWeatherComponent>(Weather).Preset.Get(), Snow);
    EXPECT_FALSE(Registry.HasAll<SWeatherComponent>(Elsewhere));
}

// The animated sprite only drives playback, so the spawned entity also needs the sprite that draws it.
TEST(EditorAssetDrop, SpriteSheetSpawnsADrawingSprite)
{
    CWorld* World = NewDropTestWorld();
    ECS::FRegistry& Registry = ECS::GetWorldRegistry(*World);

    CSpriteSheet* Sheet = NewTransientAsset<CSpriteSheet>("DropSheet");
    Sheet->HFrames = 4;
    Sheet->VFrames = 2;

    const ECS::FEntity Entity = Drop(World, Sheet, ECS::NullEntity);
    ASSERT_NE(Entity, ECS::NullEntity);
    EXPECT_EQ(Registry.Get<SAnimatedSprite3DComponent>(Entity).SpriteSheet.Get(), Sheet);
    ASSERT_TRUE(Registry.HasAll<SSprite3DComponent>(Entity));
    EXPECT_EQ(Registry.Get<SSprite3DComponent>(Entity).HFrames, 4);
    EXPECT_EQ(Registry.Get<SSprite3DComponent>(Entity).VFrames, 2);
}
