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


#ifndef _FMTROWSPLT_HXX
#define _FMTROWSPLT_HXX

#include <svl/eitem.hxx>
#include "swdllapi.h"
#include <hintids.hxx>
#include <format.hxx>

class IntlWrapper;

class SW_DLLPUBLIC SwFmtRowSplit : public SfxBoolItem
{
public:
    SwFmtRowSplit( sal_Bool bSplit = sal_True ) : SfxBoolItem( RES_ROW_SPLIT, bSplit ) {}

	// "pure virtual Methoden" vom SfxPoolItem
	virtual SfxPoolItem*    Clone( SfxItemPool *pPool = 0 ) const;
    virtual SfxItemPresentation GetPresentation( SfxItemPresentation ePres,
									SfxMapUnit eCoreMetric,
									SfxMapUnit ePresMetric,
									String &rText,
                                    const IntlWrapper*    pIntl = 0 ) const;
};

inline const SwFmtRowSplit &SwAttrSet::GetRowSplit(sal_Bool bInP) const
    { return (const SwFmtRowSplit&)Get( RES_ROW_SPLIT,bInP); }

inline const SwFmtRowSplit &SwFmt::GetRowSplit(sal_Bool bInP) const
    { return aSet.GetRowSplit(bInP); }

#endif

