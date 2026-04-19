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

#include "FoundationOghamSystemComponent.h"

#include <FoundationOgham/FoundationOghamTypeIds.h>
#include <FoundationOgham/OghamAsset.h>
#include <FoundationOgham/OghamTypes.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Math/Uuid.h>

#include <GameplayTagCondition.h>  // EvaluateConditions

namespace FoundationOgham
{
    // -------------------------------------------------------------------------
    // Notification bus handler — allows Script Canvas to receive Ogham events.
    // -------------------------------------------------------------------------

    class BehaviorOghamNotificationBusHandler
        : public OghamNotificationBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(BehaviorOghamNotificationBusHandler,
            "{C3D4E5F6-A7B8-9C0D-1E2F-A3B4C5D6E7F8}",
            AZ::SystemAllocator,
            OnDialogueEntered,
            OnDialogueClosed);

        void OnDialogueEntered(
            const DialogueEntry& entry,
            const AZStd::vector<DialogueOption>& availableOptions) override
        {
            Call(FN_OnDialogueEntered, entry, availableOptions);
        }

        void OnDialogueClosed(bool interrupted) override
        {
            Call(FN_OnDialogueClosed, interrupted);
        }
    };

    // -------------------------------------------------------------------------

    AZ_COMPONENT_IMPL(FoundationOghamSystemComponent, "FoundationOghamSystemComponent",
        FoundationOghamSystemComponentTypeId);

    void FoundationOghamSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        // OghamAsset::Reflect cascades into HistoryEntry, DialogueOption,
        // DialogueEntry and OghamSaveState — do not call them again here.
        OghamAsset::Reflect(context);

        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<FoundationOghamSystemComponent, AZ::Component>()
                ->Version(0);
        }

        if (auto* behavior = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            // Request bus — allows SC graphs to drive conversations.
            behavior->EBus<FoundationOghamRequestBus>("OghamProcessor")
                ->Attribute(AZ::Script::Attributes::Category, "Ogham")
                // ---- Asset registration ----
                ->Event("Register Asset",
                    &FoundationOghamRequests::RegisterAsset,
                    {{{ "Asset", "A ready OghamAsset to add to the narrative context" }}})
                ->Event("Unregister Asset",
                    &FoundationOghamRequests::UnregisterAsset,
                    {{{ "Asset", "The OghamAsset to remove from the narrative context" }}})
                ->Event("Unregister All Assets",
                    &FoundationOghamRequests::UnregisterAllAssets)
                // ---- Conversation ----
                ->Event("Start Conversation",
                    &FoundationOghamRequests::StartConversation,
                    {{{ "Entry Tag", "Tag of the first entry to display (searched across all registered assets)" }}})
                ->Event("Select Option",
                    &FoundationOghamRequests::SelectOption,
                    {{{ "Option Tag", "Tag of the option chosen by the player" }}})
                ->Event("Close Conversation",
                    &FoundationOghamRequests::CloseConversation,
                    {{{ "Interrupted", "Pass true when closing externally" }}})
                ->Event("Return To",
                    &FoundationOghamRequests::ReturnTo,
                    {{{ "Entry Tag", "Entry to navigate back to; clears descendant state across all assets" }}})
                ->Event("Select Option By Tag",
                    &FoundationOghamRequests::SelectOptionByTag,
                    {{{ "Entry Tag", "Tag of the entry to jump to directly (e.g. from an inline hyperlink)" }}})
                // ---- Queries ----
                ->Event("Is Conversation Active",
                    &FoundationOghamRequests::IsConversationActive)
                ->Event("Get Current Entry",
                    &FoundationOghamRequests::GetCurrentEntry)
                ->Event("Get Available Options",
                    &FoundationOghamRequests::GetAvailableOptions)
                ->Event("Get History",
                    &FoundationOghamRequests::GetHistory)
                ->Event("Get Narrative State",
                    &FoundationOghamRequests::GetNarrativeState)
                // ---- Save / Load ----
                ->Event("Create Save State",
                    &FoundationOghamRequests::CreateSaveState,
                    {{{ "Name", "Human-readable label for this save slot" }}})
                ->Event("Load Save State",
                    &FoundationOghamRequests::LoadSaveState,
                    {{{ "Save State", "Snapshot previously created by Create Save State" }}})
                // ---- State management ----
                ->Event("Clear State",
                    &FoundationOghamRequests::ClearState)
                ->Event("Apply Operation",
                    &FoundationOghamRequests::ApplyOperation,
                    {{{ "Operation", "GameplayTagOperation to apply to the narrative state" }}});

            // Notification bus — allows SC graphs to receive conversation events.
            behavior->EBus<OghamNotificationBus>("OghamNotificationBus")
                ->Attribute(AZ::Script::Attributes::Category, "Ogham")
                ->Handler<BehaviorOghamNotificationBusHandler>()
                ->Event("On Dialogue Entered",
                    &OghamNotifications::OnDialogueEntered,
                    {{{ "Entry", "The dialogue entry that was just entered" },
                      { "Available Options", "Options whose conditions are satisfied" }}})
                ->Event("On Dialogue Closed",
                    &OghamNotifications::OnDialogueClosed,
                    {{{ "Interrupted", "True if closed by a new StartConversation or explicit interrupt" }}});
        }
    }

    void FoundationOghamSystemComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("FoundationOghamService"));
    }

    void FoundationOghamSystemComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("FoundationOghamService"));
    }

    void FoundationOghamSystemComponent::GetRequiredServices(
        [[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void FoundationOghamSystemComponent::GetDependentServices(
        [[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    FoundationOghamSystemComponent::FoundationOghamSystemComponent()
    {
        if (FoundationOghamInterface::Get() == nullptr)
        {
            FoundationOghamInterface::Register(this);
        }
    }

    FoundationOghamSystemComponent::~FoundationOghamSystemComponent()
    {
        if (FoundationOghamInterface::Get() == this)
        {
            FoundationOghamInterface::Unregister(this);
        }
    }

    void FoundationOghamSystemComponent::Init()
    {
    }

    void FoundationOghamSystemComponent::Activate()
    {
        FoundationOghamRequestBus::Handler::BusConnect();
    }

    void FoundationOghamSystemComponent::Deactivate()
    {
        FoundationOghamRequestBus::Handler::BusDisconnect();
        m_isActive       = false;
        m_currentEntryId = 0;
        UnregisterAllAssets();
    }

    ////////////////////////////////////////////////////////////////////////////
    // Asset registration

    void FoundationOghamSystemComponent::RegisterAsset(
        const AZ::Data::Asset<OghamAsset>& asset)
    {
        if (!asset.IsReady())
        {
            AZ_Warning("OghamProcessor", false,
                "RegisterAsset: asset is not ready (AssetId: %s)",
                asset.GetId().ToString<AZStd::string>().c_str());
            return;
        }

        // Ignore duplicates
        for (const auto& existing : m_loadedAssets)
        {
            if (existing.GetId() == asset.GetId())
                return;
        }

        m_loadedAssets.push_back(asset);
        BuildUnifiedIndex();
    }

    void FoundationOghamSystemComponent::UnregisterAsset(
        const AZ::Data::Asset<OghamAsset>& asset)
    {
        auto it = AZStd::find_if(m_loadedAssets.begin(), m_loadedAssets.end(),
            [&asset](const AZ::Data::Asset<OghamAsset>& a)
            {
                return a.GetId() == asset.GetId();
            });

        if (it != m_loadedAssets.end())
        {
            it->Release();
            m_loadedAssets.erase(it);
            BuildUnifiedIndex();
        }
    }

    void FoundationOghamSystemComponent::UnregisterAllAssets()
    {
        for (auto& asset : m_loadedAssets)
            asset.Release();
        m_loadedAssets.clear();
        m_entryIndex.clear();
        m_childIndex.clear();
    }

    ////////////////////////////////////////////////////////////////////////////
    // StartConversation

    bool FoundationOghamSystemComponent::StartConversation(
        const Heathen::GameplayTag& entryTag)
    {
        if (m_loadedAssets.empty())
        {
            AZ_Warning("OghamProcessor", false,
                "StartConversation: no assets registered. "
                "Call RegisterAsset before StartConversation.");
            return false;
        }

        const DialogueEntry* entry = FindEntry(entryTag.GetId());
        if (!entry)
        {
            AZ_Warning("OghamProcessor", false,
                "StartConversation: entry tag not found in any registered asset.");
            return false;
        }

        // Close any currently active conversation first.
        if (m_isActive)
            CloseConversation(true);

        m_isActive       = true;
        m_currentEntryId = entryTag.GetId();

        EnterEntry(*entry);
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////
    // SelectOption

    bool FoundationOghamSystemComponent::SelectOption(const Heathen::GameplayTag& optionTag)
    {
        if (!m_isActive || m_currentEntryId == 0)
            return false;

        const DialogueEntry* entry = FindEntry(m_currentEntryId);
        if (!entry)
            return false;

        // Find the option and verify its conditions.
        const DialogueOption* chosen = nullptr;
        for (const DialogueOption& opt : entry->options)
        {
            if (opt.tag.GetId() == optionTag.GetId())
            {
                if (Heathen::EvaluateConditions(opt.conditions, m_state))
                    chosen = &opt;
                break;
            }
        }

        if (!chosen)
            return false;

        // Apply option operations.
        for (const Heathen::GameplayTagOperation& op : chosen->operations)
            op.Apply(m_state);

        // Record the choice in state: state[entryId] = optionId.
        m_state.Apply(Heathen::GameplayTag(m_currentEntryId),
                      Heathen::GameplayTagArithmetic::Set,
                      chosen->tag.GetId());

        // Append to history.
        m_history.push_back({ m_currentEntryId, chosen->tag.GetId() });

        // Navigate to target — may be in a different asset.
        const AZ::u64 targetId = chosen->targetEntry.GetId();
        if (targetId == 0)
        {
            CloseConversation(false);
        }
        else
        {
            const DialogueEntry* target = FindEntry(targetId);
            if (!target)
            {
                AZ_Warning("OghamProcessor", false,
                    "SelectOption: targetEntry not found in any registered asset — "
                    "closing conversation.");
                CloseConversation(false);
                return false;
            }

            m_currentEntryId = targetId;
            EnterEntry(*target);
        }

        return true;
    }

    ////////////////////////////////////////////////////////////////////////////
    // SelectOptionByTag
    //
    // Finds 'optionTag' in the current entry and performs the same flow as
    // SelectOption — condition check, operations, history, navigation.
    bool FoundationOghamSystemComponent::SelectOptionByTag(const Heathen::GameplayTag& optionTag)
    {
        return SelectOption(optionTag);
    }

    ////////////////////////////////////////////////////////////////////////////
    // CloseConversation

    void FoundationOghamSystemComponent::CloseConversation(bool interrupted)
    {
        if (!m_isActive)
            return;
        m_isActive       = false;
        m_currentEntryId = 0;
        OghamNotificationBus::Broadcast(&OghamNotifications::OnDialogueClosed, interrupted);
    }

    ////////////////////////////////////////////////////////////////////////////
    // ReturnTo

    bool FoundationOghamSystemComponent::ReturnTo(const Heathen::GameplayTag& entryTag)
    {
        if (!m_isActive)
            return false;

        const DialogueEntry* entry = FindEntry(entryTag.GetId());
        if (!entry)
            return false;

        // Collect all descendants across all registered assets using parentTag hierarchy.
        AZStd::unordered_set<AZ::u64> descendants;
        CollectDescendantsGlobal(entryTag.GetId(), descendants);

        for (const AZ::u64 descId : descendants)
        {
            m_state.Apply(Heathen::GameplayTag(descId),
                          Heathen::GameplayTagArithmetic::Set, 0);
        }

        // Navigate to the target entry without re-applying entryOperations.
        m_currentEntryId = entryTag.GetId();
        const AZStd::vector<DialogueOption> available = BuildAvailableOptions(*entry);
        OghamNotificationBus::Broadcast(
            &OghamNotifications::OnDialogueEntered, *entry, available);

        return true;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Queries

    bool FoundationOghamSystemComponent::IsConversationActive() const
    {
        return m_isActive;
    }

    const DialogueEntry* FoundationOghamSystemComponent::GetCurrentEntry() const
    {
        if (!m_isActive || m_currentEntryId == 0)
            return nullptr;
        return FindEntry(m_currentEntryId);
    }

    AZStd::vector<DialogueOption> FoundationOghamSystemComponent::GetAvailableOptions() const
    {
        const DialogueEntry* entry = GetCurrentEntry();
        if (!entry)
            return {};
        return BuildAvailableOptions(*entry);
    }

    const AZStd::vector<HistoryEntry>& FoundationOghamSystemComponent::GetHistory() const
    {
        return m_history;
    }

    const Heathen::GameplayTagCollection& FoundationOghamSystemComponent::GetNarrativeState() const
    {
        return m_state;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Save / Load

    OghamSaveState FoundationOghamSystemComponent::CreateSaveState(
        const AZStd::string& name) const
    {
        OghamSaveState snap;
        snap.uuid           = AZ::Uuid::CreateRandom();
        snap.name           = name;
        snap.currentEntryId = m_currentEntryId;
        snap.state          = m_state;
        snap.history        = m_history;
        return snap;
    }

    void FoundationOghamSystemComponent::LoadSaveState(const OghamSaveState& saveState)
    {
        m_currentEntryId = saveState.currentEntryId;
        m_state          = saveState.state;
        m_history        = saveState.history;
        // Assets must be re-registered before calling StartConversation to resume.
    }

    ////////////////////////////////////////////////////////////////////////////
    // State management

    void FoundationOghamSystemComponent::ClearState()
    {
        m_state.Clear();
        m_history.clear();
    }

    void FoundationOghamSystemComponent::ApplyOperation(
        const Heathen::GameplayTagOperation& op)
    {
        op.Apply(m_state);
    }

    ////////////////////////////////////////////////////////////////////////////
    // Private — index management

    void FoundationOghamSystemComponent::BuildUnifiedIndex()
    {
        m_entryIndex.clear();
        m_childIndex.clear();

        for (size_t assetIdx = 0; assetIdx < m_loadedAssets.size(); ++assetIdx)
        {
            const OghamAsset* asset = m_loadedAssets[assetIdx].Get();
            if (!asset)
                continue;

            for (size_t entryIdx = 0; entryIdx < asset->m_entries.size(); ++entryIdx)
            {
                const DialogueEntry& entry = asset->m_entries[entryIdx];
                const AZ::u64 tagId = entry.tag.GetId();

                if (tagId == 0)
                    continue;

                if (m_entryIndex.find(tagId) != m_entryIndex.end())
                {
                    AZ_Warning("OghamProcessor", false,
                        "BuildUnifiedIndex: duplicate entry tag detected across assets — "
                        "first-registered asset wins.");
                    continue;
                }

                m_entryIndex[tagId] = { assetIdx, entryIdx };

                // Build cross-asset parent→children map from parentTag field.
                const AZ::u64 parentId = entry.parentTag.GetId();
                if (parentId != 0)
                    m_childIndex[parentId].insert(tagId);
            }
        }
    }

    const DialogueEntry* FoundationOghamSystemComponent::FindEntry(AZ::u64 tagId) const
    {
        auto it = m_entryIndex.find(tagId);
        if (it == m_entryIndex.end())
            return nullptr;

        const EntryLocation& loc = it->second;
        if (loc.assetIdx >= m_loadedAssets.size())
            return nullptr;

        const OghamAsset* asset = m_loadedAssets[loc.assetIdx].Get();
        if (!asset || loc.entryIdx >= asset->m_entries.size())
            return nullptr;

        return &asset->m_entries[loc.entryIdx];
    }

    void FoundationOghamSystemComponent::CollectDescendantsGlobal(
        AZ::u64 entryTagId,
        AZStd::unordered_set<AZ::u64>& out) const
    {
        // BFS over the cross-asset child index.
        AZStd::vector<AZ::u64> queue;
        queue.push_back(entryTagId);

        while (!queue.empty())
        {
            const AZ::u64 current = queue.back();
            queue.pop_back();

            auto it = m_childIndex.find(current);
            if (it == m_childIndex.end())
                continue;

            for (const AZ::u64 child : it->second)
            {
                if (out.find(child) == out.end())
                {
                    out.insert(child);
                    queue.push_back(child);
                }
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Private — conversation helpers

    void FoundationOghamSystemComponent::EnterEntry(const DialogueEntry& entry)
    {
        for (const Heathen::GameplayTagOperation& op : entry.entryOperations)
            op.Apply(m_state);

        const AZStd::vector<DialogueOption> available = BuildAvailableOptions(entry);
        OghamNotificationBus::Broadcast(
            &OghamNotifications::OnDialogueEntered, entry, available);
    }

    AZStd::vector<DialogueOption> FoundationOghamSystemComponent::BuildAvailableOptions(
        const DialogueEntry& entry) const
    {
        AZStd::vector<DialogueOption> result;
        for (const DialogueOption& opt : entry.options)
        {
            if (Heathen::EvaluateConditions(opt.conditions, m_state))
                result.push_back(opt);
        }
        return result;
    }

} // namespace FoundationOgham
