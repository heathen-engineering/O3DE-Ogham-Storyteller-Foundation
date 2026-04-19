
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetManagerBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInterface.h>
#include <AzToolsFramework/ActionManager/Menu/MenuManagerInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorContextIdentifiers.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorMenuIdentifiers.h>

#include "OghamStoryteller.h"
#include "FoundationOghamEditorSystemComponent.h"

#include <FoundationOgham/FoundationOghamTypeIds.h>

namespace FoundationOgham
{
    AZ_COMPONENT_IMPL(FoundationOghamEditorSystemComponent, "FoundationOghamEditorSystemComponent",
        FoundationOghamEditorSystemComponentTypeId, BaseSystemComponent);

    void FoundationOghamEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<FoundationOghamEditorSystemComponent, FoundationOghamSystemComponent>()
                ->Version(0);
        }
    }

    FoundationOghamEditorSystemComponent::FoundationOghamEditorSystemComponent() = default;

    FoundationOghamEditorSystemComponent::~FoundationOghamEditorSystemComponent() = default;

    void FoundationOghamEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("FoundationOghamEditorService"));
    }

    void FoundationOghamEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("FoundationOghamEditorService"));
    }

    void FoundationOghamEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void FoundationOghamEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void FoundationOghamEditorSystemComponent::Activate()
    {
        FoundationOghamSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusConnect();

        // Register the asset handler so the AssetManager can load .ogmbin files.
        m_assetHandler = aznew OghamAssetHandler();
        AZ::Data::AssetManager::Instance().RegisterHandler(
            m_assetHandler, azrtti_typeid<OghamAsset>());
        AZ::Data::AssetCatalogRequestBus::Broadcast(
            &AZ::Data::AssetCatalogRequests::EnableCatalogForAsset,
            azrtti_typeid<OghamAsset>());
        AZ::Data::AssetCatalogRequestBus::Broadcast(
            &AZ::Data::AssetCatalogRequests::AddExtension,
            OghamAsset::ProductExtension);

        // Register the builder so the Asset Processor watches .ogmcon files.
        m_builder.RegisterBuilder();
    }

    void FoundationOghamEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusDisconnect();
        m_builder.ShutDown();

        if (m_assetHandler)
        {
            AZ::Data::AssetManager::Instance().UnregisterHandler(m_assetHandler);
            delete m_assetHandler;
            m_assetHandler = nullptr;
        }

        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        FoundationOghamSystemComponent::Deactivate();
    }

    void FoundationOghamEditorSystemComponent::NotifyRegisterViews()
    {
        AzToolsFramework::ViewPaneOptions options;
        options.paneRect              = QRect(100, 100, 500, 400);
        options.showOnToolsToolbar    = true;
        options.toolbarIcon           = ":/FoundationOgham/toolbar_icon.svg";
        options.showInMenu            = false; // menu handled by Action Manager hooks

        AzToolsFramework::RegisterViewPane<OghamStoryteller>(
            "Ogham Storyteller", "Heathen Tools", options);
    }

    // ── Action Manager hooks ────────────────────────────────────────────────────

    void FoundationOghamEditorSystemComponent::OnMenuRegistrationHook()
    {
        auto* menuManager = AZ::Interface<AzToolsFramework::MenuManagerInterface>::Get();
        if (!menuManager)
            return;

        if (!menuManager->IsMenuRegistered("heathen.menu.tools"))
        {
            AzToolsFramework::MenuProperties props;
            props.m_name = "Heathen Tools";
            menuManager->RegisterMenu("heathen.menu.tools", props);
        }
    }

    void FoundationOghamEditorSystemComponent::OnMenuBindingHook()
    {
        auto* menuManager = AZ::Interface<AzToolsFramework::MenuManagerInterface>::Get();
        if (!menuManager)
            return;

        static bool s_submenuAdded = false;
        if (!s_submenuAdded)
        {
            menuManager->AddSubMenuToMenu(
                AZStd::string(EditorIdentifiers::ToolsMenuIdentifier),
                "heathen.menu.tools", 9000);
            s_submenuAdded = true;
        }

        menuManager->AddActionToMenu("heathen.menu.tools",
            "heathen.action.oghamstoryteller", 200);
    }

    void FoundationOghamEditorSystemComponent::OnActionRegistrationHook()
    {
        auto* actionManager = AZ::Interface<AzToolsFramework::ActionManagerInterface>::Get();
        if (!actionManager)
            return;

        AzToolsFramework::ActionProperties props;
        props.m_name        = "Ogham Storyteller";
        props.m_description = "Open the Ogham Storyteller conversation editor";
        props.m_category    = "Heathen Tools";

        actionManager->RegisterAction(
            AZStd::string(EditorIdentifiers::MainWindowActionContextIdentifier),
            "heathen.action.oghamstoryteller",
            props,
            []()
            {
                AzToolsFramework::OpenViewPane("Ogham Storyteller");
            });
    }

} // namespace FoundationOgham
