
#pragma once

#include <Clients/FoundationOghamSystemComponent.h>
#include "OghamAssetBuilder.h"

#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Asset/GenericAssetHandler.h>
#include <AzToolsFramework/ActionManager/ActionManagerRegistrationNotificationBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <FoundationOgham/OghamAsset.h>

namespace FoundationOgham
{
    /// System component for FoundationOgham editor
    class FoundationOghamEditorSystemComponent
        : public FoundationOghamSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
        , public AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler
    {
        using BaseSystemComponent = FoundationOghamSystemComponent;
    public:
        AZ_COMPONENT_DECL(FoundationOghamEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        FoundationOghamEditorSystemComponent();
        ~FoundationOghamEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzToolsFramework::EditorEventsBus overrides ...
        void NotifyRegisterViews() override;

        // AzToolsFramework::ActionManagerRegistrationNotificationBus
        void OnMenuRegistrationHook()   override;
        void OnMenuBindingHook()        override;
        void OnActionRegistrationHook() override;

        OghamAssetBuilder m_builder;
        OghamAssetHandler* m_assetHandler = nullptr;
    };
} // namespace FoundationOgham
