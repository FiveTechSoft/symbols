"""
Fetch Psalms from a free Bible API and extract semantic triples.
"""
import requests
import json
import re
import os

# Bible SuperSearch API (free, no auth)
API_URL = "https://bible-api.com/"

# Alternative: use a local JSON Bible if available
LOCAL_BIBLE = "data/bible/rvr1960.json"


def fetch_psalms_online():
    """Fetch Psalms 1-150 from Bible API."""
    psalms = []
    for ch in range(1, 151):
        try:
            r = requests.get(f"{API_URL}Salmos+{ch}?translation=rvr1960",
                             timeout=15, headers={'User-Agent': 'SymbolicLLM/1.0'})
            if r.status_code == 200:
                data = r.json()
                text = data.get('text', '')
                psalms.append({
                    'chapter': ch,
                    'text': text,
                    'verses': len(text.split('\n')) if text else 0
                })
                if ch % 20 == 0:
                    print(f"  Fetched Psalm {ch}/150")
            else:
                print(f"  Warning: Psalm {ch} returned {r.status_code}")
        except Exception as e:
            print(f"  Error on Psalm {ch}: {e}")
    return psalms


def fetch_psalms_biblegateway():
    """Fetch from BibleGateway (public domain KJV text)."""
    psalms = []
    session = requests.Session()
    session.headers.update({'User-Agent': 'SymbolicLLM/1.0'})

    for ch in range(1, 151):
        try:
            url = f"https://www.biblegateway.com/passage/?search=Psalm+{ch}&version=KJV"
            r = session.get(url, timeout=15)
            if r.status_code == 200:
                # Extract text from HTML
                text = r.text
                # Find the passage text
                m = re.search(r'<div class="passage-text">(.*?)</div>', text, re.DOTALL)
                if m:
                    passage = m.group(1)
                    # Clean HTML
                    passage = re.sub(r'<[^>]+>', ' ', passage)
                    passage = re.sub(r'\s+', ' ', passage).strip()
                    psalms.append({
                        'chapter': ch,
                        'text': passage,
                        'verses': len(re.findall(r'\d+', passage[:100]))
                    })
        except:
            pass

        if ch % 30 == 0:
            print(f"  Fetched Psalm {ch}/150")

    return psalms


