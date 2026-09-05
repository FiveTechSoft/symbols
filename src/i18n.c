#include <string.h>
#include <strings.h>
#include "i18n.h"

static LANG_ID s_lang = LANG_EN;

static const int V_COUNT[I18N_COUNT] = {
    6,  /* I18N_POSITIVE       */
    3,  /* I18N_SUGGEST        */
    3,  /* I18N_NO_RESULTS     */
    6,  /* I18N_OPENER         */
    1,  /* I18N_AND            */
    1,  /* I18N_COMPOUND_HEAD  */
    1,  /* I18N_IS             */
    1,  /* I18N_OF             */
    1,  /* I18N_TAXO           */
    3,  /* I18N_UNKNOWN        */
    1,  /* I18N_NO_RECORD      */
    1,  /* I18N_REPHRASE       */
    3,  /* I18N_SOCIAL         */
    3,  /* I18N_LEARNED        */
    1   /* I18N_EXTRACT        */
};

static const char *const E_POSITIVE[6] = {
    "Stored: ", "From the map: ", "Recorded: ",
    "Found: ", "On record: ", "Listed: "
};
static const char *const S_POSITIVE[6] = {
    "Guardado: ", "Desde el mapa: ", "Registrado: ",
    "Encontrado: ", "En el registro: ", "Listado: "
};
static const char *const F_POSITIVE[6] = {
    "Enregistre : ", "Depuis la carte : ", "Registre : ",
    "Trouve : ", "Au registre : ", "Liste : "
};

static const char *const E_SUGGEST[3] = {
    "Also stored about ", "See also ", "Related: "
};
static const char *const S_SUGGEST[3] = {
    "Tambien guardado sobre ", "Ver tambien ", "Relacionado: "
};
static const char *const F_SUGGEST[3] = {
    "Aussi enregistre sur ", "Voir aussi ", "Lie a : "
};

static const char *const E_NO_RESULTS[3] = {
    "No direct record of that. ",
    "That fact is not stored. ",
    "No stored evidence for that. "
};
static const char *const S_NO_RESULTS[3] = {
    "No hay registro directo de eso. ",
    "Ese hecho no esta guardado. ",
    "Sin evidencia guardada para eso. "
};
static const char *const F_NO_RESULTS[3] = {
    "Pas de registre direct a ce sujet. ",
    "Ce fait n'est pas enregistre. ",
    "Aucune preuve enregistree sur ce point. "
};

static const char *const E_OPENER[6] = {
    "According to what I have learned, ",
    "I have recorded that ",
    "Indeed, ",
    "From the information in my map, ",
    "The evidence indicates that ",
    "I have found that "
};
static const char *const S_OPENER[6] = {
    "Segun lo que he aprendido, ",
    "Tengo registrado que ",
    "Efectivamente, ",
    "Por la informacion que manejo, ",
    "La evidencia indica que ",
    "He encontrado que "
};
static const char *const F_OPENER[6] = {
    "Selon ce que j'ai appris, ",
    "J'ai enregistre que ",
    "Effectivement, ",
    "D'apres les informations dont je dispose, ",
    "La preuve indique que ",
    "J'ai trouve que "
};

static const char *const E_AND[1] = { " and " };
static const char *const S_AND[1] = { " y " };
static const char *const F_AND[1] = { " et " };

static const char *const E_COMPOUND_HEAD[1] = { "From my data about " };
static const char *const S_COMPOUND_HEAD[1] = { "De los datos que tengo sobre " };
static const char *const F_COMPOUND_HEAD[1] = { "Selon mes donnees sur " };

static const char *const E_IS[1] = { "is " };
static const char *const S_IS[1] = { "es " };
static const char *const F_IS[1] = { "est " };

static const char *const E_OF[1] = { " of " };
static const char *const S_OF[1] = { " de " };
static const char *const F_OF[1] = { " de " };

static const char *const E_TAXO[1] = { " (which is a %s)" };
static const char *const S_TAXO[1] = { " (que es un %s)" };
static const char *const F_TAXO[1] = { " (qui est un %s)" };

