//=============================================================================
// Copyright (C) 2001 Radical Entertainment Ltd.  All Rights Reserved.
//
// Component   : Scrooby
//
// Description : Implementation of Textbible classes, for referencing strings
//               with hashes in the runtime
//
// Authors     : Mike Perzel
//
//=============================================================================

//=============================================================================
// Includes
//=============================================================================
#include <raddebug.hpp>
#include <stdio.h>
#include <string.h>
#include "FeLanguage.h"
#include "FeTextBible.h"
#include "strings/unicodeString.h"
#include "utility/debugMessages.h"
//=============================================================================
// Local constants, typedefs, macros
//=============================================================================

//=============================================================================
// Global data, local data, local classes
//=============================================================================
// The header on the textbible files is the string XLAT, but for the purposes
// of endian checking, we will treat it as an integer
static const unsigned int s_headerValue = 0x54414C58;

// Some installed language tables contain question-mark placeholders for
// individual strings. Treat them like missing translations so English can be
// used for those entries while valid localized strings remain unchanged.
static bool IsQuestionMarkPlaceholder( const UnicodeChar* text )
{
    if( text == NULL )
    {
        return false;
    }

    bool foundQuestionMark = false;
    for( const UnicodeChar* p = text; *p != 0; ++p )
    {
        if( *p == static_cast<UnicodeChar>( '?' ) )
        {
            foundQuestionMark = true;
        }
        else if( *p != static_cast<UnicodeChar>( ' ' ) &&
                 *p != static_cast<UnicodeChar>( '\t' ) &&
                 *p != static_cast<UnicodeChar>( '\r' ) &&
                 *p != static_cast<UnicodeChar>( '\n' ) )
        {
            return false;
        }
    }

    return foundQuestionMark;
}

char FeTextBible::m_currentLanguageID = 'E';
unsigned int FeTextBible::m_index = 0;

//=============================================================================
// Member functions
//=============================================================================

//=============================================================================
// FeTextBible::FeTextBible
//=============================================================================
// Description : Default Constructor
//
//=============================================================================
FeTextBible::FeTextBible()
:
//m_languageIDs( NULL ),
//m_index( 0 ),
m_englishFallback( NULL ),
m_tempString( NULL )
{
    m_languageIDs[0] = '\0';
}

//=============================================================================
// FeTextBible::~FeTextBible
//=============================================================================
// Description : Default Destructor
//
// Constraints : This is protected, and can only be reached by using the release
//               method (from tRefCounted)
//
//=============================================================================
FeTextBible::~FeTextBible()
{
    int i = 0;
    while( m_languages.Size() )
    {
        FeLanguage* language = m_languages[m_languages.Size()-1];
        if( language != NULL )
        {
            language->Release();
        }
        m_languages.Erase( m_languages.Size()-1 );
    }
    if( m_tempString )
    {
        delete m_tempString;
    }
    if( m_englishFallback != NULL )
    {
        m_englishFallback->Release();
        m_englishFallback = NULL;
    }
/*
    if( m_languageIDs )
    {
        delete[] m_languageIDs;
    }
*/
}


//=============================================================================
// FeTextBible::GetString
//=============================================================================
// Description : Grabs a string from the bible based on it's hash value
//
// Parameters  : hash - integer hash value for the string to be retrieved
//
// Returns     : A UnicodeString object with the string contained in it
//
//=============================================================================
UnicodeString FeTextBible::GetString( unsigned int hash )
{
    if( m_tempString == NULL )
    {
        m_tempString = new UnicodeString();
    }
    UnicodeChar* val = GetWChar( hash );
    if( val )
    {
        m_tempString->ReadUnicode( val );
    }
    else
    {
        m_tempString->ReadAscii( "ERROR : NO TRANSLATION" );
    }
    // We can't return by reference here, because this string can change much
    // too quickly for us to count it as a static class
    return *m_tempString;
}

