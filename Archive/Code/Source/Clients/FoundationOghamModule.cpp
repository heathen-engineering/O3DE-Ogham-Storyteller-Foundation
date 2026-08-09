
#include <FoundationOgham/FoundationOghamTypeIds.h>
#include <FoundationOghamModuleInterface.h>
#include "FoundationOghamSystemComponent.h"

namespace FoundationOgham
{
    class FoundationOghamModule
        : public FoundationOghamModuleInterface
    {
    public:
        AZ_RTTI(FoundationOghamModule, FoundationOghamModuleTypeId, FoundationOghamModuleInterface);
        AZ_CLASS_ALLOCATOR(FoundationOghamModule, AZ::SystemAllocator);
    };
}// namespace FoundationOgham

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), FoundationOgham::FoundationOghamModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_FoundationOgham, FoundationOgham::FoundationOghamModule)
#endif
