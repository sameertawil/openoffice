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
 *   http:\\www.apache.org\licenses\LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/

#ifndef INCLUDED_DRAWINGLAYER_PRIMITIVE2D_TEXTBREAKUPHELPER_HXX
#define INCLUDED_DRAWINGLAYER_PRIMITIVE2D_TEXTBREAKUPHELPER_HXX

#include <drawinglayer/drawinglayerdllapi.h>
#include <drawinglayer/primitive2d/textprimitive2d.hxx>
#include <drawinglayer/primitive2d/textlayoutdevice.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>

//////////////////////////////////////////////////////////////////////////////

namespace drawinglayer
{
    namespace primitive2d
    {
        enum BreakupUnit
        {
            BreakupUnit_character,
            BreakupUnit_word,
            BreakupUnit_sentence
        };

        class DRAWINGLAYER_DLLPUBLIC TextBreakupHelper
        {
        private:
            const TextSimplePortionPrimitive2D&     mrSource;
            Primitive2DSequence                     mxResult;
            TextLayouterDevice                      maTextLayouter;
            basegfx::tools::B2DHomMatrixBufferedOnDemandDecompose maDecTrans;

            /// bitfield
            bool                                    mbNoDXArray : 1;

            /// create a portion from nIndex to nLength and append to rTempResult
            void breakupPortion(Primitive2DVector& rTempResult, sal_uInt32 nIndex, sal_uInt32 nLength, bool bWordLineMode);

            /// breakup complete primitive
            void breakup(BreakupUnit aBreakupUnit);

        protected:
            /// allow user callback to allow changes to the new TextTransformation. Default
            /// does nothing. Retval defines if a primitive gets created, e.g. return false
            /// to suppress creation
            virtual bool allowChange(sal_uInt32 nCount, basegfx::B2DHomMatrix& rNewTransform, sal_uInt32 nIndex, sal_uInt32 nLength);

            /// allow read access to evtl. useful local parts
            const TextLayouterDevice& getTextLayouter() const { return maTextLayouter; }
            const basegfx::tools::B2DHomMatrixBufferedOnDemandDecompose& getDecTrans() const { return maDecTrans; }
            const TextSimplePortionPrimitive2D& getSource() const { return mrSource; }

        public:
            TextBreakupHelper(const TextSimplePortionPrimitive2D& rSource);
            virtual ~TextBreakupHelper();

            /// get result
            const Primitive2DSequence& getResult(BreakupUnit aBreakupUnit = BreakupUnit_character) const;
        };

    } // end of namespace primitive2d
} // end of namespace drawinglayer

//////////////////////////////////////////////////////////////////////////////

#endif //INCLUDED_DRAWINGLAYER_PRIMITIVE2D_TEXTBREAKUPHELPER_HXX

//////////////////////////////////////////////////////////////////////////////
// eof
