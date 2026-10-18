#pragma once

#include "Containers/Name.h"
#include "Containers/String.h"
#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "GameLibrary.generated.h"

namespace Lumina
{
    class CWorld;
    class CWorldSubsystem;

    // Engine level session operations. OpenLevel takes the calling world, since editor PIE runs one world per player.
    REFLECT()
    class RUNTIME_API CGameLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        // The swap runs at the next frame start, so calling this mid tick is safe.
        FUNCTION()
        static void OpenLevel(CWorld* World, const FString& Url);

        /** Ends the play session in the editor and exits the process in a packaged game, both deferred. */
        FUNCTION()
        static void QuitGame();

        // True for a dedicated server or load-test process, which never opens a window or an audio device.
        FUNCTION()
        static bool IsHeadless();
    };
}
