#ifndef I18N_H
#define I18N_H

/* Language & i18n layer (roadmap P1: English-first conversation with
   ES/FR support). Only the display connectors are localized; the symbol
   map and its names stay untouched, so answers always keep the raw
   symbol names (regression gate: strstr(answer, expected)).

   All templates are ASCII-only on purpose: the QA hygiene gate rejects
   mojibake bytes, and the corpus itself is accent-free. */

typedef enum
{
    LANG_EN = 0,
    LANG_ES,
    LANG_FR,
    LANG_COUNT
} LANG_ID;

/* Display-layer connector slots. Variants give the engine
   conversational variety without any learned component. */
typedef enum
{
    I18N_POSITIVE = 0,   /* "Stored: " / "Guardado: " (NLG fact prefix) */
    I18N_SUGGEST,        /* "Related: " (NLG see-also prefix) */
    I18N_NO_RESULTS,     /* "No direct record of that. " */
    I18N_OPENER,         /* "According to what I have learned, " */
    I18N_AND,            /* " and " list connector */
    I18N_COMPOUND_HEAD,  /* "From my data about " */
    I18N_IS,             /* "is " */
    I18N_OF,             /* " of " */
    I18N_TAXO,           /* " (which is a %s)" taxonomy suffix */
    I18N_UNKNOWN,        /* "No record of %s %s yet." (2 slots, used as format) */
    I18N_NO_RECORD,      /* "No record of that. Ask about something stored." */
    I18N_REPHRASE,       /* "Could not identify the symbols and relation." */
    I18N_SOCIAL,         /* "Hello." small-talk ack */
    I18N_LEARNED,        /* "Stored." learn confirmation */
    I18N_EXTRACT,        /* "Could not extract a clear relation..." */
    I18N_COUNT
} I18N_KEY;

void LangSet(LANG_ID id);                 /* no-op if id >= LANG_COUNT */
LANG_ID LangGet(void);
const char *LangName(LANG_ID id);         /* "English" / "Espanol" / "Francais" */
const char *LangShortName(LANG_ID id);    /* "EN" / "ES" / "FR" */

/* Case-insensitive "EN"/"ES"/"FR" lookup; LANG_COUNT when unknown. */
LANG_ID LangFindByCode(const char *code);

int LangVariantCount(I18N_KEY key);
/* Template for language `id`, variant index wraps by the count. */
const char *LangTemplate(I18N_KEY key, LANG_ID id, int variant);
/* Template for the active language. */
const char *LangString(I18N_KEY key, int variant);

#endif
