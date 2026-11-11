//===========================================================================
// Copyright (C) 2003 Radical Entertainment Ltd.  All rights reserved.
//
// Authors:     Tony Chu
//
// Revisions		Date			Author	    Revision
//                  2003/02/26		TChu		Created
//
//===========================================================================

//===========================================================================
// Includes
//===========================================================================

#include <presentation/gui/guitextbible.h>

#include <App.h>
#include <TextBible.h>
#include <FeTextBible.h>
#include <raddebug.hpp>

#include <sound/soundmanager.h>

Scrooby::TextBible* CGuiTextBible::s_textBible = NULL;
Scrooby::XLLanguage CGuiTextBible::s_currentLanguage = Scrooby::XL_ENGLISH;

//===========================================================================
// Public Member Functions
//===========================================================================

CGuiTextBible::CGuiTextBible()
{
}

CGuiTextBible::~CGuiTextBible()
{
    s_textBible = NULL;
}

void
CGuiTextBible::SetTextBible( const char* textBible )
{
    rAssert( textBible != NULL );
    s_textBible = Scrooby::App::GetInstance()->GetTextBible( textBible );
}

P3D_UNICODE*
CGuiTextBible::GetLocalizedText( const char* stringID )
{
    rAssert( stringID != NULL );
    rAssert( s_textBible != NULL );

    P3D_UNICODE* localizedText = static_cast<P3D_UNICODE*>( s_textBible->GetWChar( stringID ) );

/*
#ifndef RAD_RELEASE
    if( localizedText == NULL )
    {
        char msg[ 256 ];
        sprintf( msg, "Can't find text bible string for entry: %s!", stringID );
        rTuneWarningMsg( false, msg );
    }
#endif
*/

    return localizedText;
}

void
CGuiTextBible::SetCurrentLanguage( const Scrooby::XLLanguage language )
{
    Scrooby::XLLanguage resolvedLanguage = language;

    // English-only North American data exposes only its English table. Do not
    // leave the application marked as Spanish/French/German if that text table
    // is absent: use English for both the effective UI language and its voice
    // preference. If a localized text table is present, keep the selection;
    // individual missing/???? strings still fall back to English in FeTextBible.
    if( resolvedLanguage > Scrooby::XL_ENGLISH &&
        resolvedLanguage < Scrooby::XL_LAST_LANGUAGE &&
        s_textBible != NULL )
    {
        FeTextBible* activeBible = dynamic_cast<FeTextBible*>( s_textBible );
        if( activeBible != NULL )
        {
            const char requestedLanguageID = Scrooby::language_ID[ resolvedLanguage ];
            const bool englishAvailable =
                activeBible->HasLanguageID( 'E' ) ||
                activeBible->HasLanguageID( 'e' );

            if( !activeBible->HasLanguageID( requestedLanguageID ) &&
                englishAvailable )
            {
                resolvedLanguage = Scrooby::XL_ENGLISH;
            }
        }
    }

    Scrooby::App::GetInstance()->SetLocalizationLanguage( resolvedLanguage );

    s_currentLanguage = resolvedLanguage;

    GetSoundManager()->SetDialogueLanguage( resolvedLanguage );
}

Scrooby::XLLanguage
CGuiTextBible::GetCurrentLanguage()
{
    return s_currentLanguage;
}

bool CGuiTextBible::IsTextBibleLoaded()
{
    return s_textBible != NULL;
}


