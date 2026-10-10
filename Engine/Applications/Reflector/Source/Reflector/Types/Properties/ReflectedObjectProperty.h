#pragma once
#include "ReflectedProperty.h"
#include "Reflector/Clang/Utils.h"
#include "Reflector/CodeGeneration/CodeWriter.h"
#include "Reflector/CodeGeneration/ReflectionNames.h"

namespace Lumina
{
    class FReflectedObjectProperty : public FReflectedProperty
    {
    public:

        const char* GetTypeName() override { return "Object"; }
        const char* GetPropertyParamType() const override { return "FObjectPropertyParams"; }

        void AppendDefinition(Reflection::FCodeWriter& Writer) const override;

        void DeclareCrossModuleReference(const std::string& API, Reflection::FCodeWriter& Writer) override
        {
            const std::string Friendly = ClangUtils::MakeCodeFriendlyNamespace(TypeName);
            const std::string FnName = "Construct_CClass_" + Friendly;
            Reflection::Names::EmitGuardedCrossModuleDecl(Writer, API, "CClass", FnName);
        }
    };
}
