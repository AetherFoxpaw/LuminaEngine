#include "RuntimePCH.h"

#include "GameLibrary.h"

#include "Core/Engine/Engine.h"
#include "Core/Object/Class.h"
#include "Core/Object/ObjectCore.h"
#include "World/World.h"

namespace Lumina
{
    void CGameLibrary::OpenLevel(CWorld* World, const FString& Url)
    {
        if (GEngine != nullptr && !Url.empty())
        {
            GEngine->OpenLevel(FURL::Parse(FStringView(Url.c_str(), Url.size())), World);
        }
    }

    void CGameLibrary::QuitGame()
    {
        if (GEngine != nullptr)
        {
            GEngine->RequestExitGame();
        }
    }

    bool CGameLibrary::IsHeadless()
    {
        return GIsHeadless;
    }
}
