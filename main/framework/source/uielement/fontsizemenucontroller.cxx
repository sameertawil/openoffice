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



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_framework.hxx"
#include <uielement/fontsizemenucontroller.hxx>

//_________________________________________________________________________________________________________________
//	my own includes
//_________________________________________________________________________________________________________________
#include <threadhelp/resetableguard.hxx>
#include "services.h"

//_________________________________________________________________________________________________________________
//	interface includes
//_________________________________________________________________________________________________________________
#include <com/sun/star/awt/XDevice.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/awt/MenuItemStyle.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/view/XPrintable.hpp>

//_________________________________________________________________________________________________________________
//	includes of other projects
//_________________________________________________________________________________________________________________

#ifndef _VCL_MENU_HXX_
#include <vcl/menu.hxx>
#endif
#include <tools/mapunit.hxx>
#ifndef _VCL_SVAPP_HXX_
#include <vcl/svapp.hxx>
#endif
#include <vcl/i18nhelp.hxx>
#ifndef _VCL_OUTPUTDEVICE_HXX_
#include <vcl/outdev.hxx>
#endif
#include <vcl/print.hxx>
#ifndef _SVTOOLS_CTRLTOOL_HXX_
#include <svtools/ctrltool.hxx>
#endif
#include <dispatch/uieventloghelper.hxx>
#include <vos/mutex.hxx>

//_________________________________________________________________________________________________________________
//	Defines
//_________________________________________________________________________________________________________________
// 

using namespace com::sun::star::uno;
using namespace com::sun::star::lang;
using namespace com::sun::star::frame;
using namespace com::sun::star::beans;
using namespace com::sun::star::util;
using namespace com::sun::star::view;
using namespace com::sun::star::beans;

