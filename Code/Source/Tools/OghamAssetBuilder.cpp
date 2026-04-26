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

#include "OghamAssetBuilder.h"

#include <FoundationOgham/OghamAsset.h>
#include <FoundationOgham/OghamTypes.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/std/string/string.h>

#include <GameplayTagCondition.h>
#include <GameplayTagOperation.h>
#include <xxHash/xxHashFunctions.h>

AZ_PUSH_DISABLE_WARNING(4251, "-Wunknown-warning-option")
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
AZ_POP_DISABLE_WARNING

namespace FoundationOgham
{
    namespace
    {
        ////////////////////////////////////////////////////////////////////////
        // Hash a dot-path tag string the same way GameplayTagRegistry does.
        Heathen::GameplayTag HashTag(const char* tagStr)
        {
            if (!tagStr || tagStr[0] == '\0')
                return Heathen::GameplayTag{};
            return Heathen::GameplayTag(
                xxHash::xxHashFunctions::Hash64(AZStd::string(tagStr), 0));
        }

        ////////////////////////////////////////////////////////////////////////
        // Enum string → value helpers

        Heathen::GameplayTagComparisonOp ParseComparison(const char* s)
        {
            using E = Heathen::GameplayTagComparisonOp;
            if (!strcmp(s, "NotExists"))    return E::NotExists;
            if (!strcmp(s, "Equal"))        return E::Equal;
            if (!strcmp(s, "NotEqual"))     return E::NotEqual;
            if (!strcmp(s, "Less"))         return E::Less;
            if (!strcmp(s, "LessEqual"))    return E::LessEqual;
            if (!strcmp(s, "Greater"))      return E::Greater;
            if (!strcmp(s, "GreaterEqual")) return E::GreaterEqual;
            return E::Exists; // default
        }

        Heathen::GameplayTagLogicOp ParseLogicOp(const char* s)
        {
            using E = Heathen::GameplayTagLogicOp;
            if (!strcmp(s, "Or"))  return E::Or;
            if (!strcmp(s, "Xor")) return E::Xor;
            return E::And; // default
        }

        Heathen::GameplayTagArithmetic ParseArithmetic(const char* s)
        {
            using E = Heathen::GameplayTagArithmetic;
            if (!strcmp(s, "Add")) return E::Add;
            if (!strcmp(s, "Sub")) return E::Sub;
            if (!strcmp(s, "Mul")) return E::Mul;
            if (!strcmp(s, "Div")) return E::Div;
            if (!strcmp(s, "Min")) return E::Min;
            if (!strcmp(s, "Max")) return E::Max;
            return E::Set; // default
        }

        ////////////////////////////////////////////////////////////////////////
        // Parse a conditions array

        AZStd::vector<Heathen::GameplayTagCondition> ParseConditions(
            const rapidjson::Value& arr)
        {
            AZStd::vector<Heathen::GameplayTagCondition> result;
            if (!arr.IsArray()) return result;

            for (const auto& obj : arr.GetArray())
            {
                if (!obj.IsObject()) continue;

                Heathen::GameplayTagCondition cond;

                if (obj.HasMember("tag") && obj["tag"].IsString())
                    cond.tag = HashTag(obj["tag"].GetString());

                if (obj.HasMember("comparison") && obj["comparison"].IsString())
                    cond.comparison = ParseComparison(obj["comparison"].GetString());

                if (obj.HasMember("compareValue") && obj["compareValue"].IsUint64())
                    cond.compareValue = obj["compareValue"].GetUint64();

                if (obj.HasMember("exactMatch") && obj["exactMatch"].IsBool())
                    cond.exactMatch = obj["exactMatch"].GetBool();

                if (obj.HasMember("logicOp") && obj["logicOp"].IsString())
                    cond.logicOp = ParseLogicOp(obj["logicOp"].GetString());

                result.push_back(AZStd::move(cond));
            }
            return result;
        }

        ////////////////////////////////////////////////////////////////////////
        // Parse an operations array

