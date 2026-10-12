#include "EntityScriptPicker.h"

#include "Containers/HashTable.h"
#include "Containers/StringFormat.h"
#include "Core/Object/Class.h"
#include "Core/Object/ObjectIterator.h"
#include "Scripting/DotNet/DotNetHost.h"
#include "Scripting/EntityScript.h"
#include "Tools/UI/ImGui/ImGuiDesignIcons.h"

#include <cctype>

namespace Lumina::EntityScriptPicker
{
    namespace
    {
        FString LowercaseScriptStem(FStringView Text)
        {
            FString Lower(Text.data(), Text.size());
            for (char& Character : Lower)
            {
                Character = (char)std::tolower((unsigned char)Character);
            }
            return Lower;
        }
    }

    void GatherItems(TVector<FPickerItem>& Out)
    {
        Out.clear();

        // One pass over the script roots, since a lookup per class would walk them once for every row.
        TVector<DotNet::FScriptSourceFolder> Sources;
        DotNet::GatherScriptSourceFolders(Sources);
        THashMap<FString, const DotNet::FScriptSourceFolder*> SourceByStem;
        for (const DotNet::FScriptSourceFolder& Source : Sources)
        {
            SourceByStem.emplace(LowercaseScriptStem(Source.Stem), &Source);
        }

        TVector<FString> ManagedNames;
        DotNet::GatherEntityScriptTypes(ManagedNames);
        THashSet<FString> Managed;
        for (const FString& Name : ManagedNames)
        {
            Managed.insert(Name);
        }

        CClass* Base = CEntityScript::StaticClass();
        for (TObjectIterator<CClass> It; It; ++It)
        {
            CClass* Class = *It;
            if (Class == nullptr || Class == Base || !Class->IsChildOf(Base)
                || Class->HasAnyFlag(OF_DefaultObject) || Class->HasAnyFlag(OF_MarkedDestroy))
            {
                continue;
            }

            const FString FullName = Class->GetName().ToString();
            const FStringView View(FullName);
            const size_t Dot = View.find_last_of('.');
            const FStringView Short = Dot == FStringView::npos ? View : View.substr(Dot + 1);
            const bool bManaged = Managed.find(FullName) != Managed.end();

            FPickerItem& Item = Out.emplace_back();
            Item.Name = FString(Short.data(), Short.size());
            Item.Key  = FullName;
            Item.Icon = bManaged ? LE_ICON_LANGUAGE_CSHARP : LE_ICON_LANGUAGE_CPP;

            auto Source = SourceByStem.find(LowercaseScriptStem(Short));
            if (bManaged && Source != SourceByStem.end())
            {
                Item.Category = Source->second->Folder;
                Item.Tooltip  = Lumina::Format("{}\n{}", FullName, Source->second->Path);
            }
            else
            {
                // A C++ script has no file under the script roots, so it files under its own heading.
                Item.Category = bManaged ? FString() : FString(NativeCategory);
                Item.Tooltip  = FullName;
            }
        }
    }
}
