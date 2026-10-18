#pragma once

#include "AI/Navigation/NavTypes.h"
#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "Core/Math/Vector/VectorTypes.h"
#include "NavigationLibrary.generated.h"

namespace Lumina
{
    class CWorld;

    // With no baked navmesh every query reports not found. Agent names the volume to query, where empty means the infantry default.
    REFLECT()
    class RUNTIME_API CNavigationLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        // Reason text for a path result, ready to drop into a log line.
        FUNCTION()
        static FString DescribePathResult(ENavPathResult Result);
    };
}
