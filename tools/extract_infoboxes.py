"""
Wikipedia Infobox Extractor — Extracts structured triples from Wikipedia templates.
Wikipedia articles contain {{Infobox ...}} templates with key=value pairs
that are already semantic triples ready to ingest.
"""
import re
import html
import requests
import time
from collections import Counter

# Contactable UA: Wikimedia blocks generic/bot-like agents (403).
WIKI_UA = ('SymbolicLLM-research/1.0 '
           '(https://github.com/FiveTechSoft/symbols; corpus research)')

# Map common infobox keys to our predicate vocabulary
KEY_PREDICATE_MAP = {
    # Geography
    "capital": "CAPITAL",
    "ciudad_capital": "CAPITAL",
    "capital_sede": "CAPITAL",
    "continente": "CONTINENTE",
    "región": "REGION",
    "subregión": "SUBREGION",
    "país": "PAIS",
    "estado": "ESTADO",
    "provincia": "PROVINCIA",
    "municipio": "MUNICIPIO",
    "fronteras": "FRONTERA_CON",
    "costa": "COSTA_CON",
    "superficie": "SUPERFICIE",
    "población": "POBLACION",
    "densidad": "DENSIDAD",
    "gentilicio": "GENTILICIO",
    "idioma": "IDIOMA",
    "idiomas": "IDIOMA",
    "idioma_oficial": "IDIOMA_OFICIAL",
    "idiomas_oficiales": "IDIOMA_OFICIAL",
    "moneda": "MONEDA",
    "huso_horario": "HUSO_HORARIO",
    "dominio": "DOMINIO",
    "código_iso": "CODIGO_ISO",
    "bandera": "BANDERA",
    "escudo": "ESCUDO",
    "himno": "HIMNO",

    # People
    "nacimiento": "NACIO_EN",
    "fallecimiento": "FALLCIO_EN",
    "lugar_nacimiento": "NACIO_EN",
    "lugar_fallecimiento": "FALLCIO_EN",
    "fecha_nacimiento": "NACIO_EL",
    "fecha_fallecimiento": "FALLCIO_EL",
    "nacionalidad": "NACIONALIDAD",
    "profesión": "PROFESION",
    "ocupación": "OCUPACION",
    "trabajo": "TRABAJO_EN",
    "conocido_por": "CONOCIDO_POR",
    "educación": "ESTUDIO_EN",
    "cónyuge": "CONYUGE_DE",
    "hijos": "HIJO_DE",
    "padres": "PADRE_DE",
    "premios": "PREMIO",
    "agente_libre": "AGENTE_LIBRE",

    # Organizations
    "fundación": "FUNDADO_EN",
    "fundado": "FUNDADO_EN",
    "fundador": "FUNDADO_POR",
    "sede": "SEDE_EN",
    "miembros": "MIEMBRO_DE",
    "industria": "INDUSTRIA",
    "productos": "PRODUCE",
    "servicios": "PROVEE",
    "ingresos": "INGRESOS",
    "empleados": "EMPLEADOS",
    "organización_padre": "PERTENECE_A",

    # Works
    "autor": "CREADO_POR",
    "escritor": "ESCRITO_POR",
    "director": "DIRIGIDO_POR",
    "productor": "PRODUCIDO_POR",
    "género": "GENERO",
    "genero": "GENERO",
    "formato": "FORMATO",
    "duración": "DURACION",
    "lanzamiento": "LANZADO_EN",
    "año": "ANIO",
    "isbn": "ISBN",

    # Biology
    "reino": "REINO",
    "filo": "FILO",
    "clase": "CLASE",
    "orden": "ORDEN",
    "familia": "FAMILIA",
    "género生物科技": "GENERO",
    "especie": "ESPECIE",

    # Transport
    "aeropuerto": "AEROPUERTO",
    "estación": "ESTACION",
    "carretera": "CARRETERA",
    "matrícula": "MATRICULA",
    "motor": "MOTOR",
    "potencia": "POTENCIA",
    "velocidad": "VELOCIDAD_MAX",
    "capacidad": "CAPACIDAD",

    # Misc
    "tipo": "TIPO",
    "clase": "CLASE",
    "categoría": "CATEGORIA",
    "categoria": "CATEGORIA",
    "situación": "SITUACION",
    "situacion": "SITUACION",
    "estado": "ESTADO",
    "status": "ESTADO",
    "forma_gobierno": "GOBIERNO",
    "gobierno": "GOBIERNO",
    "presidente": "PRESIDENTE",
    "primer_ministro": "PRIMER_MINISTRO",
    "monarca": "MONARCA",
}

