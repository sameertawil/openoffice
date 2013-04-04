/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/

#include "precompiled_sfx2.hxx"

#include "ResourceManager.hxx"
#include "Tools.hxx"

#include <unotools/confignode.hxx>
#include <comphelper/componentcontext.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/namedvaluecollection.hxx>
#include <comphelper/types.hxx>
#include <comphelper/stlunosequence.hxx>

#include <rtl/ustrbuf.hxx>
#include <tools/diagnose_ex.h>

#include <com/sun/star/frame/XModuleManager.hpp>

#include <map>



using ::rtl::OUString;
using namespace css;
using namespace cssu;

namespace sfx2 { namespace sidebar {

#define gsPrivateResourceToolpanelPrefix "private:resource/toolpanel/"



class ResourceManager::Deleter
{
public:
    void operator() (ResourceManager* pObject)
    {
        delete pObject;
    }
};


ResourceManager& ResourceManager::Instance (void)
{
    static ResourceManager maInstance;
    return maInstance;
}




ResourceManager::ResourceManager (void)
    : maDecks(),
      maPanels(),
      maProcessedApplications()
{
    ReadDeckList();
    ReadPanelList();
}




ResourceManager::~ResourceManager (void)
{
    maPanels.clear();
    maDecks.clear();
}




const DeckDescriptor* ResourceManager::GetBestMatchingDeck (
    const Context& rContext,
    const Reference<frame::XFrame>& rxFrame)
{
    ReadLegacyAddons(rxFrame);
    
    for (DeckContainer::const_iterator iDeck(maDecks.begin()), iEnd(maDecks.end());
         iDeck!=iEnd;
         ++iDeck)
    {
        if (iDeck->maContextList.GetMatch(rContext) != NULL)
            return &*iDeck;
    }
    return NULL;
}




const DeckDescriptor* ResourceManager::GetDeckDescriptor (
    const ::rtl::OUString& rsDeckId) const
{
    for (DeckContainer::const_iterator
             iDeck(maDecks.begin()),
             iEnd(maDecks.end());
         iDeck!=iEnd;
         ++iDeck)
    {
        if (iDeck->msId.equals(rsDeckId))
            return &*iDeck;
    }
    return NULL;
}




const PanelDescriptor* ResourceManager::GetPanelDescriptor (
    const ::rtl::OUString& rsPanelId) const
{
    for (PanelContainer::const_iterator
             iPanel(maPanels.begin()),
             iEnd(maPanels.end());
         iPanel!=iEnd;
         ++iPanel)
    {
        if (iPanel->msId.equals(rsPanelId))
            return &*iPanel;
    }
    return NULL;
}




void ResourceManager::SetIsDeckEnabled (
    const ::rtl::OUString& rsDeckId,
    const bool bIsEnabled)
{
    for (DeckContainer::iterator
             iDeck(maDecks.begin()),
             iEnd(maDecks.end());
         iDeck!=iEnd;
         ++iDeck)
    {
        if (iDeck->msId.equals(rsDeckId))
        {
            iDeck->mbIsEnabled = bIsEnabled;
            return;
        }
    }
}




const ResourceManager::IdContainer& ResourceManager::GetMatchingDecks (
    IdContainer& rDeckIds,
    const Context& rContext,
    const Reference<frame::XFrame>& rxFrame)
{
    ReadLegacyAddons(rxFrame);

    ::std::multimap<sal_Int32,OUString> aOrderedIds;
    for (DeckContainer::const_iterator
             iDeck(maDecks.begin()),
             iEnd (maDecks.end());
         iDeck!=iEnd;
         ++iDeck)
    {
        const DeckDescriptor& rDeckDescriptor (*iDeck);
        if (rDeckDescriptor.maContextList.GetMatch(rContext) != NULL)
            aOrderedIds.insert(::std::multimap<sal_Int32,OUString>::value_type(
                    rDeckDescriptor.mnOrderIndex,
                    rDeckDescriptor.msId));
    }

    for (::std::multimap<sal_Int32,OUString>::const_iterator
             iId(aOrderedIds.begin()),
             iEnd(aOrderedIds.end());
         iId!=iEnd;
         ++iId)
    {
        rDeckIds.push_back(iId->second);
    }
    
    return rDeckIds;
}




const ResourceManager::PanelContextDescriptorContainer& ResourceManager::GetMatchingPanels (
    PanelContextDescriptorContainer& rPanelIds,
    const Context& rContext,
    const ::rtl::OUString& rsDeckId,
    const Reference<frame::XFrame>& rxFrame)
{
    ReadLegacyAddons(rxFrame);

    ::std::multimap<sal_Int32,PanelContextDescriptor> aOrderedIds;
    for (PanelContainer::const_iterator
             iPanel(maPanels.begin()),
             iEnd(maPanels.end());
         iPanel!=iEnd;
         ++iPanel)
    {
        const PanelDescriptor& rPanelDescriptor (*iPanel);
        if (rPanelDescriptor.msDeckId.equals(rsDeckId))
        {
            const ContextList::Entry* pEntry = rPanelDescriptor.maContextList.GetMatch(rContext);
            if (pEntry != NULL)
            {
                PanelContextDescriptor aPanelContextDescriptor;
                aPanelContextDescriptor.msId = rPanelDescriptor.msId;
                aPanelContextDescriptor.msMenuCommand = pEntry->msMenuCommand;
                aPanelContextDescriptor.mbIsInitiallyVisible = pEntry->mbIsInitiallyVisible;
                aOrderedIds.insert(::std::multimap<sal_Int32,PanelContextDescriptor>::value_type(
                        rPanelDescriptor.mnOrderIndex,
                        aPanelContextDescriptor));
            }
        }
    }

    for (::std::multimap<sal_Int32,PanelContextDescriptor>::const_iterator
             iId(aOrderedIds.begin()),
             iEnd(aOrderedIds.end());
         iId!=iEnd;
         ++iId)
    {
        rPanelIds.push_back(iId->second);
    }
    
    return rPanelIds;
}




void ResourceManager::ReadDeckList (void)
{
    const ::comphelper::ComponentContext aContext (::comphelper::getProcessServiceFactory());
    const ::utl::OConfigurationTreeRoot aDeckRootNode (
        aContext,
        A2S("org.openoffice.Office.UI.Sidebar/Content/DeckList"),
        false);
    if ( ! aDeckRootNode.isValid() )
        return;

    const Sequence<OUString> aDeckNodeNames (aDeckRootNode.getNodeNames());
    const sal_Int32 nCount (aDeckNodeNames.getLength());
    maDecks.resize(nCount);
    sal_Int32 nWriteIndex(0);
    for (sal_Int32 nReadIndex(0); nReadIndex<nCount; ++nReadIndex)
    {
        const ::utl::OConfigurationNode aDeckNode (aDeckRootNode.openNode(aDeckNodeNames[nReadIndex]));
        if ( ! aDeckNode.isValid())
            continue;

        DeckDescriptor& rDeckDescriptor (maDecks[nWriteIndex++]);

        rDeckDescriptor.msTitle = ::comphelper::getString(
            aDeckNode.getNodeValue("Title"));
        rDeckDescriptor.msId = ::comphelper::getString(
            aDeckNode.getNodeValue("Id"));
        rDeckDescriptor.msIconURL = ::comphelper::getString(
            aDeckNode.getNodeValue("IconURL"));
        rDeckDescriptor.msHighContrastIconURL = ::comphelper::getString(
            aDeckNode.getNodeValue("HighContrastIconURL"));
        rDeckDescriptor.msHelpURL = ::comphelper::getString(
            aDeckNode.getNodeValue("HelpURL"));
        rDeckDescriptor.msHelpText = rDeckDescriptor.msTitle;
        rDeckDescriptor.mbIsEnabled = true;
        rDeckDescriptor.mnOrderIndex = ::comphelper::getINT32(
            aDeckNode.getNodeValue("OrderIndex"));

        ReadContextList(
            aDeckNode,
            rDeckDescriptor.maContextList,
            OUString());
    }

    // When there where invalid nodes then we have to adapt the size
    // of the deck vector.
    if (nWriteIndex<nCount)
        maDecks.resize(nWriteIndex);
}




void ResourceManager::ReadPanelList (void)
{
    const ::comphelper::ComponentContext aContext (::comphelper::getProcessServiceFactory());
    const ::utl::OConfigurationTreeRoot aPanelRootNode (
        aContext,
        A2S("org.openoffice.Office.UI.Sidebar/Content/PanelList"),
        false);
    if ( ! aPanelRootNode.isValid() )
        return;

    const Sequence<OUString> aPanelNodeNames (aPanelRootNode.getNodeNames());
    const sal_Int32 nCount (aPanelNodeNames.getLength());
    maPanels.resize(nCount);
    sal_Int32 nWriteIndex (0);
    for (sal_Int32 nReadIndex(0); nReadIndex<nCount; ++nReadIndex)
    {
        const ::utl::OConfigurationNode aPanelNode (aPanelRootNode.openNode(aPanelNodeNames[nReadIndex]));
        if ( ! aPanelNode.isValid())
            continue;

        PanelDescriptor& rPanelDescriptor (maPanels[nWriteIndex++]);

        rPanelDescriptor.msTitle = ::comphelper::getString(
            aPanelNode.getNodeValue("Title"));
        rPanelDescriptor.mbIsTitleBarOptional = ::comphelper::getBOOL(
            aPanelNode.getNodeValue("TitleBarIsOptional"));
        rPanelDescriptor.msId = ::comphelper::getString(
            aPanelNode.getNodeValue("Id"));
        rPanelDescriptor.msDeckId = ::comphelper::getString(
            aPanelNode.getNodeValue("DeckId"));
        rPanelDescriptor.msHelpURL = ::comphelper::getString(
            aPanelNode.getNodeValue("HelpURL"));
        rPanelDescriptor.msImplementationURL = ::comphelper::getString(
            aPanelNode.getNodeValue("ImplementationURL"));
        rPanelDescriptor.mnOrderIndex = ::comphelper::getINT32(
            aPanelNode.getNodeValue("OrderIndex"));
        rPanelDescriptor.mbWantsCanvas = ::comphelper::getBOOL(
            aPanelNode.getNodeValue("WantsCanvas"));
        const OUString sDefaultMenuCommand (::comphelper::getString(
                aPanelNode.getNodeValue("DefaultMenuCommand")));

        ReadContextList(
            aPanelNode,
            rPanelDescriptor.maContextList,
            sDefaultMenuCommand);
    }

    // When there where invalid nodes then we have to adapt the size
    // of the deck vector.
    if (nWriteIndex<nCount)
        maPanels.resize(nWriteIndex);
}




void ResourceManager::ReadContextList (
    const ::utl::OConfigurationNode& rParentNode,
    ContextList& rContextList,
    const OUString& rsDefaultMenuCommand) const
{
    const Any aValue = rParentNode.getNodeValue("ContextList");
    Sequence<OUString> aValues;
    sal_Int32 nCount;
    if (aValue >>= aValues)
        nCount = aValues.getLength();
    else
        nCount = 0;

    for (sal_Int32 nIndex=0; nIndex<nCount; ++nIndex)
    {
        const OUString sValue (aValues[nIndex]);
        sal_Int32 nCharacterIndex (0);
        const OUString sApplicationName (sValue.getToken(0, ',', nCharacterIndex).trim());
        if (nCharacterIndex < 0)
        {
            if (sApplicationName.getLength() == 0)
            {
                // This is a valid case: in the XML file the separator
                // was used as terminator.  Using it in the last line
                // creates an additional but empty entry.
                break;
            }
            else
            {
                OSL_ASSERT("expecting three or four values per ContextList entry, separated by comma");
                continue;
            }
        }
        
        const OUString sContextName (sValue.getToken(0, ',', nCharacterIndex).trim());
        if (nCharacterIndex < 0)
        {
            OSL_ASSERT("expecting three or four values per ContextList entry, separated by comma");
            continue;
        }

        const OUString sInitialState (sValue.getToken(0, ',', nCharacterIndex).trim());

        // The fourth argument is optional.
        const OUString sMenuCommandOverride (
            nCharacterIndex<0
                ? OUString()
                : sValue.getToken(0, ',', nCharacterIndex).trim());
        const OUString sMenuCommand (
            sMenuCommandOverride.getLength()>0
                ? (sMenuCommandOverride.equalsAscii("none")
                    ? OUString()
                    : sMenuCommandOverride)
                : rsDefaultMenuCommand);

        EnumContext::Application eApplication (EnumContext::GetApplicationEnum(sApplicationName));
        EnumContext::Application eApplication2 (EnumContext::Application_None);
        if (eApplication == EnumContext::Application_None
            && !sApplicationName.equals(EnumContext::GetApplicationName(EnumContext::Application_None)))
        {
            // Handle some special names: abbreviations that make
            // context descriptions more readable.
            if (sApplicationName.equalsAscii("Writer"))
                eApplication = EnumContext::Application_Writer;
            else if (sApplicationName.equalsAscii("Calc"))
                eApplication = EnumContext::Application_Calc;
            else if (sApplicationName.equalsAscii("Draw"))
                eApplication = EnumContext::Application_Draw;
            else if (sApplicationName.equalsAscii("Impress"))
                eApplication = EnumContext::Application_Impress;
            else if (sApplicationName.equalsAscii("DrawImpress"))
            {
                // A special case among the special names:  it is
                // common to use the same context descriptions for
                // both Draw and Impress.  This special case helps to
                // avoid duplication in the .xcu file.
                eApplication = EnumContext::Application_Draw;
                eApplication2 = EnumContext::Application_Impress;
            }
            else if (sApplicationName.equalsAscii("WriterAndWeb"))
            {
                // Another special case for Writer and WriterWeb.
                eApplication = EnumContext::Application_Writer;
                eApplication2 = EnumContext::Application_WriterWeb;
            }
            else
            {
                OSL_ASSERT("application name not recognized");
                continue;
            }
        }
        
        const EnumContext::Context eContext (EnumContext::GetContextEnum(sContextName));
        if (eContext == EnumContext::Context_Unknown)
        {
            OSL_ASSERT("context name not recognized");
            continue;
        }

        bool bIsInitiallyVisible;
        if (sInitialState.equalsAscii("visible"))
            bIsInitiallyVisible = true;
        else if (sInitialState.equalsAscii("hidden"))
            bIsInitiallyVisible = false;
        else
        {
            OSL_ASSERT("unrecognized state");
            continue;
        }

        if (eApplication != EnumContext::Application_None)
            rContextList.AddContextDescription(
                Context(
                    EnumContext::GetApplicationName(eApplication),
                    EnumContext::GetContextName(eContext)),
                bIsInitiallyVisible,
                sMenuCommand);
        if (eApplication2 != EnumContext::Application_None)
            rContextList.AddContextDescription(
                Context(
                    EnumContext::GetApplicationName(eApplication2),
                    EnumContext::GetContextName(eContext)),
                bIsInitiallyVisible,
                sMenuCommand);
    }
}




void ResourceManager::ReadLegacyAddons (const Reference<frame::XFrame>& rxFrame)
{
    // Get module name for given frame.
    ::rtl::OUString sModuleName (GetModuleName(rxFrame));
    if (sModuleName.getLength() == 0)
        return;
    if (maProcessedApplications.find(sModuleName) != maProcessedApplications.end())
    {
        // Addons for this application have already been read.
        // There is nothing more to do.
        return;
    }

    // Mark module as processed.  Even when there is an error that
    // prevents the configuration data from being read, this error
    // will not be triggered a second time.
    maProcessedApplications.insert(sModuleName);

    // Get access to the configuration root node for the application.
    ::utl::OConfigurationTreeRoot aLegacyRootNode (GetLegacyAddonRootNode(sModuleName));
    if ( ! aLegacyRootNode.isValid())
        return;

    // Process child nodes.
    ::std::vector<OUString> aMatchingNodeNames;
    GetToolPanelNodeNames(aMatchingNodeNames, aLegacyRootNode);
    const sal_Int32 nCount (aMatchingNodeNames.size());
    size_t nDeckWriteIndex (maDecks.size());
    size_t nPanelWriteIndex (maPanels.size());
    maDecks.resize(maDecks.size() + nCount);
    maPanels.resize(maPanels.size() + nCount);
    for (sal_Int32 nReadIndex(0); nReadIndex<nCount; ++nReadIndex)
    {
        const OUString& rsNodeName (aMatchingNodeNames[nReadIndex]);
        const ::utl::OConfigurationNode aChildNode (aLegacyRootNode.openNode(rsNodeName));
        if ( ! aChildNode.isValid())
            continue;

        DeckDescriptor& rDeckDescriptor (maDecks[nDeckWriteIndex++]);
        rDeckDescriptor.msTitle = ::comphelper::getString(aChildNode.getNodeValue("UIName"));
        rDeckDescriptor.msId = rsNodeName;
        rDeckDescriptor.msIconURL = ::comphelper::getString(aChildNode.getNodeValue("ImageURL"));
        rDeckDescriptor.msHighContrastIconURL = rDeckDescriptor.msIconURL;
        rDeckDescriptor.msHelpURL = ::comphelper::getString(aChildNode.getNodeValue("HelpURL"));
        rDeckDescriptor.msHelpText = rDeckDescriptor.msTitle;
        rDeckDescriptor.maContextList.AddContextDescription(Context(sModuleName, A2S("any")), true, OUString());
        rDeckDescriptor.mbIsEnabled = true;

        PanelDescriptor& rPanelDescriptor (maPanels[nPanelWriteIndex++]);
        rPanelDescriptor.msTitle = ::comphelper::getString(aChildNode.getNodeValue("UIName"));
        rPanelDescriptor.mbIsTitleBarOptional = true;
        rPanelDescriptor.msId = rsNodeName;
        rPanelDescriptor.msDeckId = rsNodeName;
        rPanelDescriptor.msHelpURL = ::comphelper::getString(aChildNode.getNodeValue("HelpURL"));
        rPanelDescriptor.maContextList.AddContextDescription(Context(sModuleName, A2S("any")), true, OUString());
        rPanelDescriptor.msImplementationURL = rsNodeName;            
    }

    // When there where invalid nodes then we have to adapt the size
    // of the deck and panel vectors.
    if (nDeckWriteIndex < maDecks.size())
        maDecks.resize(nDeckWriteIndex);
    if (nPanelWriteIndex < maPanels.size())
        maPanels.resize(nPanelWriteIndex);
}




::rtl::OUString ResourceManager::GetModuleName (
    const cssu::Reference<css::frame::XFrame>& rxFrame)
{
    if ( ! rxFrame.is() || ! rxFrame->getController().is())
        return OUString();
    
    try
    {
        const ::comphelper::ComponentContext aContext (::comphelper::getProcessServiceFactory());
        const Reference<frame::XModuleManager> xModuleManager (
            aContext.createComponent("com.sun.star.frame.ModuleManager"),
            UNO_QUERY_THROW);
        return xModuleManager->identify(rxFrame);
    }
    catch (const Exception&)
    {
        DBG_UNHANDLED_EXCEPTION();
    }
    return OUString();
}   




::utl::OConfigurationTreeRoot ResourceManager::GetLegacyAddonRootNode (
    const ::rtl::OUString& rsModuleName) const
{
    try
    {
        const ::comphelper::ComponentContext aContext (::comphelper::getProcessServiceFactory());
        const Reference<container::XNameAccess> xModuleAccess (
            aContext.createComponent("com.sun.star.frame.ModuleManager"),
            UNO_QUERY_THROW);
        const ::comphelper::NamedValueCollection aModuleProperties (xModuleAccess->getByName(rsModuleName));
        const ::rtl::OUString sWindowStateRef (aModuleProperties.getOrDefault(
                "ooSetupFactoryWindowStateConfigRef",
                ::rtl::OUString()));

        ::rtl::OUStringBuffer aPathComposer;
        aPathComposer.appendAscii("org.openoffice.Office.UI.");
        aPathComposer.append(sWindowStateRef);
        aPathComposer.appendAscii("/UIElements/States");

        return ::utl::OConfigurationTreeRoot(aContext, aPathComposer.makeStringAndClear(), false);
    }
    catch( const Exception& )
    {
        DBG_UNHANDLED_EXCEPTION();
    }

    return ::utl::OConfigurationTreeRoot();
}




void ResourceManager::GetToolPanelNodeNames (
    ::std::vector<OUString>& rMatchingNames,
    const ::utl::OConfigurationTreeRoot aRoot) const
{
    Sequence<OUString> aChildNodeNames (aRoot.getNodeNames());
    const sal_Int32 nCount (aChildNodeNames.getLength());
    for (sal_Int32 nIndex(0); nIndex<nCount; ++nIndex)
    {
        if (aChildNodeNames[nIndex].matchAsciiL(
                RTL_CONSTASCII_STRINGPARAM( "private:resource/toolpanel/")))
            rMatchingNames.push_back(aChildNodeNames[nIndex]);
    }
}



} } // end of namespace sfx2::sidebar