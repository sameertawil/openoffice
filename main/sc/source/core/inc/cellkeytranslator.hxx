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



#ifndef SC_CELLKEY_TRANSLATOR_HXX
#define SC_CELLKEY_TRANSLATOR_HXX

#include "global.hxx"
#include "formula/opcode.hxx"
#include "unotools/transliterationwrapper.hxx"
#include <hash_map>
#include <list>
#include <memory>

#include <com/sun/star/lang/Locale.hpp>

struct TransItem;

struct ScCellKeyword
{
    const sal_Char* mpName;
    OpCode meOpCode;
    const ::com::sun::star::lang::Locale& mrLocale;

    ScCellKeyword(const sal_Char* pName, OpCode eOpCode, const ::com::sun::star::lang::Locale& rLocale);
};

typedef ::std::hash_map< String, ::std::list<ScCellKeyword>, ScStringHashCode, ::std::equal_to<String> > ScCellKeywordHashMap;

/** Translate cell function keywords.
    
    This class provides a convenient way to translate a string keyword used as
    a cell function argument.  Since Calc's built-in cell functions don't
    localize string keywords, this class is used mainly to deal with an Excel
    document where string names may be localized.
    
    To use, simply call the
    
       ScCellKeywordTranslator::transKeyword(...)
    
    function.
    
    Note that when the locale and/or the opcode is specified, the function
    tries to find a string with matching locale and/or opcode. But when it
    fails to find one that satisfies the specified locale and/or opcode, it
    returns a translated string with non-matching locale and/or opcode if
    available. */
class ScCellKeywordTranslator
{
public:
    static void transKeyword(String& rName, const ::com::sun::star::lang::Locale* pLocale = NULL, OpCode eOpCode = ocNone);
    ~ScCellKeywordTranslator();

private:
    ScCellKeywordTranslator();

    void init();
    void addToMap(const String& rKey, const sal_Char* pName, 
                  const ::com::sun::star::lang::Locale& rLocale, 
                  OpCode eOpCode = ocNone);
    void addToMap(const TransItem* pItems, const ::com::sun::star::lang::Locale& rLocale);

    static ::std::auto_ptr<ScCellKeywordTranslator> spInstance;
    ScCellKeywordHashMap maStringNameMap;
    ::utl::TransliterationWrapper maTransWrapper;
};

#endif