# Infobox template patterns (case-insensitive)
INFOBOX_PATTERNS = [
    r'\{\{Ficha de país',
    r'\{\{Ficha de persona',
    r'\{\{Ficha de organización',
    r'\{\{Ficha de obra',
    r'\{\{Infobox',
    r'\{\{Place',
    r'\{\{Country',
    r'\{\{Person',
    r'\{\{Organization',
    r'\{\{Album',
    r'\{\{Film',
    r'\{\{Videojuego',
    r'\{\{Empresa',
    r'\{\{Localidad',
    r'\{\{Municipio',
    r'\{\{Río',
    r'\{\{Montaña',
    r'\{\{Isla',
    r'\{\{Lago',
]


def split_top_level(text):
    """Split an infobox body into field chunks.

    Fields start at line-leading '|' (MediaWiki convention); chunks
    swallowed by multiline {{templates}} are re-joined until braces
    balance, so inner pipes ({{nowrap|X}}, citations, file links)
    never become separators. A globally depth-tracked split would
    drift on the first unbalanced fragment and lose every later
    field; balance is re-anchored per field instead. Shattering a
    value is what produced the poison ({{NOWRAP, INGLÉS_}},
    [[ARCHIVO:...). Depth 1 (not 0) is the flush level: the text
    spans one outer {{...}} whose closing }} was stripped by the
    caller, so every complete field returns pending-opens to
    exactly 1. Stray closers may flush early (the tail becomes a
    keyless chunk the pair loop skips); stray openers merge to end
    (the cleaner drops the blob): degradation stays local, never
    shifts later fields. """
    def bal(s):
        return s.count('{{') + s.count('[[') - s.count('}}') - s.count(']]')

    parts, buf, depth = [], [], 0
    for ch in re.split(r'\n\s*\|', text):
        buf.append(ch)
        depth += bal(ch)
        if depth <= 1:
            parts.append('\n|'.join(buf))
            buf = []
    if buf:
        parts.append('\n|'.join(buf))
    return parts


def clean_infobox_value(val):
    """Clean a wikitext value: remove templates, links, HTML.

    Template/link removal runs innermost-first to fixpoint, so nested
    templates ({{cita web|...{{...}}...}}) vanish whole instead of
    leaving their name as text (KMCITA_WEB). HTML entities (&nbsp;)
    are markup, never content. Anything still carrying unbalanced
    markup residue ([[{, ]], <, >) is a shattered fragment: return ''
    so the pair is dropped, never laundered into a clean-looking lie.
    """
    # File-namespace links ([[Archivo:...|20px|...]]) are images,
    # never content: drop whole (lint already treats their tails
    # as file references, never facts).
    val = re.sub(r'\[\[(?:Archivo|File|Image|Imagen):[^\]]*\]\]', '',
                 val, flags=re.IGNORECASE)
    # Unwrap whole-value wrappers bearing a real link:
    # {{nowrap|[[Corona noruega]]...}} → the content after the
    # template name (the fact). Citations/markers ({{cita web|...}},
    # {{esd}}) carry no link and fall through to deletion below.
    val = re.sub(r'^\s*\{\{[^|{}]*\|([^{}]*\[\[[^\]]+\]\][^{}]*)\}\}\s*$',
                 r'\1', val)
    # Remove {{template}} innermost-first to fixpoint (nesting-safe)
    prev = None
    while prev != val:
        prev = val
        val = re.sub(r'\{\{[^{}]*\}\}', '', val)
    # Remove [[link|a|b|display]] → display (LAST segment), or
    # [[link]] → link (fixpoint)
    prev = None
    while prev != val:
        prev = val
        val = re.sub(r'\[\[(?:[^\]]*\|)?([^\]|]+)\]\]', r'\1', val)
    # Remove <ref>...</ref>
    val = re.sub(r'<ref[^>]*>.*?</ref>', '', val, flags=re.DOTALL)
    val = re.sub(r'<ref[^>]*/?>', '', val)
    # Remove HTML tags, then entities (&nbsp; &amp; ...)
    val = re.sub(r'<[^>]+>', '', val)
    val = html.unescape(val).replace(' ', ' ')
    # Remove wiki formatting
    val = re.sub(r"'{2,}", '', val)
    val = re.sub(r'{{(?:negrita|nihongo|lang)[^}]*}}', '', val)
    # Clean whitespace
    val = re.sub(r'\s+', ' ', val).strip()
    # Remove trailing commas, dots
    val = val.rstrip('.,;:')
    # Fragment backstop: unbalanced markup residue or splitter
    # residue (|) never cleans, and no legit value starts with
    # punctuation (suffix tails like "-a"): return '' so the pair
    # is dropped, never laundered into a clean-looking lie.
    if re.search(r'[{}\[\]<>|]', val):
        return ''
    if val and not re.match(r'(?u)[^\W_]', val):
        return ''
    return val


