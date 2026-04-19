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

#include <FoundationOgham/OghamAsset.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Serialization/Utils.h>

namespace FoundationOgham
{
    ////////////////////////////////////////////////////////////////////////////
    // OghamAsset static members

    const AZStd::unordered_set<AZ::u64> OghamAsset::s_emptySet;

    ////////////////////////////////////////////////////////////////////////////
    // OghamAsset::Reflect

    void OghamAsset::Reflect(AZ::ReflectContext* context)
    {
        // Ensure dependent types are reflected first.
        HistoryEntry::Reflect(context);
        DialogueOption::Reflect(context);
        DialogueEntry::Reflect(context);
        OghamSaveState::Reflect(context);

        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<OghamAsset, AZ::Data::AssetData>()
                ->Version(1)
                ->Field("Entries", &OghamAsset::m_entries);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // OghamAsset runtime helpers

    void OghamAsset::BuildIndex()
    {
        m_index.clear();
        m_childMap.clear();

        for (size_t i = 0; i < m_entries.size(); ++i)
        {
            const AZ::u64 id = m_entries[i].tag.GetId();
            if (id != 0)
            {
                m_index[id] = i;
            }

            const AZ::u64 parentId = m_entries[i].parentTag.GetId();
            if (parentId != 0)
            {
                m_childMap[parentId].insert(id);
            }
        }
    }

    const DialogueEntry* OghamAsset::FindEntry(AZ::u64 tagId) const
    {
        const auto it = m_index.find(tagId);
        if (it == m_index.end())
        {
            return nullptr;
        }
        return &m_entries[it->second];
    }

    const DialogueEntry* OghamAsset::FindEntry(const Heathen::GameplayTag& tag) const
    {
        return FindEntry(tag.GetId());
    }

    const AZStd::unordered_set<AZ::u64>& OghamAsset::GetChildren(AZ::u64 parentTagId) const
    {
        const auto it = m_childMap.find(parentTagId);
        return (it != m_childMap.end()) ? it->second : s_emptySet;
    }

    void OghamAsset::CollectDescendants(
        AZ::u64 entryTagId, AZStd::unordered_set<AZ::u64>& out) const
    {
        // BFS
        AZStd::vector<AZ::u64> queue;
        queue.push_back(entryTagId);

        while (!queue.empty())
        {
            const AZ::u64 current = queue.back();
            queue.pop_back();

            const AZStd::unordered_set<AZ::u64>& children = GetChildren(current);
            for (const AZ::u64 childId : children)
            {
                if (out.insert(childId).second)  // inserted = not already visited
                {
                    queue.push_back(childId);
                }
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // OghamAssetHandler

    AZ::Data::AssetPtr OghamAssetHandler::CreateAsset(
        const AZ::Data::AssetId& id, const AZ::Data::AssetType& /*type*/)
    {
        return aznew OghamAsset();
    }

    AZ::Data::AssetHandler::LoadResult OghamAssetHandler::LoadAssetData(
        const AZ::Data::Asset<AZ::Data::AssetData>& asset,
        AZStd::shared_ptr<AZ::Data::AssetDataStream> stream,
        const AZ::Data::AssetFilterCB& /*assetLoadFilterCB*/)
    {
        OghamAsset* oghamAsset = asset.GetAs<OghamAsset>();
        if (!oghamAsset)
        {
            return LoadResult::Error;
        }

        // Deserialise via ObjectStream (the builder serialises the same way).
        if (!AZ::Utils::LoadObjectFromStreamInPlace(*stream, *oghamAsset))
        {
            return LoadResult::Error;
        }

        // Build runtime index after deserialisation.
        oghamAsset->BuildIndex();

        return LoadResult::LoadComplete;
    }

    void OghamAssetHandler::DestroyAsset(AZ::Data::AssetPtr ptr)
    {
        delete ptr;
    }

    void OghamAssetHandler::GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(azrtti_typeid<OghamAsset>());
    }

} // namespace FoundationOgham
