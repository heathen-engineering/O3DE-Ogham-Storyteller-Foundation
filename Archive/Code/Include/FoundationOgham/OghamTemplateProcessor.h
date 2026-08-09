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

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>

#include <AzFramework/Spawnable/Spawnable.h>
#include <AzFramework/Spawnable/SpawnableEntitiesInterface.h>

#include <FoundationOgham/FoundationOghamBus.h>
#include <FoundationOgham/OghamTypes.h>

namespace FoundationOgham
{
    ///<summary>
    /// Instantiation mode: controls how spawned templates are managed across node transitions.
    ///
    /// Diff    — keeps templates alive if the same Spawnable UUID appears in the new node;
    ///           spawns new ones, destroys ones no longer present.
    /// Replace — destroys all active templates on every node entry, then spawns fresh.
    /// Append  — always spawns new templates; never proactively destroys them.
    ///           Templates are expected to self-destruct when done.
    ///</summary>
    enum class OghamInstantiationMode : AZ::u8
    {
        Diff    = 0,
        Replace = 1,
        Append  = 2,
    };

    ///<summary>
    /// Close mode: controls what happens to active templates when the conversation ends.
    ///
    /// Clear — destroys all tracked templates immediately.
    /// None  — leaves templates alive; they self-manage their own destruction.
    ///</summary>
    enum class OghamCloseMode : AZ::u8
    {
        Clear = 0,
        None  = 1,
    };

    ///<summary>
    /// Load mode: controls when Spawnable assets are loaded into memory.
    ///
    /// PreWarm  — on each OnDialogueEntered, pre-loads Spawnable assets referenced by
    ///            option target entries (one node of lookahead) so they are resident
    ///            before they are needed.
    /// OnDemand — loads assets only when the node that references them is entered.
    ///            May cause a 1-2 frame spawn delay on first encounter.
    ///</summary>
    enum class OghamLoadMode : AZ::u8
    {
        PreWarm  = 0,
        OnDemand = 1,
    };

    // =========================================================================
    // OghamTemplateProcessor
    //
    // Component that listens to OghamNotificationBus and spawns/despawns
    // AzFramework::Spawnable entity templates based on the Spawnable UUIDs
    // encoded in the current DialogueEntry's textKeys via the Lexicon.
    //
    // Lives on a world entity whose transform acts as the anchor position for
    // all spawned templates.  UI canvas entities ignore the world transform.
    //
    // Dependencies: FoundationOghamRequestBus, FoundationLocalisationRequestBus,
    //               AzFramework::SpawnableEntitiesInterface.
    // =========================================================================
    class OghamTemplateProcessor
        : public AZ::Component
        , public OghamNotificationBus::Handler
        , public AZ::Data::AssetBus::MultiHandler
    {
    public:
        AZ_COMPONENT(OghamTemplateProcessor,
            "{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}",
            AZ::Component);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

    protected:
        // AZ::Component
        void Activate()   override;
        void Deactivate() override;

        // OghamNotificationBus::Handler
        void OnDialogueEntered(
            const DialogueEntry& entry,
            const AZStd::vector<DialogueOption>& availableOptions) override;
        void OnOptionSelected([[maybe_unused]] const DialogueOption&) override {}
        void OnDialogueClosed(bool interrupted) override;

        // AZ::Data::AssetBus::MultiHandler — used for pre-warm async loads
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetError(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

    private:
        // Spawns a Spawnable asset (must be ready) and stores the ticket.
        void SpawnAsset(const AZ::Data::Asset<AzFramework::Spawnable>& asset,
                        const AZ::Uuid& uuid);

        // Spawns immediately if asset is resident; otherwise queues async load + marks for spawn.
        void SpawnOrQueue(const AZ::Uuid& uuid);

        // Despawns and removes the ticket for a given UUID.
        void DespawnUuid(const AZ::Uuid& uuid);

        // Despawns all tracked tickets and cancels pending spawns.
        void DespawnAll();

        // Collects Spawnable UUIDs from a DialogueEntry's textKeys.
        AZStd::unordered_set<AZ::Uuid> CollectSpawnableUuids(const DialogueEntry& entry) const;

        // Queues an async cache-warm load for a UUID (no spawn on ready).
        void QueuePreWarmLoad(const AZ::Uuid& uuid);

        // ── Serialized fields ──────────────────────────────────────────────────
        OghamInstantiationMode m_mode      = OghamInstantiationMode::Diff;
        OghamCloseMode         m_closeMode = OghamCloseMode::Clear;
        OghamLoadMode          m_loadMode  = OghamLoadMode::PreWarm;

        // ── Runtime state ──────────────────────────────────────────────────────
        AZStd::unordered_map<AZ::Uuid, AzFramework::EntitySpawnTicket> m_activeTickets;
        AZStd::unordered_set<AZ::Uuid>                                  m_pendingLoads;  ///< all async loads in flight
        AZStd::unordered_set<AZ::Uuid>                                  m_pendingSpawns; ///< subset of pendingLoads that should spawn on OnAssetReady
    };

} // namespace FoundationOgham

// Type info required for SerializeContext::Enum<T>() and EditContext::Enum<T>()
namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(FoundationOgham::OghamInstantiationMode, "{2A1B3C4D-E5F6-7890-ABCD-111213141516}");
    AZ_TYPE_INFO_SPECIALIZE(FoundationOgham::OghamCloseMode,         "{3B2C4D5E-F6A7-8901-BCDE-212223242526}");
    AZ_TYPE_INFO_SPECIALIZE(FoundationOgham::OghamLoadMode,          "{4C3D5E6F-A7B8-9012-CDEF-313233343536}");
}