def clean_infobox_key(key):
    """Clean a wikitext key: lowercase, remove spaces/underscores."""
    key = key.strip().lower()
    key = re.sub(r'[\s_]+', '_', key)
    key = key.strip('_')
    return key


def parse_infobox(text):
    """
    Parse Wikipedia wikitext and extract infobox key=value pairs.
    Returns list of (key, value) tuples.
    """
    pairs = []

    # Find infobox start
    infobox_start = -1
    for pattern in INFOBOX_PATTERNS:
        m = re.search(pattern, text, re.IGNORECASE)
        if m:
            infobox_start = m.start()
            break

    if infobox_start == -1:
        return pairs

    # Find the matching closing }}
    depth = 0
    i = infobox_start
    while i < len(text) - 1:
        if text[i:i+2] == '{{':
            depth += 1
            i += 2
        elif text[i:i+2] == '}}':
            depth -= 1
            if depth == 0:
                break
            i += 2
        else:
            i += 1

    infobox_text = text[infobox_start:i]

    # Parse key = value pairs (splitter is brace-aware, see above)
    lines = split_top_level(infobox_text)

    for line in lines:
        line = line.strip()
        if '=' not in line:
            continue

        # Split on first =
        eq_pos = line.index('=')
        key = line[:eq_pos].strip()
        val = line[eq_pos+1:].strip()

        # Skip template name (first line)
        double_open = '{{'
        if key.startswith(double_open):
            continue

        # Clean
        key = clean_infobox_key(key)
        val = clean_infobox_value(val)

        if key and val and len(val) > 1:
            pairs.append((key, val))

    return pairs


def triples_from_infobox(article_title, pairs):
    """Convert infobox key=value pairs to (subject, predicate, object) triples."""
    triples = []
    subject = article_title.upper().replace(' ', '_')

    for key, val in pairs:
        pred = KEY_PREDICATE_MAP.get(key)
        if pred is None:
            # Use raw key as predicate
            pred = key.upper()

        # Handle multiple values (comma-separated)
        values = [v.strip() for v in re.split(r',\s*(?![^()]*\))', val) if v.strip()]

        for v in values:
            if len(v) > 1 and len(v) < 100:
                # Skip values that are just numbers or dates
                if re.match(r'^[\d.,/\-]+$', v):
                    continue
                # Absence markers are not facts
                if v.strip().upper() in NULL_VALUES:
                    continue
                # Suffix tails ("-a" from "Noruego, -a") and splitter
                # residue never become symbols (same fragment rule as
                # the cleaner: content starts alphanumeric, carries no
                # markup).
                if not re.match(r'(?u)[^\W_]', v):
                    continue
                if re.search(r'[{}\[\]<>|]', v):
                    continue
                obj = v.upper().replace(' ', '_')
                triples.append((subject, pred, obj))

    return triples


def fetch_infoboxes_wikipedia(titles, lang='es', limit=500):
    """
    Fetch wikitext for multiple articles and extract infobox triples.
    Returns list of (subject, predicate, object) triples.
    """
    api_url = f"https://{lang}.wikipedia.org/w/api.php"
    session = requests.Session()
    session.headers.update({'User-Agent': WIKI_UA})

    all_triples = []
    stats = Counter()

    # Process in batches of 50 (MediaWiki API limit)
    for batch_start in range(0, min(len(titles), limit), 50):
        batch = titles[batch_start:batch_start+50]

        # Fetch wikitext for batch
        params = {
            'action': 'query',
            'titles': '|'.join(batch),
            'prop': 'revisions',
            'rvprop': 'content',
            'rvslots': 'main',
            'format': 'json',
        }

        try:
            resp = session.get(api_url, params=params, timeout=30)
            if resp.status_code != 200:
                time.sleep(2)
                continue
            data = resp.json()
        except:
            time.sleep(2)
            continue

        pages = data.get('query', {}).get('pages', {})
        for pid, page in pages.items():
            title = page.get('title', '')
            revisions = page.get('revisions', [])
            if not revisions:
                continue

            content = revisions[0].get('slots', {}).get('main', {}).get('*', '')
            if not content:
                continue

            pairs = parse_infobox(content)
            if pairs:
                triples = triples_from_infobox(title, pairs)
                all_triples.extend(triples)
                stats[title] = len(triples)

        time.sleep(0.5)

    return all_triples, stats


