
#pragma once

namespace FoundationOgham
{
    // System Component TypeIds
    inline constexpr const char* FoundationOghamSystemComponentTypeId = "{DBAEFEB7-9031-41A4-A789-4538FAB39EAD}";
    inline constexpr const char* FoundationOghamEditorSystemComponentTypeId = "{1664016F-373C-4B55-9974-AEB552262363}";

    // Module derived classes TypeIds
    inline constexpr const char* FoundationOghamModuleInterfaceTypeId = "{5B770578-908B-4FD2-A2EF-9405C740D32F}";
    inline constexpr const char* FoundationOghamModuleTypeId = "{604491E4-1704-40D7-B61D-2A945F0083E9}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* FoundationOghamEditorModuleTypeId = FoundationOghamModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* FoundationOghamRequestsTypeId = "{12AC7C43-724D-4ADA-88BB-90CC233468AC}";
} // namespace FoundationOgham
