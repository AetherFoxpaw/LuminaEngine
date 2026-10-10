#pragma once
#include "ReflectedProperty.h"
#include "Reflector/CodeGeneration/CodeWriter.h"

namespace Lumina
{
#define DEFINE_REFLECTED_NUMERIC_PROPERTY(ClassName, TypeFlag, TypeNameStr) \
    class ClassName final : public FReflectedProperty \
    { \
    public: \
        void AppendDefinition(Reflection::FCodeWriter& Writer) const override \
        { \
            const std::string PropertyFlagStr = PropertyFlagsToString(PropertyFlags); \
            AppendPropertyDef(Writer, PropertyFlagStr.c_str(), #TypeFlag); \
        } \
        const char* GetTypeName() override { return TypeNameStr; } \
        const char* GetPropertyParamType() const override { return "FNumericPropertyParams"; } \
    };

    // Unsigned integer properties
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedUInt8Property,  Lumina::EPropertyTypeFlags::UInt8,  "UInt8")
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedUInt16Property, Lumina::EPropertyTypeFlags::UInt16, "UInt16")
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedUInt32Property, Lumina::EPropertyTypeFlags::UInt32, "UInt32")
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedUInt64Property, Lumina::EPropertyTypeFlags::UInt64, "UInt64")

    // Signed integer properties
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedInt8Property,   Lumina::EPropertyTypeFlags::Int8,   "Int8")
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedInt16Property,  Lumina::EPropertyTypeFlags::Int16,  "Int16")
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedInt32Property,  Lumina::EPropertyTypeFlags::Int32,  "Int32")
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedInt64Property,  Lumina::EPropertyTypeFlags::Int64,  "Int64")

    // An ECS::FEntity, which shares the numeric params since it is stored as its packed uint32.
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedEntityProperty, Lumina::EPropertyTypeFlags::Entity, "Entity")

    // Floating point + bool properties
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedFloatProperty,  Lumina::EPropertyTypeFlags::Float,  "Float")
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedDoubleProperty, Lumina::EPropertyTypeFlags::Double, "Double")
    DEFINE_REFLECTED_NUMERIC_PROPERTY(FReflectedBoolProperty,   Lumina::EPropertyTypeFlags::Bool,   "Bool")

    #undef DEFINE_REFLECTED_NUMERIC_PROPERTY
}