def generate_psalms_knowledge():
    """
    Generate a knowledge base of Psalms semantic triples.
    Based on well-known Psalm themes and content.
    """
    triples = set()

    # Core entities in Psalms
    entities = {
        'DIOS': ['SENOR', 'ALTISIMO', 'CREADOR', 'SALVADOR', 'REY', 'JUEZ',
                  'ROCA', 'FORTALEZA', 'ESCUDO', 'LUZ', 'PASTOR'],
        'DAVID': ['REY', 'SALMISTA', 'SIervo_DE_DIOS', 'PASTOR', 'GUERRERO'],
        'ISRAEL': ['PUEBLO_DE_DIOS', 'NACION_ELEGIDA', 'CASA_DE_JACOB'],
        'SION': ['CIUDAD_SANTA', 'JERUSALEN', 'MONTE_SANTO', 'MORADA_DE_DIOS'],
        'ENEMIGOS': ['OPRESORES', 'IMPÍOS', 'PERSEGUIDORES', 'NACIONES'],
        'JUSTOS': ['PIADOSOS', 'RECTOS', 'FIELES', 'Siervos_DEL_SENOR'],
        'MALVADOS': ['PECADORES', 'SOBERBIOS', 'IMPÍOS', 'MENTIROSOS'],
    }

    # Predicate mappings for Psalm vocabulary
    verb_predicates = {
        # Praise & worship
        'alabar': 'ALABANZA', 'alabado': 'ALABANZA', 'alabaré': 'ALABANZA',
        'cantar': 'CANTA_A', 'cantaré': 'CANTA_A', 'cantaremos': 'CANTA_A',
        'glorificar': 'GLORIFICA', 'glorificaré': 'GLORIFICA',
        'adorar': 'ADORA', 'adoraré': 'ADORA',
        'bendecir': 'BENDICE', 'bendeciré': 'BENDICE',
        'ensalzar': 'ENSALZA', 'ensalzaré': 'ENSALZA',
        ' magnificar': 'MAGNIFICA',

        # Protection & salvation
        'proteger': 'PROTEGE', 'protegerá': 'PROTEGE', 'protegido': 'PROTEGE',
        'salvar': 'SALVA', 'salvará': 'SALVA', 'salvado': 'SALVA',
        'rescatar': 'RESCATA', 'rescatará': 'RESCATA',
        'librar': 'LIBRA', 'librará': 'LIBRA', 'librado': 'LIBRA',
        'escudar': 'ESCUDAR', 'escudará': 'ESCUDAR',
        'amparar': 'AMPARA', 'amparará': 'AMPARA',
        'refugio': 'REFUGIO', 'refugiará': 'REFUGIO',
        'defender': 'DEFIENDE', 'defenderá': 'DEFIENDE',

        # Hearing & answering
        'oir': 'ESCUCHA', 'oído': 'ESCUCHA', 'oirá': 'ESCUCHA',
        'escuchar': 'ESCUCHA', 'escuchará': 'ESCUCHA',
        'responder': 'RESPONDE', 'responderá': 'RESPONDE',

        # Creation & sovereignty
        'crear': 'CREA', 'creó': 'CREA', 'creado': 'CREA',
        'gobernar': 'GOBIERNA', 'reinar': 'REINA',
        'juzgar': 'JUZGA', 'juzgará': 'JUZGA',

        # Comfort & healing
        'consolar': 'CONSOLA', 'consolará': 'CONSOLA',
        'sanar': 'SANA', 'sanará': 'SANA',
        'restaurar': 'RESTAURA', 'restaurará': 'RESTAURA',

        # Trust & fear
        'temer': 'TEMOR_DE', 'temido': 'TEMOR_DE',
        'confiar': 'CONFIANZA_EN', 'confiará': 'CONFIANZA_EN',
        'esperar': 'ESPERANZA_EN', 'esperará': 'ESPERANZA_EN',
        'aferrarse': 'SE_AFERRA_A', 'aferrará': 'SE_AFERRA_A',

        # Destruction of enemies
        'destruir': 'DESTRUYE', 'destruirá': 'DESTRUYE',
        'exterminar': 'EXTERMINA', 'exterminará': 'EXTERMINA',
        'caerán': 'CAEN', 'caerá': 'CAEN',
        'perecer': 'PERECE', 'perecerán': 'PERECE',

        # Emotional states
        'llorar': 'LLORA', 'lloraré': 'LLORA',
        'alegrarse': 'SE_ALEGRA', 'alegrará': 'SE_ALEGRA',
        'temblar': 'TEMBLA', 'temblarán': 'TEMBLA',
        'callar': 'CALLA', 'callará': 'CALLA',
        'clamar': 'CLAMA_A', 'clamaré': 'CLAMA_A',
        'gritar': 'GRITA_A', 'gritaré': 'GRITA_A',
    }

    # Psalm-specific relationship triples
    psalm_relationships = [
        # God's attributes
        ('DIOS', 'ES', 'MISERICORDIOSO'),
        ('DIOS', 'ES', 'JUSTO'),
        ('DIOS', 'ES', 'SANTO'),
        ('DIOS', 'ES', 'FIEL'),
        ('DIOS', 'ES', 'ETOERNO'),
        ('DIOS', 'ES', 'PODEROSO'),
        ('DIOS', 'ES', 'SABIO'),
        ('DIOS', 'ES', 'BONDADOSO'),
        ('DIOS', 'TIENE', 'MISERICORDIA'),
        ('DIOS', 'TIENE', 'GRACIA'),
        ('DIOS', 'TIENE', 'FURIA'),
        ('DIOS', 'TIENE', 'CELO'),

        # God's actions
        ('DIOS', 'PROTEGE', 'JUSTOS'),
        ('DIOS', 'SALVA', 'ISRAEL'),
        ('DIOS', 'ESCUCHA', 'ORACIONES'),
        ('DIOS', 'RESPONDE', 'CLAMORES'),
        ('DIOS', 'CREA', 'CIELOS'),
        ('DIOS', 'CREA', 'TIERRA'),
        ('DIOS', 'CREA', 'HOMBRES'),
        ('DIOS', 'JUZGA', 'MALVADOS'),
        ('DIOS', 'DESTRUYE', 'ENEMIGOS'),
        ('DIOS', 'GOBIERNA', 'NACIONES'),
        ('DIOS', 'REINA', 'SOBRE_TODAS_LAS_OBRAS'),
        ('DIOS', 'CAMINA', 'ENTRE_EL_PUEBLO'),
        ('DIOS', 'HABITA', 'SION'),
        ('DIOS', 'MORADA', 'JERUSALEN'),

        # David's relationship with God
        ('DAVID', 'ALABANZA', 'DIOS'),
        ('DAVID', 'CANTA_A', 'DIOS'),
        ('DAVID', 'CLAMA_A', 'DIOS'),
        ('DAVID', 'CONFIANZA_EN', 'DIOS'),
        ('DAVID', 'ESPERANZA_EN', 'DIOS'),
        ('DAVID', 'SE_AFERRA_A', 'DIOS'),
        ('DAVID', 'TEMPOR_DE', 'DIOS'),
        ('DAVID', 'BUSCA_A', 'DIOS'),
        ('DAVID', 'AMA_A', 'DIOS'),
        ('DAVID', 'NARRA', 'OBRAS_DE_DIOS'),

        # Israel's relationship
        ('ISRAEL', 'ALABANZA', 'DIOS'),
        ('ISRAEL', 'CANTA_A', 'DIOS'),
        ('ISRAEL', 'CONFÍA_EN', 'DIOS'),
        ('ISRAEL', 'TEMPOR_DE', 'DIOS'),
        ('ISRAEL', 'PUEBLO_DE', 'DIOS'),
        ('ISRAEL', 'RECIBE', 'PROMESAS'),

        # Zion/Jerusalem
        ('SION', 'MORADA_DE', 'DIOS'),
        ('SION', 'CIUDAD_DE', 'DIOS'),
        ('SION', 'TRONO_DE', 'DIOS'),
        ('JERUSALEN', 'ES', 'CIUDAD_SANTA'),
        ('JERUSALEN', 'HABITA_EN', 'DIOS'),

        # Enemies
        ('ENEMIGOS', 'RODEAN', 'JUSTOS'),
        ('ENEMIGOS', 'PERSEGUIR', 'ISRAEL'),
        ('ENEMIGOS', 'BUSCA', 'DESTRUIR'),
        ('ENEMIGOS', 'HABLAN', 'MENTIRAS'),
        ('ENEMIGOS', 'PLANIFICAN', 'MAL'),
        ('ENEMIGOS', 'CAEN', 'DESPUÉS_DE_DIOS'),

        # Just/Righteous
        ('JUSTOS', 'ALABAN', 'DIOS'),
        ('JUSTOS', 'OBEDECEN', 'DIOS'),
        ('JUSTOS', 'CONFÍA_EN', 'DIOS'),
        ('JUSTOS', 'SON_PROTEGIDOS', 'POR_DIOS'),
        ('JUSTOS', 'CANTAN', 'ALABANZAS'),
        ('JUSTOS', 'JUZGAN', 'CON_JUSTICIA'),

        # Malvados/Wicked
        ('MALVADOS', 'RECHAZAN', 'DIOS'),
        ('MALVADOS', 'PERSECUTAN', 'JUSTOS'),
        ('MALVADOS', 'HABLAN', 'FALSO'),
        ('MALVADOS', 'SON_JUZGADOS', 'POR_DIOS'),
        ('MALVADOS', 'CAEN', 'EN_SU_PROPIA_RED'),
        ('MALVADOS', 'PERECEN', 'ETERNAMENTE'),

        # Thematic relationships
        ('SALMO', 'ES', 'CANTICO'),
        ('SALMO', 'CONTIENE', 'ALABANZA'),
        ('SALMO', 'CONTIENE', 'LAMENTO'),
        ('SALMO', 'CONTIENE', 'PETICION'),
        ('SALMO', 'CONTIENE', 'ACCION_DE_GRACIAS'),
        ('SALMO', 'CONTIENE', 'PROFECÍA'),
        ('SALMO', 'ENSINA', 'TEMOR_DE_DIOS'),
        ('SALMO', 'ENSINA', 'CONFIANZA'),
        ('SALMO', 'ENSINA', 'JUSTICIA'),
        ('SALMO', 'CONSUELA', 'AFLIGIDOS'),
        ('SALMO', 'FORTALECE', 'DESANIMADOS'),

        # Psalm categories
        ('SALMO_DE_ALABANZA', 'ES', 'SALMO'),
        ('SALMO_DE_LAMENTO', 'ES', 'SALMO'),
        ('SALMO_DE_ACCION_DE_GRACIAS', 'ES', 'SALMO'),
        ('SALMO_MESIÁNICO', 'ES', 'SALMO'),
        ('SALMO_DE_CONFIANZA', 'ES', 'SALMO'),
        ('SALMO_DE_SABIDURÍA', 'ES', 'SALMO'),
        ('SALMO_REAL', 'ES', 'SALMO'),

        # Key Psalm verses as knowledge
        ('SALMO_23', 'Dice', 'EL_SENOR_ES_MI_PASTOR'),
        ('SALMO_23', 'Dice', 'NADA_ME_FALTARA'),
        ('SALMO_23', 'Dice', 'EN_PASTOS_VERDES_ME_HARA_YACER'),
        ('SALMO_23', 'Dice', 'JUNTO_A_AGUAS_DE_REPOSO_ME_PASTOREARA'),
        ('SALMO_23', 'Dice', 'MI_ALMA_VOLVERA_A_RESTAURAR'),
        ('SALMO_23', 'Dice', 'CAMINARE_EN_LA_SENDA_DE_LA_JUSTICIA'),
        ('SALMO_23', 'Dice', 'AUNQUE_CAMINE_EN_VALLE_DE_SOMBRA_DE_MUERTE'),
        ('SALMO_23', 'Dice', 'NO_TEMERE_NADA'),
        ('SALMO_23', 'Dice', 'TU_VARA_Y TU_CAYADO_ME_SOSTENDRAN'),
        ('SALMO_23', 'Dice', 'PREPARAS_MESA_DELANTE_DE_MI'),
        ('SALMO_23', 'Dice', 'MI_CALIZ_SE_DESBOARA'),
        ('SALMO_23', 'Dice', 'BONDAD_Y MISERICORDIA_ME_SEGUIRAN'),
        ('SALMO_23', 'Dice', 'VOLVERE_A_LA_CASA_DEL_SENOR'),

        ('SALMO_91', 'Dice', 'EL_QUE_HABITA_EN_EL_REPOSO_DEL_ALTISIMO'),
        ('SALMO_91', 'Dice', 'MORARA_BAJO_LA_SOMBRA_DEL_TODOPOTENTE'),
        ('SALMO_91', 'Dice', 'DIRA_AL_SENOR_Refugio_MIO'),
        ('SALMO_91', 'Dice', 'MI_FORTALEZA_Y_MI_CIUDAD_SALVADORA'),
        ('SALMO_91', 'Dice', 'NO_TEMERAS_LOS_PESTES_DE_LA_NOCHE'),
        ('SALMO_91', 'Dice', 'MIL_CAERAN_A_TU_LADO'),
        ('SALMO_91', 'Dice', 'DIEZ_MIL_A_TU_DIESTRA'),
        ('SALMO_91', 'Dice', 'NO_SE_ACERCARA_A_TI'),

        ('SALMO_119', 'Dice', 'BENDECIDOS_LOS_INTEGROS_EN_SU_CAMINO'),
        ('SALMO_119', 'Dice', 'LOS_QUE_OBEDIENCIA_LA_LEY_DE_DIOS'),

        ('SALMO_1', 'Dice', 'BENDECIDOS_EL_VARON_QUE_NO_ANDA_EN_CONSEJO_DE_MALVADOS'),
        ('SALMO_1', 'Dice', 'SU_DELEITE_ES_LA_LEY_DEL_SENOR'),
        ('SALMO_1', 'Dice', 'COMO_ARBOL_JUNTO_A_MANANTIALES'),
        ('SALMO_1', 'Dice', 'DARA_SU_FRUPO_A_SU_TIEMPO'),

        ('SALMO_150', 'Dice', 'ALABAD_A_DIOS_EN_SU_SANCTUARIO'),
        ('SALMO_150', 'Dice', 'ALABADLO_CON_CAMPAÑA_Y_TROMPETA'),
        ('SALMO_150', 'Dice', 'TODO_LO QUE_Tiene_Alma_LOUVE_AL_SENOR'),
    ]

    for t in psalm_relationships:
        triples.add(t)

    return list(triples)


if __name__ == "__main__":
    print("=== Psalms Knowledge Generator ===\n")

    # Try online first
    print("1. Generating Psalms knowledge base...")
    triples = generate_psalms_knowledge()
    print(f"   Generated {len(triples)} triples\n")

    # Write TSV
    os.makedirs('data/samples', exist_ok=True)
    outfile = 'data/samples/psalms_knowledge.tsv'
    with open(outfile, 'w', encoding='utf-8') as f:
        for s, p, o in sorted(triples):
            f.write(f'{s}\t{p}\t{o}\n')

    print(f"2. Saved to {outfile}")

    # Stats
    from collections import Counter
    entities = Counter()
    predicates = Counter()
    for s, p, o in triples:
        entities[s] += 1
        predicates[p] += 1

    print(f"\n3. Top entities:")
    for e, c in entities.most_common(15):
        print(f"   {e:25s} {c:3d}")

    print(f"\n4. Top predicates:")
    for p, c in predicates.most_common(15):
        print(f"   {p:25s} {c:3d}")

    print(f"\n5. Sample triples:")
    for s, p, o in triples[:20]:
        print(f"   {s:25s} {p:25s} {o}")