        AZStd::vector<Heathen::GameplayTagOperation> ParseOperations(
            const rapidjson::Value& arr)
        {
            AZStd::vector<Heathen::GameplayTagOperation> result;
            if (!arr.IsArray()) return result;

            for (const auto& obj : arr.GetArray())
            {
                if (!obj.IsObject()) continue;

                Heathen::GameplayTagOperation op;

                if (obj.HasMember("tag") && obj["tag"].IsString())
                    op.tag = HashTag(obj["tag"].GetString());

                if (obj.HasMember("arithmetic") && obj["arithmetic"].IsString())
                    op.arithmetic = ParseArithmetic(obj["arithmetic"].GetString());

                if (obj.HasMember("value") && obj["value"].IsUint64())
                    op.value = obj["value"].GetUint64();

                if (obj.HasMember("conditions") && obj["conditions"].IsArray())
                    op.conditions = ParseConditions(obj["conditions"]);

                result.push_back(AZStd::move(op));
            }
            return result;
        }

    } // anonymous namespace

    ////////////////////////////////////////////////////////////////////////////
    // RegisterBuilder

    void OghamAssetBuilder::RegisterBuilder()
    {
        AssetBuilderSDK::AssetBuilderDesc desc;
        desc.m_name       = BuilderName;
        desc.m_busId      = azrtti_typeid<OghamAssetBuilder>();
        desc.m_version    = 1;
        desc.m_builderType = AssetBuilderSDK::AssetBuilderDesc::AssetBuilderType::External;

        desc.m_patterns.emplace_back(
            AssetBuilderSDK::AssetBuilderPattern(FilePattern,
                AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));

        desc.m_createJobFunction = AZStd::bind(
            &OghamAssetBuilder::CreateJobs, this,
            AZStd::placeholders::_1, AZStd::placeholders::_2);

        desc.m_processJobFunction = AZStd::bind(
            &OghamAssetBuilder::ProcessJob, this,
            AZStd::placeholders::_1, AZStd::placeholders::_2);

        BusConnect(desc.m_busId);
        AssetBuilderSDK::AssetBuilderBus::Broadcast(
            &AssetBuilderSDK::AssetBuilderBus::Events::RegisterBuilderInformation, desc);
    }

    ////////////////////////////////////////////////////////////////////////////
    // ShutDown

    void OghamAssetBuilder::ShutDown()
    {
        m_isShuttingDown = true;
        BusDisconnect();
    }

    ////////////////////////////////////////////////////////////////////////////
    // CreateJobs

