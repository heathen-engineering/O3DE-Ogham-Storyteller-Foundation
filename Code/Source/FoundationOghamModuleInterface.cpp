
#include "FoundationOghamModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <FoundationOgham/FoundationOghamTypeIds.h>

#include <Clients/FoundationOghamSystemComponent.h>

namespace FoundationOgham
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(FoundationOghamModuleInterface,
        "FoundationOghamModuleInterface", FoundationOghamModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(FoundationOghamModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(FoundationOghamModuleInterface, AZ::SystemAllocator);

    FoundationOghamModuleInterface::FoundationOghamModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            FoundationOghamSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList FoundationOghamModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<FoundationOghamSystemComponent>(),
        };
    }
} // namespace FoundationOgham
