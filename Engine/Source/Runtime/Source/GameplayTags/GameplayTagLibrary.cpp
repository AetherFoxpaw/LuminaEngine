#include "RuntimePCH.h"

#include "GameplayTagLibrary.h"

#include "GameplayTags/GameplayTagComponent.h"
#include "GameplayTags/GameplayTagRegistry.h"
#include "World/World.h"

namespace Lumina
{
    namespace
    {
        // The component stores serializable name-backed tags, so an id has to be resolved to one first.
        SGameplayTag TagFromId(uint32 TagId)
        {
            SGameplayTag Tag;
            const FString Name = FGameplayTagRegistry::Get().GetName(TagId);
            if (!Name.empty())
            {
                Tag.TagName = FName(Name.c_str());
            }
            return Tag;
        }

        const SGameplayTagComponent* ComponentOf(CWorld* World, ECS::FEntity Entity, uint32 TagId)
        {
            if (World == nullptr || TagId == 0)
            {
                return nullptr;
            }
            return World->TryGetComponent<SGameplayTagComponent>(Entity);
        }
    }
    uint32 CGameplayTagLibrary::RequestTag(const FString& Name)
    {
        return Name.empty() ? 0u : FGameplayTagRegistry::Get().RequestTag(FStringView(Name.c_str(), Name.size()));
    }

    FString CGameplayTagLibrary::GetTagName(uint32 TagId)
    {
        return FGameplayTagRegistry::Get().GetName(TagId);
    }

    bool CGameplayTagLibrary::Matches(uint32 A, uint32 B)
    {
        return FGameplayTagRegistry::Get().Matches(A, B);
    }

    bool CGameplayTagLibrary::MatchesExact(uint32 A, uint32 B)
    {
        return FGameplayTagRegistry::Get().MatchesExact(A, B);
    }

    uint32 CGameplayTagLibrary::GetParent(uint32 TagId)
    {
        return FGameplayTagRegistry::Get().GetParent(TagId);
    }

    bool CGameplayTagLibrary::IsValidTag(uint32 TagId)
    {
        return FGameplayTagRegistry::Get().IsValid(TagId);
    }
}
