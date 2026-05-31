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

#include <FoundationOgham/OghamTemplateProcessor.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzFramework/Components/TransformComponent.h>

#include <FoundationLocalisation/FoundationLocalisationBus.h>
#include <FoundationLocalisation/LexiconHintType.h>

#include <xxHash/xxHashFunctions.h>

namespace FoundationOgham
{
    void OghamTemplateProcessor::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Enum<OghamInstantiationMode>()
                ->Value("Diff",    OghamInstantiationMode::Diff)
                ->Value("Replace", OghamInstantiationMode::Replace)
                ->Value("Append",  OghamInstantiationMode::Append);

            sc->Enum<OghamCloseMode>()
                ->Value("Clear", OghamCloseMode::Clear)
                ->Value("None",  OghamCloseMode::None);

            sc->Enum<OghamLoadMode>()
                ->Value("PreWarm",  OghamLoadMode::PreWarm)
                ->Value("OnDemand", OghamLoadMode::OnDemand);

            sc->Class<OghamTemplateProcessor, AZ::Component>()
                ->Version(1)
                ->Field("Mode",      &OghamTemplateProcessor::m_mode)
                ->Field("CloseMode", &OghamTemplateProcessor::m_closeMode)
                ->Field("LoadMode",  &OghamTemplateProcessor::m_loadMode);

