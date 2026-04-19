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

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/vector.h>

#include <FoundationOgham/FoundationOghamBus.h>
#include <FoundationOgham/OghamAsset.h>
#include <FoundationOgham/OghamTypes.h>


namespace FoundationOgham
{
    ///<summary>
    /// OghamProcessor — the narrative state-machine singleton.
    ///
    /// Asset registration (RegisterAsset / UnregisterAsset) and conversation
    /// start are decoupled.  Multiple OghamAssets can be registered simultaneously;
    /// a unified entry index built from all registered assets allows options and
    /// ReturnTo to navigate across file boundaries transparently.
    ///
    /// Typical usage:
    ///   1. RegisterAsset(asset1); RegisterAsset(asset2); ...
    ///   2. StartConversation(entryTag);
    ///   3. SelectOption / ReturnTo as needed
    ///   4. CloseConversation
    ///   5. UnregisterAllAssets() when the level unloads
    ///</summary>
    class FoundationOghamSystemComponent
        : public AZ::Component
        , protected FoundationOghamRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(FoundationOghamSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        FoundationOghamSystemComponent();
        ~FoundationOghamSystemComponent() override;

    protected:
        // -----------------------------------------------------------------------
        // AZ::Component
        void Init()       override;
        void Activate()   override;
        void Deactivate() override;

        // -----------------------------------------------------------------------
        // FoundationOghamRequestBus — asset registration
        void RegisterAsset(const AZ::Data::Asset<OghamAsset>& asset) override;
        void UnregisterAsset(const AZ::Data::Asset<OghamAsset>& asset) override;
        void UnregisterAllAssets() override;

        // -----------------------------------------------------------------------
        // FoundationOghamRequestBus — conversation flow
        bool StartConversation(const Heathen::GameplayTag& entryTag) override;
        bool SelectOption(const Heathen::GameplayTag& optionTag) override;
        void CloseConversation(bool interrupted) override;
        bool ReturnTo(const Heathen::GameplayTag& entryTag) override;
        bool SelectOptionByTag(const Heathen::GameplayTag& optionTag) override;

        // -----------------------------------------------------------------------
        // FoundationOghamRequestBus — queries
        bool IsConversationActive() const override;
        const DialogueEntry* GetCurrentEntry() const override;
        AZStd::vector<DialogueOption> GetAvailableOptions() const override;
        const AZStd::vector<HistoryEntry>& GetHistory() const override;
        const Heathen::GameplayTagCollection& GetNarrativeState() const override;

        // -----------------------------------------------------------------------
        // FoundationOghamRequestBus — save / load
        OghamSaveState CreateSaveState(const AZStd::string& name) const override;
        void LoadSaveState(const OghamSaveState& saveState) override;

        // -----------------------------------------------------------------------
        // FoundationOghamRequestBus — state management
        void ClearState() override;
        void ApplyOperation(const Heathen::GameplayTagOperation& op) override;

    private:
        // -----------------------------------------------------------------------
        // Internal entry location — indexes into m_loadedAssets
        struct EntryLocation
        {
            size_t assetIdx = 0;   ///< index into m_loadedAssets
            size_t entryIdx = 0;   ///< index into asset->m_entries
        };

        /// Rebuilds m_entryIndex and m_childIndex from m_loadedAssets.
        /// Called whenever an asset is registered or unregistered.
        void BuildUnifiedIndex();

        /// Finds an entry by tag ID across all registered assets.
        /// Returns nullptr if not found or asset is not ready.
        const DialogueEntry* FindEntry(AZ::u64 tagId) const;

        /// BFS over m_childIndex to collect all descendants of 'entryTagId'
        /// across all registered assets.
        void CollectDescendantsGlobal(
            AZ::u64 entryTagId,
            AZStd::unordered_set<AZ::u64>& out) const;

        /// Enters an entry: applies entryOperations and fires OnDialogueEntered.
        void EnterEntry(const DialogueEntry& entry);

        /// Builds the available-options list from an entry's current conditions.
        AZStd::vector<DialogueOption> BuildAvailableOptions(const DialogueEntry& entry) const;

        // -----------------------------------------------------------------------
        // Protocol handler for Ogham:// inline links

        // -----------------------------------------------------------------------
        // Registered assets
        AZStd::vector<AZ::Data::Asset<OghamAsset>> m_loadedAssets;

        // Unified lookup — rebuilt by BuildUnifiedIndex()
        AZStd::unordered_map<AZ::u64, EntryLocation>              m_entryIndex;
        AZStd::unordered_map<AZ::u64, AZStd::unordered_set<AZ::u64>> m_childIndex;

        // -----------------------------------------------------------------------
        // Active conversation state
        AZ::u64                        m_currentEntryId = 0;
        bool                           m_isActive       = false;
        Heathen::GameplayTagCollection m_state;
        AZStd::vector<HistoryEntry>    m_history;
    };

} // namespace FoundationOgham
