#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "dialogue.h"

static void ToLower(const char *src, char *dst, size_t max)
{
    size_t i = 0;
    while (src[i] && i < max - 1)
    {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

static int ContainsWord(const char *text, const char *word)
{
    char low[512];
    char wlow[128];
    ToLower(text, low, sizeof(low));
    ToLower(word, wlow, sizeof(wlow));

    const char *p = strstr(low, wlow);
    if (p == NULL)
        return 0;

    size_t wlen = strlen(wlow);
    if (p > low && *(p - 1) != ' ' && *(p - 1) != ',' && *(p - 1) != '.' && *(p - 1) != '!')
        return 0;
    if (*(p + wlen) != '\0' && *(p + wlen) != ' ' && *(p + wlen) != ',' &&
        *(p + wlen) != '.' && *(p + wlen) != '!' && *(p + wlen) != '?')
        return 0;

    return 1;
}

DIALOGUE_INTENT DialogueClassify(const char *input)
{
    DIALOGUE_INTENT intent;
    memset(&intent, 0, sizeof(intent));
    intent.act = ACT_STATEMENT;
    intent.confidence = 0.5f;

    if (input == NULL)
        return intent;

    char low[512];
    ToLower(input, low, sizeof(low));
    size_t len = strlen(low);

    /* Strip trailing punctuation for matching */
    while (len > 0 && (low[len-1] == '.' || low[len-1] == '!' ||
           low[len-1] == '?' || low[len-1] == ' '))
    {
        low[--len] = '\0';
    }

    /* Greetings */
    if (ContainsWord(low, "hola") || ContainsWord(low, "buenos dias") ||
        ContainsWord(low, "buenas tardes") || ContainsWord(low, "buenas noches") ||
        ContainsWord(low, "hey") || ContainsWord(low, "saludos") ||
        ContainsWord(low, "que tal") || strcmp(low, "hi") == 0 ||
        strcmp(low, "hello") == 0)
    {
        intent.act = ACT_GREETING;
        intent.confidence = 0.95f;
        return intent;
    }

    /* Farewells */
    if (ContainsWord(low, "adios") || ContainsWord(low, "hasta luego") ||
        ContainsWord(low, "chao") || ContainsWord(low, "nos vemos") ||
        ContainsWord(low, "me voy") || ContainsWord(low, "hasta pronto") ||
        strcmp(low, "bye") == 0 || ContainsWord(low, "me despido"))
    {
        intent.act = ACT_FAREWELL;
        intent.confidence = 0.95f;
        return intent;
    }

    /* Thanks */
    if (ContainsWord(low, "gracias") || ContainsWord(low, "muchas gracias") ||
        ContainsWord(low, "te agradezco") || ContainsWord(low, "muy amable") ||
        ContainsWord(low, "genial"))
    {
        intent.act = ACT_THANKS;
        intent.confidence = 0.95f;
        return intent;
    }

    /* Agreement */
    if (ContainsWord(low, "de acuerdo") || ContainsWord(low, "correcto") ||
        ContainsWord(low, "exacto") || ContainsWord(low, "claro") ||
        ContainsWord(low, "si exactamente") || ContainsWord(low, "así es") ||
        ContainsWord(low, "tal cual"))
    {
        intent.act = ACT_AGREEMENT;
        intent.confidence = 0.90f;
        return intent;
    }

    /* Disagreement */
    if (ContainsWord(low, "no es correcto") || ContainsWord(low, "equivocado") ||
        ContainsWord(low, "no acuerdo") || ContainsWord(low, "falso") ||
        ContainsWord(low, "eso no") || ContainsWord(low, "para nada"))
    {
        intent.act = ACT_DISAGREEMENT;
        intent.confidence = 0.85f;
        return intent;
    }

    /* Identity query */
    if (ContainsWord(low, "quien eres") || ContainsWord(low, "que eres") ||
        ContainsWord(low, "como te llamas") || ContainsWord(low, "tu nombre") ||
        ContainsWord(low, "que sabes hacer"))
    {
        intent.act = ACT_IDENTITY_QUERY;
        intent.confidence = 0.90f;
        return intent;
    }

    /* Capability query */
    if (ContainsWord(low, "que puedes") || ContainsWord(low, "que sabes") ||
        ContainsWord(low, "que sabes preguntar") || ContainsWord(low, "para que sirves") ||
        ContainsWord(low, "que haces"))
    {
        intent.act = ACT_CAPABILITY_QUERY;
        intent.confidence = 0.90f;
        return intent;
    }

    /* Apology */
    if (ContainsWord(low, "perdon") || ContainsWord(low, "disculpa") ||
        ContainsWord(low, "lo siento"))
    {
        intent.act = ACT_APOLOGY;
        intent.confidence = 0.85f;
        return intent;
    }

    /* Question detection */
    if (len > 0 && (low[len-1] == '?' || strstr(low, " que ") ||
        strstr(low, " como ") || strstr(low, " donde ") ||
        strstr(low, " quien ") || strstr(low, " cuanto ") ||
        strstr(low, " porque ") || strstr(low, " cual ")))
    {
        intent.act = ACT_QUESTION;
        intent.confidence = 0.70f;
        return intent;
    }

    return intent;
}

int DialogueRespond(SPEECH_ACT act, const char *input,
                    char *out, uint32_t out_size)
{
    if (out == NULL || out_size == 0)
        return 0;

    out[0] = '\0';

    switch (act)
    {
        case ACT_GREETING:
        {
            const char *responses[] = {
                "Hola. Puedo ayudarte con preguntas sobre el conocimiento que poseo.",
                "Hola. Estoy listo para responder consultas sobre hechos y conceptos.",
                "Hola. Preguntame lo que quieras sobre el grafo de conocimiento.",
                "Hola. Tengo acceso a un grafo de relaciones. En que puedo ayudarte?"
            };
            /* Simple deterministic selection based on string length */
            uint32_t idx = 0;
            if (input)
            {
                size_t len = strlen(input);
                idx = (uint32_t)(len % 4);
            }
            snprintf(out, out_size, "%s", responses[idx]);
            return 1;
        }

        case ACT_FAREWELL:
        {
            const char *responses[] = {
                "Hasta luego. El conocimiento permanece en el grafo para cuando vuelvas.",
                "Adios. Puedes cargar el modelo con /save para no perder nada.",
                "Nos vemos. Recuerda que el grafo se actualiza en tiempo real.",
                "Hasta pronto. Cualquier nuevo hecho que aprenda estara disponible."
            };
            uint32_t idx = 0;
            if (input)
                idx = (uint32_t)(strlen(input) % 4);
            snprintf(out, out_size, "%s", responses[idx]);
            return 1;
        }

        case ACT_THANKS:
        {
            const char *responses[] = {
                "De nada. El conocimiento esta para ser consultado.",
                "A la orden. Si necesitas mas datos, pregunta con confianza.",
                "Por nada. Cada respuesta viene de evidencia almacenada, no de adivinanzas.",
                "Con gusto. Recuerda: sin alucinaciones, solo hechos verificables."
            };
            uint32_t idx = 0;
            if (input)
                idx = (uint32_t)(strlen(input) % 4);
            snprintf(out, out_size, "%s", responses[idx]);
            return 1;
        }

        case ACT_AGREEMENT:
        {
            snprintf(out, out_size,
                "Correcto. Si tienes mas preguntas sobre el tema, estoy a disposicion.");
            return 1;
        }

        case ACT_DISAGREEMENT:
        {
            snprintf(out, out_size,
                "Entendido. Si posees informacion contradictoria, "
                "puedes ensenarmela y la almacenare en el grafo.");
            return 1;
        }

        case ACT_IDENTITY_QUERY:
        {
            snprintf(out, out_size,
                "Sistema de razonamiento simbolico basado en grafos de conocimiento. "
                "No uso redes neuronales ni matrices densas. "
                "Mi conocimiento reside en relaciones explicitas <Sujeto, Predicado, Objeto> "
                "y razono por encadenamiento deductivo con confianza atenuada.");
            return 1;
        }

        case ACT_CAPABILITY_QUERY:
        {
            snprintf(out, out_size,
                "Puedo: (1) aprender hechos nuevos, (2) responder preguntas sobre el grafo, "
                "(3) razonar por encadenamiento profundo, (4) detectar sinonimos por embeddings 32D, "
                "(5) detectar contradicciones, (6) guardar y cargar modelos. "
                "Escribe una frase para ensenarme o pregunta lo que quieras.");
            return 1;
        }

        case ACT_APOLOGY:
        {
            snprintf(out, out_size,
                "No hay problema. Si la respuesta no fue satisfactoria, "
                "intentare con otra consulta o puedes reformular la pregunta.");
            return 1;
        }

        case ACT_CONFUSION:
        {
            snprintf(out, out_size,
                "No estoy seguro de entender. Puedes reformular tu pregunta "
                "o preguntar sobre un concepto especifico del grafo.");
            return 1;
        }

        default:
            return 0;
    }
}