    void OghamAssetBuilder::CreateJobs(
        const AssetBuilderSDK::CreateJobsRequest&  request,
        AssetBuilderSDK::CreateJobsResponse&       response)
    {
        if (m_isShuttingDown)
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        for (const AssetBuilderSDK::PlatformInfo& platform : request.m_enabledPlatforms)
        {
            AssetBuilderSDK::JobDescriptor job;
            job.m_jobKey          = "Ogham Conversation";
            job.m_critical        = false;
            job.SetPlatformIdentifier(platform.m_identifier.c_str());
            response.m_createJobOutputs.push_back(job);
        }

        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    ////////////////////////////////////////////////////////////////////////////
    // ProcessJob

    void OghamAssetBuilder::ProcessJob(
        const AssetBuilderSDK::ProcessJobRequest&  request,
        AssetBuilderSDK::ProcessJobResponse&       response)
    {
        if (m_isShuttingDown)
        {
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        // ---- Read the .ogmcon source file ----
        AZ::IO::FileIOStream stream(request.m_fullPath.c_str(), AZ::IO::OpenMode::ModeRead);
        if (!stream.IsOpen())
        {
            AZ_Error("OghamAssetBuilder", false,
                "Failed to open source file: %s", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        const AZ::IO::SizeType fileSize = stream.GetLength();
        AZStd::string jsonText(fileSize, '\0');
        stream.Read(fileSize, jsonText.data());
        stream.Close();

        // ---- Parse JSON ----
        rapidjson::Document doc;
        doc.Parse(jsonText.c_str());
        if (doc.HasParseError())
        {
            AZ_Error("OghamAssetBuilder", false,
                "JSON parse error in '%s': %s (offset %zu)",
                request.m_fullPath.c_str(),
                rapidjson::GetParseError_En(doc.GetParseError()),
                doc.GetErrorOffset());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        if (!doc.HasMember("entries") || !doc["entries"].IsArray())
        {
            AZ_Error("OghamAssetBuilder", false,
                "'%s': missing or invalid 'entries' array.",
                request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        // ---- Build OghamAsset ----
        OghamAsset asset;

        for (const auto& entryVal : doc["entries"].GetArray())
        {
            if (!entryVal.IsObject()) continue;

            DialogueEntry entry;

            if (entryVal.HasMember("tag") && entryVal["tag"].IsString())
                entry.tag = HashTag(entryVal["tag"].GetString());

            if (entryVal.HasMember("parentTag") && entryVal["parentTag"].IsString())
                entry.parentTag = HashTag(entryVal["parentTag"].GetString());

            // dataKeys is the current name; textKeys is the legacy fallback
            const char* keysField = entryVal.HasMember("dataKeys") ? "dataKeys" : "textKeys";
            if (entryVal.HasMember(keysField) && entryVal[keysField].IsArray())
            {
                for (const auto& tk : entryVal[keysField].GetArray())
                {
                    if (tk.IsString())
                        entry.textKeys.push_back(tk.GetString());
                }
            }

            if (entryVal.HasMember("entryOperations"))
                entry.entryOperations = ParseOperations(entryVal["entryOperations"]);

            if (entryVal.HasMember("options") && entryVal["options"].IsArray())
            {
                for (const auto& optVal : entryVal["options"].GetArray())
                {
                    if (!optVal.IsObject()) continue;

                    DialogueOption opt;

                    if (optVal.HasMember("tag") && optVal["tag"].IsString())
                        opt.tag = HashTag(optVal["tag"].GetString());

                    if (optVal.HasMember("textKey") && optVal["textKey"].IsString())
                        opt.textKey = optVal["textKey"].GetString();

                    // targetTag is the current name; targetEntry is the legacy fallback
                    if (optVal.HasMember("targetTag") && optVal["targetTag"].IsString())
                        opt.targetEntry = HashTag(optVal["targetTag"].GetString());
                    else if (optVal.HasMember("targetEntry") && optVal["targetEntry"].IsString())
                        opt.targetEntry = HashTag(optVal["targetEntry"].GetString());

                    if (optVal.HasMember("conditions"))
                        opt.conditions = ParseConditions(optVal["conditions"]);

                    if (optVal.HasMember("operations"))
                        opt.operations = ParseOperations(optVal["operations"]);

                    entry.options.push_back(AZStd::move(opt));
                }
            }

            asset.m_entries.push_back(AZStd::move(entry));
        }

        // ---- Serialize to .ogmbin ----
        AZ::IO::Path outputPath(request.m_tempDirPath.c_str());
        AZ::IO::Path inputPath(request.m_fullPath.c_str());
        outputPath /= inputPath.Stem();
        outputPath.ReplaceExtension(OghamAsset::ProductExtension);

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(
            serializeContext,
            &AZ::ComponentApplicationRequests::GetSerializeContext);

        if (!serializeContext)
        {
            AZ_Error("OghamAssetBuilder", false, "Could not obtain SerializeContext.");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        if (!AZ::Utils::SaveObjectToFile(
                outputPath.String(), AZ::ObjectStream::ST_BINARY,
                &asset, azrtti_typeid<OghamAsset>(), serializeContext))
        {
            AZ_Error("OghamAssetBuilder", false,
                "Failed to serialise OghamAsset to '%s'", outputPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return;
        }

        // ---- Register product ----
        AssetBuilderSDK::JobProduct product(
            outputPath.String(),
            azrtti_typeid<OghamAsset>(),
            /*subId=*/0);
        product.m_dependenciesHandled = true;

        response.m_outputProducts.push_back(AZStd::move(product));
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }

} // namespace FoundationOgham