static const char *const E_UNKNOWN[3] = {
    "No record of %s %s yet. Teach it to me.",
    "Nothing stored about %s %s.",
    "Unknown: %s %s. Use /learn S P O to store it."
};
static const char *const S_UNKNOWN[3] = {
    "Sin registro de %s %s por ahora. Ensenamelo.",
    "No hay nada guardado sobre %s %s.",
    "Desconocido: %s %s. Usa /learn S P O para guardarlo."
};
static const char *const F_UNKNOWN[3] = {
    "Pas de trace de %s %s pour l'instant. Apprends-le-moi.",
    "Rien de stocke au sujet de %s %s.",
    "Inconnu : %s %s. Utilisez /learn S P O pour le stocker."
};

static const char *const E_NO_RECORD[1] = {
    "No record of that. Ask about something stored."
};
static const char *const S_NO_RECORD[1] = {
    "No hay registro de eso. Pregunta algo guardado."
};
static const char *const F_NO_RECORD[1] = {
    "Aucun registre a ce sujet. Posez une question sur un fait stocke."
};

static const char *const E_REPHRASE[1] = {
    "Could not identify the symbols and relation. Rephrase it."
};
static const char *const S_REPHRASE[1] = {
    "No pude identificar el simbolo y la relacion. Reformula."
};
static const char *const F_REPHRASE[1] = {
    "Impossible d'identifier le symbole et la relation. Reformulez."
};

static const char *const E_SOCIAL[3] = {
    "Hello.",
    "Noted.",
    "I store symbols and relations. Teach me with /learn S P O."
};
static const char *const S_SOCIAL[3] = {
    "Hola.",
    "Anotado.",
    "Guardo simbolos y relaciones. Ensenamelo con /learn S P O."
};
static const char *const F_SOCIAL[3] = {
    "Bonjour.",
    "Note.",
    "Je stocke des symboles et des relations. Apprends-les-moi avec "
    "/learn S P O."
};

static const char *const E_LEARNED[3] = {
    "Stored.",
    "Noted. Symbols and relation saved.",
    "Saved. Ask me about it."
};
static const char *const S_LEARNED[3] = {
    "Guardado.",
    "Anotado. Simbolos y relacion guardados.",
    "Guardado. Preguntame al respecto."
};
static const char *const F_LEARNED[3] = {
    "Enregistre.",
    "Note. Symboles et relation stockes.",
    "Stocke. Posez-moi des questions a ce sujet."
};

static const char *const E_EXTRACT[1] = {
    "Could not extract a clear relation. Try it as 'symbol relation object'."
};
static const char *const S_EXTRACT[1] = {
    "No extraje una relacion clara. Intentalo como 'simbolo relacion objeto'."
};
static const char *const F_EXTRACT[1] = {
    "Je n'extrais pas de relation claire. Essayez 'symbole relation objet'."
};

static const char *const LANG_NAMES[LANG_COUNT] = {
    "English", "Espanol", "Francais"
};
static const char *const LANG_CODES[LANG_COUNT] = { "EN", "ES", "FR" };

void LangSet(LANG_ID id)
{
    if (id < LANG_COUNT)
        s_lang = id;
}

LANG_ID LangGet(void)
{
    return s_lang;
}

const char *LangName(const LANG_ID id)
{
    return (id >= LANG_COUNT) ? LANG_NAMES[LANG_EN] : LANG_NAMES[id];
}

const char *LangShortName(const LANG_ID id)
{
    return (id >= LANG_COUNT) ? LANG_CODES[LANG_EN] : LANG_CODES[id];
}

const LANG_ID LangFindByCode(const char *code)
{
    if (code == NULL)
        return LANG_COUNT;
    for (int i = 0; i < LANG_COUNT; i++)
    {
        if (strcasecmp(code, LANG_CODES[i]) == 0)
            return (LANG_ID)i;
    }
    return LANG_COUNT;
}

