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

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>

#include <FoundationOgham/OghamTypes.h>

namespace FoundationOgham
{
    // -------------------------------------------------------------------------
    // OghamAsset
    // -------------------------------------------------------------------------

    ///<summary>
    /// Compiled narrative asset produced by OghamAssetBuilder from a .ogmcon source.
    ///
    /// m_entries — the full flat list of all DialogueEntry nodes.
    ///             Serialised into the .ogmbin file via ObjectStream.
    ///
    /// After the asset is deserialised, BuildIndex() must be called (the asset
    /// handler does this automatically).  Once built:
    ///   m_index    — maps tag ID → index in m_entries for O(1) lookup
    ///   m_childMap — maps parent tag ID → set of direct-child entry tag IDs
    ///                used by ReturnTo() in OghamProcessor to collect descendants
    ///
    /// Extension constants:
    ///   SourceExtension — ".ogmcon"  (JSON source file)
    ///   ProductExtension — ".ogmbin" (compiled binary)
    ///</summary>
    class OghamAsset : public AZ::Data::AssetData
    {
    public:
        AZ_CLASS_ALLOCATOR(OghamAsset, AZ::SystemAllocator, 0)
        AZ_RTTI(OghamAsset, "{5A6B7C8D-9E0F-1A2B-3C4D-5E6F7A8B9C0D}", AZ::Data::AssetData)

        static void Reflect(AZ::ReflectContext* context);

        static constexpr const char* SourceExtension  = "ogmcon";
        static constexpr const char* ProductExtension = "ogmbin";
        static constexpr const char* AssetGroup       = "Ogham";

        // ----- Serialised data -----
        AZStd::vector<DialogueEntry> m_entries;

        // ----- Runtime index (built after deserialisation) -----

        /// Builds m_index and m_childMap from m_entries.
        /// Called automatically by OghamAssetHandler after loading.
        void BuildIndex();

        /// Finds an entry by exact tag ID. Returns nullptr if not found.
        const DialogueEntry* FindEntry(AZ::u64 tagId) const;

        /// Finds an entry by GameplayTag. Returns nullptr if not found.
        const DialogueEntry* FindEntry(const Heathen::GameplayTag& tag) const;

        /// Returns the set of direct child entry IDs for a given parent tag ID.
        /// Returns an empty set if the parent has no children or is unknown.
        const AZStd::unordered_set<AZ::u64>& GetChildren(AZ::u64 parentTagId) const;

        /// Fills 'out' with all descendant entry IDs of the given entry (BFS/DFS).
        void CollectDescendants(AZ::u64 entryTagId, AZStd::unordered_set<AZ::u64>& out) const;

    private:
        AZStd::unordered_map<AZ::u64, size_t>                        m_index;    ///< tagId → index
        AZStd::unordered_map<AZ::u64, AZStd::unordered_set<AZ::u64>> m_childMap; ///< parent → direct children
        static const AZStd::unordered_set<AZ::u64>                   s_emptySet;
    };

    // -------------------------------------------------------------------------
    // OghamAssetHandler
    // -------------------------------------------------------------------------

    ///<summary>
    /// Custom asset handler for OghamAsset.
    /// Deserialises the ObjectStream .ogmbin file then calls BuildIndex().
    ///</summary>
    class OghamAssetHandler : public AZ::Data::AssetHandler
    {
    public:
        AZ_CLASS_ALLOCATOR(OghamAssetHandler, AZ::SystemAllocator, 0)
        AZ_RTTI(OghamAssetHandler, "{6B7C8D9E-0F1A-2B3C-4D5E-6F7A8B9C0D1E}", AZ::Data::AssetHandler)

        // AZ::Data::AssetHandler
        AZ::Data::AssetPtr CreateAsset(
            const AZ::Data::AssetId& id, const AZ::Data::AssetType& type) override;

        LoadResult LoadAssetData(
            const AZ::Data::Asset<AZ::Data::AssetData>& asset,
            AZStd::shared_ptr<AZ::Data::AssetDataStream> stream,
            const AZ::Data::AssetFilterCB& assetLoadFilterCB) override;

        void DestroyAsset(AZ::Data::AssetPtr ptr) override;
        void GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes) override;
    };

} // namespace FoundationOgham