# Whole-value absence markers: "no value" is not a fact
# (JAPÓN IDIOMA_OFICIAL NINGUNO). Never become symbols.
# "-" excluded on purpose: MENOS SIMBOLO - is real (minus sign).
NULL_VALUES = frozenset([
    'NINGUNO', 'NINGUNA', 'NINGUN', 'NINGÚN',
    'NO', 'N/A',
])


# Common Wikipedia categories to scrape infoboxes from
CATEGORY_TITLES = {
    "países": [
        "España", "Francia", "Alemania", "Italia", "Portugal", "Brasil", "México",
        "Argentina", "Colombia", "Chile", "Perú", "Venezuela", "Cuba", "Ecuador",
        "Bolivia", "Paraguay", "Uruguay", "Costa_Rica", "Panamá", "Guatemala",
        "Honduras", "El_Salvador", "Nicaragua", "República_Dominicana", "Haití",
        "Jamaica", "Trinidad_y_Tobago", "Belice", "Guyana", "Surinam",
        "Estados_Unidos", "Canadá", "Reino_Unido", "Rusia", "China",
        "Japón", "Corea_del_Sur", "India", "Australia", "Nueva_Zelanda",
        "Sudáfrica", "Egipto", "Nigeria", "Kenia", "Marruecos",
        "Turquía", "Arabia_Saudita", "Irán", "Irak", "Israel",
        "Noruega", "Suecia", "Finlandia", "Dinamarca", "Países_Bajos",
        "Bélgica", "Suiza", "Austria", "Polonia", "Chequia",
        "Hungría", "Rumanía", "Bulgaria", "Grecia", "Croacia",
    ],
    "ciudades": [
        "Madrid", "Barcelona", "París", "Londres", "Roma", "Berlín",
        "Lisboa", "Buenos_Aires", "São_Paulo", "Ciudad_de_México",
        "Bogotá", "Lima", "Santiago_de_Chile", "Caracas", "Quito",
        "La_Habana", "Santo_Domingo", "San_José", "Panamá",
        "Nueva_York", "Los_Ángeles", "Chicago", "Miami",
        "Tokio", "Pekín", "Shanghái", "Bombay", "Delhi",
        "El_Cairo", "Estambul", "Moscú", "San_Petersburgo",
        "Ámsterdam", "Bruselas", "Viena", "Praga", "Varsovia",
        "Budapest", "Bucarest", "Atenas", "Zagreb", "Belgrado",
    ],
    "personas": [
        "Albert_Einstein", "Marie_Curie", "Isaac_Newton", "Charles_Darwin",
        "Leonardo_da_Vinci", "Miguel_Ángel", "William_Shakespeare",
        "Mozart", "Beethoven", "Ludwig_van_Beethoven",
        "Napoleón_Bonaparte", "Julio_César", "Alejandro_Magno",
        "Simón_Bolívar", "José_de_San_Martín", "Bernardo_O'Higgins",
        "Antonio_de_Nebrija", "Miguel_de_Cervantes", "Lope_de_Vega",
        "Pablo_Picasso", "Salvador_Dalí", "Federico_García_Lorca",
        "Gabriel_García_Márquez", "Pablo_Neruda", "Octavio_Paz",
        "Mario_Vargas_Llosa", "Isabel_Allende", "Jorge_Luis_Borges",
        "Linus_Torvalds", "Steve_Jobs", "Bill_Gates",
        "Tim_Berners-Lee", "Vint_Cerf", "Robert_Kahn",
    ],
}


if __name__ == "__main__":
    print("=== Wikipedia Infobox Extractor ===\n")

    all_triples = []
    all_stats = Counter()

    for category, titles in CATEGORY_TITLES.items():
        print(f"Fetching {category} ({len(titles)} articles)...")
        triples, stats = fetch_infoboxes_wikipedia(titles, limit=len(titles))
        all_triples.extend(triples)
        all_stats.update(stats)
        print(f"  Got {len(triples)} triples from {len(stats)} articles\n")

    # Deduplicate
    unique = list(set(all_triples))
    print(f"\nTotal: {len(all_triples)} triples, {len(unique)} unique")

    # Write TSV
    with open("data/samples/wikidata_infoboxes.tsv", "w", encoding="utf-8") as f:
        for s, p, o in sorted(unique):
            f.write(f"{s}\t{p}\t{o}\n")

    # Stats
    pred_counts = Counter()
    for s, p, o in unique:
        pred_counts[p] += 1

    print("\nPredicate distribution:")
    for pred, count in pred_counts.most_common(30):
        print(f"  {pred:25s} {count:5d}")

    print(f"\nSaved to data/samples/wikidata_infoboxes.tsv")