int LangVariantCount(const I18N_KEY key)
{
    if (key >= I18N_COUNT)
        return 0;
    return V_COUNT[key];
}

const char *LangTemplate(const I18N_KEY key, const LANG_ID id, int variant)
{
    int n = LangVariantCount(key);
    LANG_ID lang = id;
    if (n == 0)
        return "";
    if (lang < 0 || lang >= LANG_COUNT)
        lang = LANG_EN;
    if (variant < 0)
        variant = 0;
    variant %= n;

    switch (key)
    {
    case I18N_POSITIVE:
        switch (lang)
        {
        case LANG_EN: return E_POSITIVE[variant];
        case LANG_ES: return S_POSITIVE[variant];
        default:      return F_POSITIVE[variant];
        }
    case I18N_SUGGEST:
        switch (lang)
        {
        case LANG_EN: return E_SUGGEST[variant];
        case LANG_ES: return S_SUGGEST[variant];
        default:      return F_SUGGEST[variant];
        }
    case I18N_NO_RESULTS:
        switch (lang)
        {
        case LANG_EN: return E_NO_RESULTS[variant];
        case LANG_ES: return S_NO_RESULTS[variant];
        default:      return F_NO_RESULTS[variant];
        }
    case I18N_OPENER:
        switch (lang)
        {
        case LANG_EN: return E_OPENER[variant];
        case LANG_ES: return S_OPENER[variant];
        default:      return F_OPENER[variant];
        }
    case I18N_AND:
        switch (lang)
        {
        case LANG_EN: return E_AND[0];
        case LANG_ES: return S_AND[0];
        default:      return F_AND[0];
        }
    case I18N_COMPOUND_HEAD:
        switch (lang)
        {
        case LANG_EN: return E_COMPOUND_HEAD[0];
        case LANG_ES: return S_COMPOUND_HEAD[0];
        default:      return F_COMPOUND_HEAD[0];
        }
    case I18N_IS:
        switch (lang)
        {
        case LANG_EN: return E_IS[0];
        case LANG_ES: return S_IS[0];
        default:      return F_IS[0];
        }
    case I18N_OF:
        switch (lang)
        {
        case LANG_EN: return E_OF[0];
        case LANG_ES: return S_OF[0];
        default:      return F_OF[0];
        }
    case I18N_TAXO:
        switch (lang)
        {
        case LANG_EN: return E_TAXO[0];
        case LANG_ES: return S_TAXO[0];
        default:      return F_TAXO[0];
        }
    case I18N_UNKNOWN:
        switch (lang)
        {
        case LANG_EN: return E_UNKNOWN[variant];
        case LANG_ES: return S_UNKNOWN[variant];
        default:      return F_UNKNOWN[variant];
        }
    case I18N_NO_RECORD:
        switch (lang)
        {
        case LANG_EN: return E_NO_RECORD[0];
        case LANG_ES: return S_NO_RECORD[0];
        default:      return F_NO_RECORD[0];
        }
    case I18N_REPHRASE:
        switch (lang)
        {
        case LANG_EN: return E_REPHRASE[0];
        case LANG_ES: return S_REPHRASE[0];
        default:      return F_REPHRASE[0];
        }
    case I18N_SOCIAL:
        switch (lang)
        {
        case LANG_EN: return E_SOCIAL[variant];
        case LANG_ES: return S_SOCIAL[variant];
        default:      return F_SOCIAL[variant];
        }
    case I18N_LEARNED:
        switch (lang)
        {
        case LANG_EN: return E_LEARNED[variant];
        case LANG_ES: return S_LEARNED[variant];
        default:      return F_LEARNED[variant];
        }
    default:
        switch (lang)
        {
        case LANG_EN: return E_EXTRACT[0];
        case LANG_ES: return S_EXTRACT[0];
        default:      return F_EXTRACT[0];
        }
    }
    return "";
}

const char *LangString(const I18N_KEY key, int variant)
{
    return LangTemplate(key, s_lang, variant);
}
