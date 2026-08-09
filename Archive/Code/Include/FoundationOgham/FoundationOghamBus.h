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

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

#include <FoundationOgham/OghamAsset.h>
#include <FoundationOgham/OghamTypes.h>

namespace FoundationOgham
{
    // =========================================================================
    // OghamNotifications  — broadcast when conversation state changes
    // =========================================================================

    class OghamNotifications : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        ///<summary>
        /// Fired when the processor enters a new DialogueEntry.
        /// 'entry'            — the entry that was just entered.
        /// 'availableOptions' — options whose conditions are currently satisfied.
        ///</summary>
        virtual void OnDialogueEntered(
            const DialogueEntry& entry,
            const AZStd::vector<DialogueOption>& availableOptions) = 0;

        ///<summary>
        /// Fired when the player selects an option, before the conversation
        /// navigates to the next entry (or closes).
        ///</summary>
        virtual void OnOptionSelected(const DialogueOption& option) = 0;

        ///<summary>
        /// Fired when the active conversation ends.
        /// 'interrupted' — true if closed by CloseConversation(true) or by a
        ///                 new StartConversation call while one was already active.
        ///</summary>
        virtual void OnDialogueClosed(bool interrupted) = 0;
    };

    using OghamNotificationBus = AZ::EBus<OghamNotifications>;

    // =========================================================================
    // FoundationOghamRequests  — the processor request interface
    // =========================================================================

    class FoundationOghamRequests
    {
    public:
        AZ_RTTI(FoundationOghamRequests, "{8B90C9AD-C36E-4FBF-A409-891CDA8DD811}");
        virtual ~FoundationOghamRequests() = default;

        // -----------------------------------------------------------------
        // Asset registration
        //
        // Register every OghamAsset that is part of the current narrative
        // context before calling StartConversation.  Assets can be registered
        // and unregistered at any time (e.g. when streaming in/out levels).
        // All conversation lookups — including cross-file option targets and
        // ReturnTo descendant collection — search across all registered assets.
        //
        // Duplicate AssetIds are silently ignored.
        // Duplicate entry tags across different assets log a warning and the
        // first-registered asset wins.
        // -----------------------------------------------------------------

        ///<summary>
        /// Adds an asset to the narrative context and rebuilds the unified entry
        /// index.  'asset' must have IsReady() == true.
        ///</summary>
        virtual void RegisterAsset(const AZ::Data::Asset<OghamAsset>& asset) = 0;

        ///<summary>
        /// Removes an asset from the narrative context and rebuilds the index.
        ///</summary>
        virtual void UnregisterAsset(const AZ::Data::Asset<OghamAsset>& asset) = 0;

        ///<summary>
        /// Clears all registered assets and the unified entry index.
        /// Does not close an active conversation.
        ///</summary>
        virtual void UnregisterAllAssets() = 0;

        // -----------------------------------------------------------------
        // Conversation flow
        // -----------------------------------------------------------------

        ///<summary>
        /// Begins (or restarts) a conversation.
        ///
        /// 'entryTag' — the first entry to display.  Searched across all
        ///              currently registered assets.
        ///
        /// At least one asset must be registered before calling this.
        /// If a conversation is already active it is closed first with
        /// interrupted = true.
        ///
        /// On success fires OnDialogueEntered for the starting entry.
        ///</summary>
        virtual bool StartConversation(const Heathen::GameplayTag& entryTag) = 0;

        ///<summary>
        /// Selects an option in the current entry.
        ///
        /// Applies the option's operations, records history, then navigates to
        /// the target entry.  The target may live in any registered asset.
        /// If targetEntry is invalid (0), the conversation closes normally.
        ///
        /// Returns false if: no active conversation, option not found, or
        /// option conditions are not met.
        ///</summary>
        virtual bool SelectOption(const Heathen::GameplayTag& optionTag) = 0;

        ///<summary>
        /// Closes the active conversation.
        /// 'interrupted' — passed through to OnDialogueClosed.
        /// No-op if no conversation is active.
        ///</summary>
        virtual void CloseConversation(bool interrupted = false) = 0;

        ///<summary>
        /// Selects an option in the current entry by its tag.  Equivalent to
        /// SelectOption but intended for protocol-link dispatch (e.g. from a
        /// [Label](Ogham://Option.Tag) inline hyperlink in a MarkdownTextComponent).
        ///
        /// Looks up 'optionTag' in the active entry, checks its conditions, applies
        /// its operations, records history, then navigates to its targetEntry.
        /// If targetEntry is invalid (0), the conversation closes normally.
        ///
        /// Returns false if: no active conversation, option not found in the
        /// current entry, or option conditions are not satisfied.
        ///</summary>
        virtual bool SelectOptionByTag(const Heathen::GameplayTag& optionTag) = 0;

        ///<summary>
        /// Navigates back to a previous entry.
        ///
        /// All entries that are descendants of 'entryTag' (across all registered
        /// assets, using the parentTag field for hierarchy) are removed from the
        /// narrative state.  The processor then enters 'entryTag' without
        /// re-applying its entryOperations.
        ///
        /// History remains append-only and is not affected.
        ///
        /// Returns false if no active conversation or entry not found.
        ///</summary>
        virtual bool ReturnTo(const Heathen::GameplayTag& entryTag) = 0;

        // -----------------------------------------------------------------
        // Queries
        // -----------------------------------------------------------------

        /// Returns true if a conversation is currently active.
        virtual bool IsConversationActive() const = 0;

        /// Returns the current active entry, or nullptr if none.
        virtual const DialogueEntry* GetCurrentEntry() const = 0;

        /// Finds any entry by tag across all registered assets. Returns nullptr if not found.
        /// Used by OghamTemplateProcessor for lookahead pre-warming.
        virtual const DialogueEntry* FindEntry(const Heathen::GameplayTag& tag) const = 0;

        /// Returns the options in the current entry whose conditions are satisfied.
        virtual AZStd::vector<DialogueOption> GetAvailableOptions() const = 0;

        /// Returns all options in the current entry regardless of conditions.
        /// Compare with GetAvailableOptions() to determine which options are gated.
        virtual AZStd::vector<DialogueOption> GetAllOptions() const = 0;

        /// Returns true if the named option's conditions are met in the current state.
        /// Returns false if no conversation is active or the option is not found.
        virtual bool IsOptionAvailable(const Heathen::GameplayTag& optionTag) const = 0;

        /// Returns the full narrative history (append-only, never cleared by ReturnTo).
        virtual const AZStd::vector<HistoryEntry>& GetHistory() const = 0;

        /// Returns the current narrative state collection.
        virtual const Heathen::GameplayTagCollection& GetNarrativeState() const = 0;

        /// Returns a new collection containing only state entries at or below 'tag'.
        virtual Heathen::GameplayTagCollection ReadState(const Heathen::GameplayTag& tag) const = 0;

        // -----------------------------------------------------------------
        // Save / Load
        // -----------------------------------------------------------------

        ///<summary>
        /// Captures a portable snapshot of the current processor state.
        /// 'name' — human-readable label stored in the snapshot.
        ///
        /// NOTE: The snapshot does NOT store which assets were registered.
        /// After LoadSaveState, re-register the same assets, then call
        /// StartConversation(GameplayTag(saveState.currentEntryId)) to resume.
        ///</summary>
        virtual OghamSaveState CreateSaveState(const AZStd::string& name) const = 0;

        ///<summary>
        /// Restores state and history from a snapshot without firing entry events.
        ///</summary>
        virtual void LoadSaveState(const OghamSaveState& saveState) = 0;

        // -----------------------------------------------------------------
        // State management
        // -----------------------------------------------------------------

        ///<summary>
        /// Clears all narrative state tags. Does not affect history.
        /// Does not close an active conversation or unregister assets.
        ///</summary>
        virtual void ClearState() = 0;

        ///<summary>
        /// Clears narrative state tags at or below 'tag'.
        ///</summary>
        virtual void ClearState(const Heathen::GameplayTag& tag) = 0;

        ///<summary>
        /// Clears the full conversation history.
        ///</summary>
        virtual void ClearHistory() = 0;

        ///<summary>
        /// Removes the last 'steps' entries from the conversation history.
        /// Clamped to the current history length; no-op if steps <= 0.
        ///</summary>
        virtual void ClearHistory(AZ::s32 steps) = 0;

        ///<summary>
        /// Direct write access to the narrative state — applies a single operation.
        ///</summary>
        virtual void ApplyOperation(const Heathen::GameplayTagOperation& op) = 0;

        ///<summary>
        /// Applies multiple operations to the narrative state in order.
        ///</summary>
        virtual void ApplyOperations(const AZStd::vector<Heathen::GameplayTagOperation>& ops) = 0;
    };

    class FoundationOghamBusTraits : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    };

    using FoundationOghamRequestBus = AZ::EBus<FoundationOghamRequests, FoundationOghamBusTraits>;
    using FoundationOghamInterface  = AZ::Interface<FoundationOghamRequests>;

} // namespace FoundationOgham