namespace framework
{

DEFINE_XSERVICEINFO_MULTISERVICE        (   FontSizeMenuController			            ,
                                            OWeakObject                                 ,
                                            SERVICENAME_POPUPMENUCONTROLLER			    ,
											IMPLEMENTATIONNAME_FONTSIZEMENUCONTROLLER
										)

DEFINE_INIT_SERVICE                     (   FontSizeMenuController, {} )

FontSizeMenuController::FontSizeMenuController( const ::com::sun::star::uno::Reference< ::com::sun::star::lang::XMultiServiceFactory >& xServiceManager ) :
	svt::PopupMenuControllerBase( xServiceManager ),
    m_pHeightArray( 0 ),
    m_bRebuildMenu( sal_True )
{
}

FontSizeMenuController::~FontSizeMenuController()
{
    delete []m_pHeightArray;
}

// private function
rtl::OUString FontSizeMenuController::retrievePrinterName( com::sun::star::uno::Reference< com::sun::star::frame::XFrame >& rFrame )
{
    rtl::OUString aPrinterName;
                
    if ( rFrame.is() )
    {
        Reference< XController > xController = m_xFrame->getController();
        if ( xController.is() )
        {
            Reference< XPrintable > xPrintable( xController->getModel(), UNO_QUERY );
            if ( xPrintable.is() )
            {
                Sequence< PropertyValue > aPrinterSeq = xPrintable->getPrinter();
                for ( int i = 0; i < aPrinterSeq.getLength(); i++ )
                {
                    if ( aPrinterSeq[i].Name.equalsAscii( "Name" ))
                    {
                        aPrinterSeq[i].Value >>= aPrinterName;
                        break;
                    }
                }
            }
        }
    }
    
    return aPrinterName;
}

// private function
void FontSizeMenuController::setCurHeight( long nHeight, Reference< css::awt::XPopupMenu >& rPopupMenu )
{
	// check menu item
	rtl::OUString	aHeight     = Application::GetSettings().GetUILocaleI18nHelper().GetNum( nHeight, 1, sal_True, sal_False  );
	sal_uInt16		    nChecked    = 0;
	sal_uInt16		    nItemCount  = rPopupMenu->getItemCount();
	for( sal_uInt16 i = 0; i < nItemCount; i++ )
	{
		sal_uInt16 nItemId = rPopupMenu->getItemId( i );

		if ( m_pHeightArray[i] == nHeight )
		{
			rPopupMenu->checkItem( nItemId, sal_True );
			return;
		}

		if ( rPopupMenu->isItemChecked( nItemId ) )
			nChecked = nItemId;
	}

	if ( nChecked )
		rPopupMenu->checkItem( nChecked, sal_False );
}

// private function
void FontSizeMenuController::fillPopupMenu( Reference< css::awt::XPopupMenu >& rPopupMenu )
{
    const rtl::OUString     aFontNameCommand( RTL_CONSTASCII_USTRINGPARAM( ".uno:FontHeight?FontHeight=" ));
    VCLXPopupMenu*          pPopupMenu = (VCLXPopupMenu *)VCLXMenu::GetImplementation( rPopupMenu );
    PopupMenu*              pVCLPopupMenu = 0;
    
    resetPopupMenu( rPopupMenu );
    if ( pPopupMenu )
        pVCLPopupMenu = (PopupMenu *)pPopupMenu->GetMenu();
        
    if ( pVCLPopupMenu )
    {
        FontList*       pFontList = 0;
        Printer*        pInfoPrinter = 0;
        rtl::OUString   aPrinterName;
        
        vos::OGuard aSolarMutexGuard( Application::GetSolarMutex() );
        
        // try to retrieve printer name of document
        aPrinterName = retrievePrinterName( m_xFrame );
        if ( aPrinterName.getLength() > 0 )
        {
            pInfoPrinter = new Printer( aPrinterName );
            if ( pInfoPrinter && pInfoPrinter->GetDevFontCount() > 0 )
                pFontList = new FontList( pInfoPrinter );
        }
        
        if ( pFontList == 0 )
            pFontList   = new FontList( Application::GetDefaultDevice() );

        FontInfo aFntInfo = pFontList->Get( m_aFontDescriptor.Name, m_aFontDescriptor.StyleName );
        
	    // setup font size array
	    if ( m_pHeightArray )
		    delete m_pHeightArray;

	    const long* pTempAry;
	    const long* pAry = pFontList->GetSizeAry( aFntInfo );
	    sal_uInt16 nSizeCount = 0;
	    while ( pAry[nSizeCount] )
		    nSizeCount++;

	    sal_uInt16 nPos = 0;
        const rtl::OUString aFontHeightCommand( RTL_CONSTASCII_USTRINGPARAM( ".uno:FontHeight?FontHeight.Height:float=" ));
	    
        // first insert font size names (for simplified/traditional chinese)
	    float           fPoint;
        rtl::OUString   aHeightString;
        FontSizeNames   aFontSizeNames( Application::GetSettings().GetUILanguage() );
	    m_pHeightArray = new long[nSizeCount+aFontSizeNames.Count()];
        rtl::OUString   aCommand;
        
        if ( !aFontSizeNames.IsEmpty() )
	    {
		    if ( pAry == pFontList->GetStdSizeAry() )
		    {
			    // for scalable fonts all font size names
			    sal_uLong nCount = aFontSizeNames.Count();
			    for( sal_uLong i = 0; i < nCount; i++ )
			    {
				    String	aSizeName = aFontSizeNames.GetIndexName( i );
				    long	nSize = aFontSizeNames.GetIndexSize( i );
				    m_pHeightArray[nPos] = nSize;
				    nPos++; // Id is nPos+1
				    pVCLPopupMenu->InsertItem( nPos, aSizeName, MIB_RADIOCHECK | MIB_AUTOCHECK );
				    fPoint = float( m_pHeightArray[nPos-1] ) / 10;
                    
                    // Create dispatchable .uno command and set it
                    aCommand = aFontHeightCommand + rtl::OUString::valueOf( fPoint );
                    pVCLPopupMenu->SetItemCommand( nPos, aCommand );
			    }
		    }
		    else
		    {
			    // for fixed size fonts only selectable font size names
			    pTempAry = pAry;
			    while ( *pTempAry )
			    {
				    String aSizeName = aFontSizeNames.Size2Name( *pTempAry );
				    if ( aSizeName.Len() )
				    {
					    m_pHeightArray[nPos] = *pTempAry;
					    nPos++; // Id is nPos+1
					    pVCLPopupMenu->InsertItem( nPos, aSizeName, MIB_RADIOCHECK | MIB_AUTOCHECK );
				        fPoint = float( m_pHeightArray[nPos-1] ) / 10;

                        // Create dispatchable .uno command and set it
                        aCommand = aFontHeightCommand + rtl::OUString::valueOf( fPoint );
                        pVCLPopupMenu->SetItemCommand( nPos, aCommand );
                    }                        
				    pTempAry++;
			    }
		    }
	    }

	    // then insert numerical font size values
        const vcl::I18nHelper& rI18nHelper = Application::GetSettings().GetUILocaleI18nHelper();
	    pTempAry = pAry;
	    while ( *pTempAry )
	    {
		    m_pHeightArray[nPos] = *pTempAry;
		    nPos++; // Id is nPos+1
		    pVCLPopupMenu->InsertItem( nPos, rI18nHelper.GetNum( *pTempAry, 1, sal_True, sal_False ), MIB_RADIOCHECK | MIB_AUTOCHECK );
            fPoint = float( m_pHeightArray[nPos-1] ) / 10;

            // Create dispatchable .uno command and set it
            aCommand = aFontHeightCommand + rtl::OUString::valueOf( fPoint );
            pVCLPopupMenu->SetItemCommand( nPos, aCommand );
		    
            pTempAry++;
	    }

	    setCurHeight( long( m_aFontHeight.Height * 10), rPopupMenu );
	    
	    delete pFontList;
	    delete pInfoPrinter;
    }
}

// XEventListener
void SAL_CALL FontSizeMenuController::disposing( const EventObject& ) throw ( RuntimeException )
{
    Reference< css::awt::XMenuListener > xHolder(( OWeakObject *)this, UNO_QUERY );

    osl::MutexGuard aLock( m_aMutex );
    m_xFrame.clear();
    m_xDispatch.clear();
    m_xCurrentFontDispatch.clear();
    if ( m_xPopupMenu.is() )
        m_xPopupMenu->removeMenuListener( Reference< css::awt::XMenuListener >(( OWeakObject *)this, UNO_QUERY ));
    m_xPopupMenu.clear();
}

// XStatusListener
void SAL_CALL FontSizeMenuController::statusChanged( const FeatureStateEvent& Event ) throw ( RuntimeException )
{
    com::sun::star::awt::FontDescriptor                 aFontDescriptor;
    ::com::sun::star::frame::status::FontHeight   aFontHeight;

    if ( Event.State >>= aFontDescriptor )
    {
        osl::MutexGuard aLock( m_aMutex );
        m_aFontDescriptor = aFontDescriptor;
        
        if ( m_xPopupMenu.is() )
            fillPopupMenu( m_xPopupMenu );

    }
    else if ( Event.State >>= aFontHeight )
    {
        osl::MutexGuard aLock( m_aMutex );
        m_aFontHeight = aFontHeight;

	    if ( m_xPopupMenu.is() )
        {
            vos::OGuard aSolarMutexGuard( Application::GetSolarMutex() );
	        setCurHeight( long( m_aFontHeight.Height * 10), m_xPopupMenu );
        }
    }
}

// XMenuListener
void FontSizeMenuController::impl_select(const Reference< XDispatch >& _xDispatch,const ::com::sun::star::util::URL& aTargetURL)
{
    Sequence<PropertyValue>	     aArgs;
    if(::comphelper::UiEventsLogger::isEnabled()) //#i88653#
        UiEventLogHelper(::rtl::OUString::createFromAscii("FontSizeMenuController")).log(m_xServiceManager, m_xFrame, aTargetURL, aArgs);
	OSL_ENSURE(_xDispatch.is(),"FontSizeMenuController::impl_select: No dispatch");
	if ( _xDispatch.is() )
		_xDispatch->dispatch( aTargetURL, aArgs );
}

// XPopupMenuController
void FontSizeMenuController::impl_setPopupMenu()
{
    Reference< XDispatchProvider > xDispatchProvider( m_xFrame, UNO_QUERY );
    com::sun::star::util::URL aTargetURL;
    // Register for font name updates which gives us info about the current font!
    aTargetURL.Complete = rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( ".uno:CharFontName" ));
    m_xURLTransformer->parseStrict( aTargetURL );
    m_xCurrentFontDispatch = xDispatchProvider->queryDispatch( aTargetURL, ::rtl::OUString(), 0 );
}
		
void SAL_CALL FontSizeMenuController::updatePopupMenu() throw ( ::com::sun::star::uno::RuntimeException )
{
    osl::ClearableMutexGuard aLock( m_aMutex );

	throwIfDisposed();
    
    Reference< XDispatch > xDispatch( m_xCurrentFontDispatch );
    com::sun::star::util::URL aTargetURL;
    aTargetURL.Complete = rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( ".uno:CharFontName" ));
    m_xURLTransformer->parseStrict( aTargetURL );
    aLock.clear();

    if ( xDispatch.is() )
    {
        xDispatch->addStatusListener( SAL_STATIC_CAST( XStatusListener*, this ), aTargetURL );
        xDispatch->removeStatusListener( SAL_STATIC_CAST( XStatusListener*, this ), aTargetURL );
    }

	svt::PopupMenuControllerBase::updatePopupMenu();
}
}
