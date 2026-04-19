/*
 * Copyright (c) 2026 Heathen Engineering Limited
 * Irish Registered Company #556277
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <FoundationOgham/OghamTypes.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Script/ScriptContext.h>

// GameplayTag types are already reflected by FoundationGameplayTagsSystemComponent.
// We only need to pull in the headers here for type resolution.
#include <GameplayTagCondition.h>
#include <GameplayTagOperation.h>

namespace FoundationOgham
{
    ////////////////////////////////////////////////////////////////////////////
    // HistoryEntry

    void HistoryEntry::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<HistoryEntry>()
                ->Version(1)
                ->Field("EntryId",        &HistoryEntry::entryId)
                ->Field("SelectedOption", &HistoryEntry::selectedOption);

            if (auto* edit = serialize->GetEditContext())
            {
                edit->Class<HistoryEntry>("History Entry",
                        "One step in the narrative history.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HistoryEntry::entryId,
                        "Entry ID", "Hash of the entry tag")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &HistoryEntry::selectedOption,
                        "Selected Option", "Hash of the option chosen; 0 = interrupted");
            }
        }

        if (auto* behavior = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behavior->Class<HistoryEntry>("Ogham History Entry")
                ->Attribute(AZ::Script::Attributes::Category, "Ogham")
                ->Constructor<>()
                ->Property("entryId",        BehaviorValueProperty(&HistoryEntry::entryId))
                ->Property("selectedOption", BehaviorValueProperty(&HistoryEntry::selectedOption));
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // DialogueOption

    void DialogueOption::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<DialogueOption>()
                ->Version(1)
                ->Field("Tag",         &DialogueOption::tag)
                ->Field("TextKey",     &DialogueOption::textKey)
                ->Field("TargetEntry", &DialogueOption::targetEntry)
                ->Field("Conditions",  &DialogueOption::conditions)
                ->Field("Operations",  &DialogueOption::operations);

            if (auto* edit = serialize->GetEditContext())
            {
                edit->Class<DialogueOption>("Dialogue Option",
                        "A selectable option within a dialogue entry.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueOption::tag,
                        "Tag", "Unique identifier for this option")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueOption::textKey,
                        "Text Key", "Localisation key for the option label")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueOption::targetEntry,
                        "Target Entry",
                        "Entry to navigate to on selection. Invalid (0) = close conversation.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueOption::conditions,
                        "Conditions",
                        "If non-empty, all must be satisfied for this option to be available.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueOption::operations,
                        "Operations", "State changes applied when this option is selected.");
            }
        }

        if (auto* behavior = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behavior->Class<DialogueOption>("Dialogue Option")
                ->Attribute(AZ::Script::Attributes::Category, "Ogham")
                ->Constructor<>()
                ->Property("tag",         BehaviorValueProperty(&DialogueOption::tag))
                ->Property("textKey",     BehaviorValueProperty(&DialogueOption::textKey))
                ->Property("targetEntry", BehaviorValueProperty(&DialogueOption::targetEntry))
                ->Property("conditions",  BehaviorValueProperty(&DialogueOption::conditions))
                ->Property("operations",  BehaviorValueProperty(&DialogueOption::operations));
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // DialogueEntry

    void DialogueEntry::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<DialogueEntry>()
                ->Version(1)
                ->Field("Tag",             &DialogueEntry::tag)
                ->Field("ParentTag",       &DialogueEntry::parentTag)
                ->Field("TextKeys",        &DialogueEntry::textKeys)
                ->Field("EntryOperations", &DialogueEntry::entryOperations)
                ->Field("Options",         &DialogueEntry::options);

            if (auto* edit = serialize->GetEditContext())
            {
                edit->Class<DialogueEntry>("Dialogue Entry",
                        "A single node in the narrative graph.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueEntry::tag,
                        "Tag", "Unique identifier for this entry")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueEntry::parentTag,
                        "Parent Tag",
                        "Hierarchical parent; used by ReturnTo(). Leave invalid for root entries.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueEntry::textKeys,
                        "Text Keys",
                        "Localisation keys for display strings (narrator, body, speaker, etc.)")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueEntry::entryOperations,
                        "Entry Operations",
                        "State changes applied when this entry is entered.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DialogueEntry::options,
                        "Options", "The selectable choices available in this entry.");
            }
        }

        if (auto* behavior = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behavior->Class<DialogueEntry>("Dialogue Entry")
                ->Attribute(AZ::Script::Attributes::Category, "Ogham")
                ->Constructor<>()
                ->Property("tag",             BehaviorValueProperty(&DialogueEntry::tag))
                ->Property("parentTag",       BehaviorValueProperty(&DialogueEntry::parentTag))
                ->Property("textKeys",        BehaviorValueProperty(&DialogueEntry::textKeys))
                ->Property("entryOperations", BehaviorValueProperty(&DialogueEntry::entryOperations))
                ->Property("options",         BehaviorValueProperty(&DialogueEntry::options));
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // OghamSaveState

    void OghamSaveState::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<OghamSaveState>()
                ->Version(1)
                ->Field("Uuid",           &OghamSaveState::uuid)
                ->Field("Name",           &OghamSaveState::name)
                ->Field("CurrentEntryId", &OghamSaveState::currentEntryId)
                ->Field("State",          &OghamSaveState::state)
                ->Field("History",        &OghamSaveState::history);

            if (auto* edit = serialize->GetEditContext())
            {
                edit->Class<OghamSaveState>("Ogham Save State",
                        "A portable snapshot of the narrative processor state.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &OghamSaveState::uuid,
                        "UUID", "Unique ID for this save slot")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &OghamSaveState::name,
                        "Name", "Human-readable label")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &OghamSaveState::currentEntryId,
                        "Current Entry ID", "Tag hash of the entry active at save time")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &OghamSaveState::state,
                        "State", "Snapshot of the narrative tag collection")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &OghamSaveState::history,
                        "History", "Full narrative history at save time");
            }
        }

        if (auto* behavior = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behavior->Class<OghamSaveState>("Ogham Save State")
                ->Attribute(AZ::Script::Attributes::Category, "Ogham")
                ->Constructor<>()
                ->Property("uuid",           BehaviorValueProperty(&OghamSaveState::uuid))
                ->Property("name",           BehaviorValueProperty(&OghamSaveState::name))
                ->Property("currentEntryId", BehaviorValueProperty(&OghamSaveState::currentEntryId))
                ->Property("state",          BehaviorValueProperty(&OghamSaveState::state))
                ->Property("history",        BehaviorValueProperty(&OghamSaveState::history));
        }
    }

} // namespace FoundationOgham
