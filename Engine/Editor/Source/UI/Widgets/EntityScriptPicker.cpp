#include "EntityScriptPicker.h"

#include "Containers/StringFormat.h"
#include "Core/Object/Class.h"
#include "Core/Object/ObjectIterator.h"
#include "Core/Object/ScriptClass.h"
#include "Scripting/EntityScript.h"
#include "Tools/UI/ImGui/ImGuiDesignIcons.h"

#include <cctype>

namespace Lumina::EntityScriptPicker
{
    namespace
    {
        FString NormalizeSourceKey(FStringView Text)
        {
            FString Lower(Text.data(), Text.size());
            for (char& Character : Lower)
            {
                Character = Character == '\\' ? '/' : (char)std::tolower((unsigned char)Character);
            }
            return Lower;
        }
    }

    FScriptSourceIndex::FScriptSourceIndex()
    {
        DotNet::GatherScriptSourceFolders(Sources);
        for (const DotNet::FScriptSourceFolder& Source : Sources)
        {
            ByStem.emplace(NormalizeSourceKey(Source.Stem), &Source);
            ByPath.emplace(NormalizeSourceKey(Source.Path), &Source);
        }
    }

    const DotNet::FScriptSourceFolder* FScriptSourceIndex::Find(const CStruct* Type, FStringView ShortName) const
    {
        if (Type->HasMeta("SourceFile"))
        {
            auto Found = ByPath.find(NormalizeSourceKey(Type->GetMeta("SourceFile")));
            if (Found != ByPath.end())
            {
                return Found->second;
            }
        }
        auto Found = ByStem.find(NormalizeSourceKey(ShortName));
        return Found != ByStem.end() ? Found->second : nullptr;
    }

    void GatherItems(TVector<FPickerItem>& Out)
    {
        Out.clear();
        const FScriptSourceIndex SourceIndex;

        CClass* Base = CEntityScript::StaticClass();
        for (TObjectIterator<CClass> It; It; ++It)
        {
            CClass* Class = *It;
            if (Class == nullptr || Class == Base || !Class->IsChildOf(Base)
                || Class->HasAnyFlag(OF_DefaultObject) || Class->HasAnyFlag(OF_MarkedDestroy)
                || Class->IsAbstract() || Class->IsSuperseded())
            {
                continue;
            }

            const FString FullName = Class->GetName().ToString();
            const FStringView Short = ShortTypeName(FullName);
            const bool bManaged = ToScriptClass(Class) != nullptr;

            FPickerItem& Item = Out.emplace_back();
            Item.Name = Class->HasMeta("DisplayName") ? Class->GetMeta("DisplayName") : FString(Short.data(), Short.size());
            Item.Key  = FullName;
            Item.Icon = bManaged ? LE_ICON_LANGUAGE_CSHARP : LE_ICON_LANGUAGE_CPP;

            const DotNet::FScriptSourceFolder* Source = bManaged ? SourceIndex.Find(Class, Short) : nullptr;
            if (Source != nullptr)
            {
                Item.Category = Source->Folder;
                Item.Tooltip  = Lumina::Format("{}\n{}", FullName, Source->Path);
            }
            else
            {
                // A C++ script has no file under the script roots, so it files under its own heading.
                Item.Category = bManaged ? FString() : FString(NativeCategory);
                Item.Tooltip  = FullName;
            }

            // A declared REFLECT(Category) or ToolTip wins over where the source file happens to sit.
            if (Class->HasMeta("Category"))
            {
                Item.Category = Class->GetMeta("Category");
            }
            if (Class->HasMeta("ToolTip"))
            {
                Item.Tooltip = Lumina::Format("{}\n{}", Class->GetMeta("ToolTip"), Item.Tooltip);
            }
        }
    }
}