//=============================================================================
// FeTextBible::GetString
//=============================================================================
// Description : Grabs a string from the bible based on it's symbolic key name
//
// Parameters  : key - a string containing the symbolic name for the string in
//               the bible to be retrieved
//
// Returns     : A UnicodeString object with the string contained in it
//
//=============================================================================
UnicodeString FeTextBible::GetString( const char* key )
{
    if( m_tempString == NULL )
    {
        m_tempString = new UnicodeString();
    }

    UnicodeChar* val = GetWChar( key );
    if( val )
    {
        m_tempString->ReadUnicode( val );
    }
    else
    {
        m_tempString->ReadAscii( "ERROR : NO TRANSLATION" );
    }
    return *m_tempString;
}


//=============================================================================
// FeTextBible::GetWChar
//=============================================================================
// Description : Grabs a string from the bible based on it's hash ID
//
// Constraints : The array returned must be deleted by the object which
//               retrieves the string
//
// Parameters  : hash - integer hash value for the string to be retrieved
//
// Returns     : A pointer to an array of short's with the string contained
//               in it, or NULL if the string was not found
//
//=============================================================================
unsigned short* FeTextBible::GetWChar( unsigned int hash )
{
    FeLanguage* currentLanguage =
        m_index < static_cast<unsigned int>( m_languages.Size() )
        ? m_languages[ m_index ] : NULL;
    unsigned short* value = currentLanguage != NULL
        ? currentLanguage->GetString( hash ) : NULL;

    if( ( value == NULL || IsQuestionMarkPlaceholder( value ) ) &&
        m_englishFallback != NULL &&
        m_englishFallback != currentLanguage )
    {
        unsigned short* englishValue = m_englishFallback->GetString( hash );
        if( englishValue != NULL )
        {
            return englishValue;
        }
    }

    return value;
}

//=============================================================================
// FeTextBible::GetWChar
//=============================================================================
// Description : Grabs a string from the bible based on it's symbolic name
//
// Constraints : The array returned must be deleted by the object which
//               retrieves the string
//
// Parameters  : key - a string containing the symbolic name for the string in
//               the bible to be retrieved
//
// Returns     : A pointer to an array of short's with the string contained
//               in it, or NULL if the string was not found
//
//=============================================================================
UnicodeChar* FeTextBible::GetWChar( const char* key )
{
    FeLanguage* currentLanguage =
        m_index < static_cast<unsigned int>( m_languages.Size() )
        ? m_languages[ m_index ] : NULL;
    UnicodeChar* value = currentLanguage != NULL
        ? currentLanguage->GetString( key ) : NULL;

    if( ( value == NULL || IsQuestionMarkPlaceholder( value ) ) &&
        m_englishFallback != NULL &&
        m_englishFallback != currentLanguage )
    {
        UnicodeChar* englishValue = m_englishFallback->GetString( key );
        if( englishValue != NULL )
        {
            return englishValue;
        }
    }

    return value;
}

bool FeTextBible::HasLanguageID( char languageID ) const
{
    if( languageID == '\0' )
    {
        return false;
    }

    for( int i = 0; i < MAX_NUM_LANGUAGES && m_languageIDs[i] != '\0'; ++i )
    {
        if( m_languageIDs[i] == languageID )
        {
            return true;
        }
    }

    return false;
}

void FeTextBible::SetCurrentLanguageID( char languageID )
{
    m_currentLanguageID = languageID;
}

void FeTextBible::SetLanguage( unsigned int index )
{
    if( index > 0 && index < static_cast<unsigned int>(m_languages.Size()) )
    {
        //Scrooby::Log::Message( Scrooby::LVL_ERROR, "FeTextBible::SetLanguage - index not valid" );
        rAssert( false );
        // No change in the current index
    }
    else
    {
        m_index = index;
    }
}

void FeTextBible::SetLanguage( char language )
{
    m_currentLanguageID = language;
    int i = strlen( m_languageIDs );
    for( ; i >= 0; i-- )
    {
        if( m_languageIDs[i] == language )
        {
            m_index = i;
            break;
        }
    }
}
