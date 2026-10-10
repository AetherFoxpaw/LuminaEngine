#include "ReflectedInstancedStructProperty.h"

#include "Reflector/CodeGeneration/CodeWriter.h"
#include "Reflector/Types/ReflectedType.h"

namespace Lumina
{
    void FReflectedInstancedStructProperty::AppendDefinition(Reflection::FCodeWriter& Writer) const
    {
        const std::string PropertyFlagStr = PropertyFlagsToString(PropertyFlags);

        // No type name means a bare FInstancedStruct, which has no base symbol to bind, so emit null.
        const std::string CustomData = TypeName.empty()
            ? std::string("nullptr")
            : ("Construct_CStruct_" + ClangUtils::MakeCodeFriendlyNamespace(TypeName));

        AppendPropertyDef(Writer, PropertyFlagStr.c_str(), "Lumina::EPropertyTypeFlags::InstancedStruct", CustomData);
    }
}