            if (auto* ec = sc->GetEditContext())
            {
                ec->Enum<OghamInstantiationMode>("Instantiation Mode",
                    "How spawned templates are managed across node transitions")
                    ->Value("Diff",    OghamInstantiationMode::Diff)
                    ->Value("Replace", OghamInstantiationMode::Replace)
                    ->Value("Append",  OghamInstantiationMode::Append);

                ec->Enum<OghamCloseMode>("Close Mode",
                    "What happens to active templates when the conversation ends")
                    ->Value("Clear", OghamCloseMode::Clear)
                    ->Value("None",  OghamCloseMode::None);

                ec->Enum<OghamLoadMode>("Load Mode",
                    "When Spawnable assets are loaded into memory")
                    ->Value("PreWarm",  OghamLoadMode::PreWarm)
                    ->Value("OnDemand", OghamLoadMode::OnDemand);

                ec->Class<OghamTemplateProcessor>(
                    "Ogham Template Processor",
                    "Spawns and despawns entity templates in response to Ogham narrative events. "
                    "Reads Spawnable UUID keys from each DialogueEntry's textKeys via the active Lexicon.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &OghamTemplateProcessor::m_mode,
                        "Instantiation Mode",
                        "Diff: keep alive if same UUID appears in new node; "
                        "Replace: destroy all then respawn fresh; "
                        "Append: always spawn, never proactively destroy")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &OghamTemplateProcessor::m_closeMode,
                        "Close Mode",
                        "Clear: destroy all tracked templates when conversation ends; "
                        "None: leave templates alive to self-manage")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &OghamTemplateProcessor::m_loadMode,
                        "Load Mode",
                        "PreWarm: pre-load Spawnables for option targets one node ahead; "
                        "OnDemand: load on encounter (may cause a brief spawn delay)");
            }
        }
    }

    void OghamTemplateProcessor::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("OghamTemplateProcessorService"));
    }

    void OghamTemplateProcessor::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("OghamTemplateProcessorService"));
    }

    void OghamTemplateProcessor::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void OghamTemplateProcessor::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    // =========================================================================

    void OghamTemplateProcessor::Activate()
    {
        OghamNotificationBus::Handler::BusConnect();
    }

    void OghamTemplateProcessor::Deactivate()
    {
        AZ::Data::AssetBus::MultiHandler::BusDisconnect();
        OghamNotificationBus::Handler::BusDisconnect();
        m_pendingSpawns.clear();
        m_pendingLoads.clear();
        m_activeTickets.clear(); // RAII: each EntitySpawnTicket destructor despawns its entities
    }

    // =========================================================================
    // OghamNotificationBus::Handler

    void OghamTemplateProcessor::OnDialogueEntered(
        const DialogueEntry& entry,
        const AZStd::vector<DialogueOption>& availableOptions)
    {
        AZStd::unordered_set<AZ::Uuid> uuids = CollectSpawnableUuids(entry);

        switch (m_mode)
        {
        case OghamInstantiationMode::Replace:
        {
            m_pendingSpawns.clear();
            DespawnAll();
            for (const AZ::Uuid& uuid : uuids)
                SpawnOrQueue(uuid);
            break;
        }
        case OghamInstantiationMode::Diff:
        {
            // Collect tickets no longer present in the new UUID set
            AZStd::vector<AZ::Uuid> toRemove;
            for (const auto& [uuid, ticket] : m_activeTickets)
            {
                if (uuids.find(uuid) == uuids.end())
                    toRemove.push_back(uuid);
            }
            for (const AZ::Uuid& uuid : toRemove)
                DespawnUuid(uuid);

            // Cancel any in-flight loads whose UUID is no longer wanted
            AZStd::vector<AZ::Uuid> cancelSpawns;
            for (const AZ::Uuid& uuid : m_pendingSpawns)
            {
                if (uuids.find(uuid) == uuids.end())
                    cancelSpawns.push_back(uuid);
            }
            for (const AZ::Uuid& uuid : cancelSpawns)
                m_pendingSpawns.erase(uuid);

            // Spawn any UUIDs not already active (or already loading)
            for (const AZ::Uuid& uuid : uuids)
            {
                if (m_activeTickets.find(uuid) == m_activeTickets.end())
                    SpawnOrQueue(uuid);
            }
            break;
        }
        case OghamInstantiationMode::Append:
        {
            for (const AZ::Uuid& uuid : uuids)
                SpawnOrQueue(uuid);
            break;
        }
        }

        // PreWarm: pre-load Spawnables that will be needed for the option target nodes
        if (m_loadMode == OghamLoadMode::PreWarm)
        {
            for (const DialogueOption& option : availableOptions)
            {
                if (!option.targetEntry.IsValid())
                    continue;

                const DialogueEntry* targetEntry = nullptr;
                FoundationOghamRequestBus::BroadcastResult(
                    targetEntry,
                    &FoundationOghamRequests::FindEntry,
                    option.targetEntry);

                if (!targetEntry)
                    continue;

                for (const AZ::Uuid& uuid : CollectSpawnableUuids(*targetEntry))
                    QueuePreWarmLoad(uuid);
            }
        }
    }

    void OghamTemplateProcessor::OnDialogueClosed([[maybe_unused]] bool interrupted)
    {
        if (m_closeMode == OghamCloseMode::Clear)
        {
            m_pendingSpawns.clear();
            DespawnAll();
        }
    }

    // =========================================================================
    // AZ::Data::AssetBus::MultiHandler

    void OghamTemplateProcessor::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        AZ::Data::AssetBus::MultiHandler::BusDisconnect(asset.GetId());
        const AZ::Uuid& uuid = asset.GetId().m_guid;
        m_pendingLoads.erase(uuid);

        if (m_pendingSpawns.erase(uuid) > 0)
        {
            auto spawnableAsset = AZ::Data::AssetManager::Instance().FindAsset<AzFramework::Spawnable>(
                asset.GetId(), AZ::Data::AssetLoadBehavior::NoLoad);
            if (spawnableAsset.IsReady())
                SpawnAsset(spawnableAsset, uuid);
        }
    }

    void OghamTemplateProcessor::OnAssetError(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        AZ::Data::AssetBus::MultiHandler::BusDisconnect(asset.GetId());
        const AZ::Uuid& uuid = asset.GetId().m_guid;
        m_pendingLoads.erase(uuid);
        m_pendingSpawns.erase(uuid);
        AZ_Warning("OghamTemplateProcessor", false,
            "Failed to load spawnable asset %s",
            asset.GetId().ToString<AZStd::string>().c_str());
    }

    // =========================================================================
    // Private helpers

    void OghamTemplateProcessor::SpawnAsset(
        const AZ::Data::Asset<AzFramework::Spawnable>& asset,
        const AZ::Uuid& uuid)
    {
        AZ::Transform worldTm = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTm, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);

        AzFramework::EntitySpawnTicket ticket(asset);

        AzFramework::SpawnAllEntitiesOptionalArgs args;
        args.m_preInsertionCallback = [worldTm](
            AzFramework::EntitySpawnTicket::Id /*id*/,
            AzFramework::SpawnableEntityContainerView view)
        {
            for (AZ::Entity* entity : view)
            {
                auto* tf = entity->FindComponent<AzFramework::TransformComponent>();
                if (tf && !tf->GetParentId().IsValid())
                    tf->SetWorldTM(worldTm * tf->GetLocalTM());
            }
        };

        AzFramework::SpawnableEntitiesInterface::Get()->SpawnAllEntities(ticket, AZStd::move(args));
        m_activeTickets.emplace(uuid, AZStd::move(ticket));
    }

    void OghamTemplateProcessor::SpawnOrQueue(const AZ::Uuid& uuid)
    {
        if (m_pendingSpawns.count(uuid))
            return;

        const AZ::Data::AssetId assetId(uuid, 0);

        auto asset = AZ::Data::AssetManager::Instance().FindAsset<AzFramework::Spawnable>(
            assetId, AZ::Data::AssetLoadBehavior::NoLoad);
        if (asset.IsReady())
        {
            SpawnAsset(asset, uuid);
            return;
        }

        if (m_pendingLoads.count(uuid))
            return;

        asset = AZ::Data::AssetManager::Instance().GetAsset<AzFramework::Spawnable>(
            assetId, AZ::Data::AssetLoadBehavior::QueueLoad);
        AZ::Data::AssetBus::MultiHandler::BusConnect(assetId);
        m_pendingLoads.insert(uuid);
        m_pendingSpawns.insert(uuid);
    }

    void OghamTemplateProcessor::DespawnUuid(const AZ::Uuid& uuid)
    {
        m_activeTickets.erase(uuid); // RAII despawn
    }

    void OghamTemplateProcessor::DespawnAll()
    {
        m_activeTickets.clear(); // RAII despawn
    }

    AZStd::unordered_set<AZ::Uuid> OghamTemplateProcessor::CollectSpawnableUuids(
        const DialogueEntry& entry) const
    {
        AZStd::unordered_set<AZ::Uuid> result;

        for (const OghamContentKey& ck : entry.contentKeys)
        {
            if (ck.keyOrValue.empty())
                continue;

            const AZ::u64 hash = xxHash::xxHashFunctions::Hash64(ck.keyOrValue, 0);

            FoundationLocalisation::LexiconHintType hintType =
                FoundationLocalisation::LexiconHintType::None;
            FoundationLocalisation::FoundationLocalisationRequestBus::BroadcastResult(
                hintType,
                &FoundationLocalisation::FoundationLocalisationRequests::GetEntryHintType,
                hash);

            if (hintType != FoundationLocalisation::LexiconHintType::Spawnable)
                continue;

            AZ::Uuid assetUuid;
            FoundationLocalisation::FoundationLocalisationRequestBus::BroadcastResult(
                assetUuid,
                static_cast<AZ::Uuid(FoundationLocalisation::FoundationLocalisationRequests::*)(AZ::u64)>(
                    &FoundationLocalisation::FoundationLocalisationRequests::ResolveAssetId),
                hash);

            if (!assetUuid.IsNull())
                result.insert(assetUuid);
        }

        return result;
    }

    void OghamTemplateProcessor::QueuePreWarmLoad(const AZ::Uuid& uuid)
    {
        if (m_activeTickets.count(uuid) || m_pendingLoads.count(uuid))
            return;

        const AZ::Data::AssetId assetId(uuid, 0);

        auto asset = AZ::Data::AssetManager::Instance().FindAsset<AzFramework::Spawnable>(
            assetId, AZ::Data::AssetLoadBehavior::NoLoad);
        if (asset.IsReady())
            return; // already resident

        asset = AZ::Data::AssetManager::Instance().GetAsset<AzFramework::Spawnable>(
            assetId, AZ::Data::AssetLoadBehavior::QueueLoad);
        AZ::Data::AssetBus::MultiHandler::BusConnect(assetId);
        m_pendingLoads.insert(uuid);
        // not added to m_pendingSpawns — cache-warm only
    }

} // namespace FoundationOgham
