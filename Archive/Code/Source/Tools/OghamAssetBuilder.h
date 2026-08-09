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

#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>

namespace FoundationOgham
{
    ///<summary>
    /// Compiles .ogmcon (JSON) source assets into .ogmbin (ObjectStream binary) products.
    ///
    /// Source format (.ogmcon):
    /// {
    ///   "entries": [
    ///     {
    ///       "tag": "Act1.Scene1.D1",
    ///       "parentTag": "Act1.Scene1",
    ///       "textKeys": ["dialogue.act1.d1.body"],
    ///       "entryOperations": [
    ///         { "tag": "Act1.Scene1.D1.Visited", "arithmetic": "Set", "value": 1,
    ///           "conditions": [] }
    ///       ],
    ///       "options": [
    ///         {
    ///           "tag": "Act1.Scene1.D1.Opt1",
    ///           "textKey": "dialogue.act1.d1.opt1",
    ///           "targetEntry": "Act1.Scene1.D2",
    ///           "conditions": [],
    ///           "operations": []
    ///         }
    ///       ]
    ///     }
    ///   ]
    /// }
    ///
    /// All "tag" fields use dot-separated paths hashed with XXH3_64bits(seed=0).
    /// "targetEntry": "" or absent means close conversation on selection.
    ///</summary>
    class OghamAssetBuilder
        : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        AZ_RTTI(OghamAssetBuilder, "{7C8D9E0F-1A2B-3C4D-5E6F-7A8B9C0D1E2F}",
                AssetBuilderSDK::AssetBuilderCommandBus::Handler)

        static constexpr const char* BuilderName  = "Ogham Conversation Builder";
        static constexpr const char* FilePattern  = "*.ogmcon";

        void RegisterBuilder();

        // AssetBuilderSDK::AssetBuilderCommandBus
        void CreateJobs(
            const AssetBuilderSDK::CreateJobsRequest&  request,
            AssetBuilderSDK::CreateJobsResponse&       response);

        void ProcessJob(
            const AssetBuilderSDK::ProcessJobRequest&  request,
            AssetBuilderSDK::ProcessJobResponse&       response);

        void ShutDown() override;

    private:
        bool m_isShuttingDown = false;
    };

} // namespace FoundationOgham
