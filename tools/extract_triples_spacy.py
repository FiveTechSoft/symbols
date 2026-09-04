"""
spaCy dependency-based triple extractor for Spanish text.
Handles copular sentences (X es Y), verbal SVO, and prepositional complements.
"""
import spacy

nlp = spacy.load("es_core_news_md")

VERB_MAP = {
    "ser": "ES", "estar": "ES", "existir": "EXISTE",
    "tener": "TIENE", "poseer": "TIENE", "contar": "TIENE",
    "encontrar": "UBICADO_EN", "situar": "UBICADO_EN", "ubicar": "UBICADO_EN",
    "producir": "PRODUCE", "generar": "PRODUCE", "fabricar": "PRODUCE",
    "usar": "USA", "utilizar": "USA", "emplear": "USA", "requerir": "USA", "necesitar": "USA",
    "pertenecer": "PERTENECE_A", "formar": "FORMA_PARTE_DE",
    "incluir": "INCLUYE", "contener": "INCLUYE",
    "causar": "CAUSA", "provocar": "CAUSA",
    "nacer": "NACER_EN", "morir": "MORIR_EN",
    "vivir": "VIVIR_EN", "residir": "VIVIR_EN", "habitar": "VIVIR_EN",
    "programar": "PROGRAMA", "compilar": "COMPILA_CON",
    "estudiar": "ESTUDIA", "trabajar": "TRABAJA_EN",
    "crear": "CREAR", "fundar": "FUNDAR", "establecer": "ESTABLECER",
    "ganar": "GANAR", "perder": "PERDER",
    "escribir": "ESCRIBIR", "publicar": "PUBLICAR",
    "dirigir": "DIRIGIR", "liderar": "DIRIGIR",
    "pilotar": "PILOTAR", "conducir": "CONDUCIR",
    "lanzar": "LANZAR", "desarrollar": "DESARROLLAR",
    "recibir": "RECIBIR", "obtener": "OBTENER",
    "representar": "REPRESENTAR", " CONVERTIR": "CONVERTIR_EN",
}

PREP_MAP = {
    "en": "EN", "de": "DE", "por": "POR", "para": "PARA",
    "con": "CON", "sin": "SIN", "sobre": "SOBRE",
    "entre": "ENTRE", "hasta": "HASTA", "desde": "DESDE",
    "hacia": "HACIA", "como": "COMO", "según": "SEGUN",
    "ante": "ANTE", "bajo": "BAJO", "contra": "CONTRA",
    "durante": "DURANTE", "mediante": "MEDIANTE",
    "tras": "TRAS", "versus": "VS",
}

# Determiners/possessives to skip when extracting noun phrases
SKIP_DEPS = {"det", "poss", "amod"}


def get_clean_np(token):
    """Get a clean noun phrase from a token, skipping determiners/possessives/adjectives."""
    # Walk up to the head noun if this is a modifier
    root = token
    while root.dep_ in SKIP_DEPS and root.head != root:
        root = root.head

    # Collect the noun phrase: start from root, include flat/name children
    parts = []
    for child in root.children:
        if child.dep_ in ("flat", "name", "appos"):
            parts.append(child.text)

    name = " ".join(parts) if parts else root.text
    return name.upper().strip(".,;:!?\"'()[]{}")


def extract_triples_spacy(text):
    """
    Extract SVO triples using spaCy dependency parsing.
    Handles:
      1. Copular: "X es Y" → ROOT=Y, cop=es, nsubj=X
      2. Verbal SVO: "X tiene Y" → VERB=tiene, nsubj=X, obj=Y
      3. Prepositional: "X programa en Y" → VERB=programa, obl=en Y
    """
    doc = nlp(text)
    triples = []

    for sent in doc.sents:
        for token in sent:
            # ========================================
            # Case A: Copular sentence (X es Y)
            # ROOT is the predicate noun/adjective, with cop child
            # ========================================
            if token.dep_ == "ROOT" and token.pos_ in ("NOUN", "PROPN", "ADJ"):
                # Check if there's a copular verb (es, era, fue...)
                has_cop = False
                cop_lemma = "SER"
                for child in token.children:
                    if child.dep_ == "cop":
                        has_cop = True
                        cop_lemma = VERB_MAP.get(child.lemma_.upper(), "ES")
                        break

                if has_cop:
                    # Subject is the nsubj of the ROOT
                    subj = None
                    for child in token.children:
                        if child.dep_ in ("nsubj", "nsubj:pass"):
                            subj = get_clean_np(child)
                            break

                    if subj and subj != token.text.upper():
                        pred = cop_lemma
                        obj = get_clean_np(token)
                        triples.append((subj, pred, obj))

                    # Also check for obl on the copular verb itself
                    # "X está en Y" — en is obl of ROOT? No, check children
                    for child in token.children:
                        if child.dep_ == "obl":
                            obl_text = get_clean_np(child)
                            # Find preposition
                            prep = None
                            for sub in child.children:
                                if sub.dep_ == "case":
                                    prep = PREP_MAP.get(sub.lemma_.lower(), sub.text.upper())
                            if subj and obl_text and prep and subj != obl_text:
                                triples.append((subj, f"{pred}_{prep}", obl_text))

            # ========================================
            # Case B: Verbal SVO
            # token is a VERB with nsubj and obj/attr
            # ========================================
            elif token.pos_ == "VERB" or (token.pos_ == "AUX" and token.dep_ != "cop"):
                subj = None
                obj = None
                attr = None
                obl = None
                obl_prep = None

                for child in token.children:
                    dep = child.dep_

                    if dep in ("nsubj", "nsubj:pass"):
                        subj = get_clean_np(child)
                    elif dep in ("obj", "dobj"):
                        obj = get_clean_np(child)
                    elif dep == "attr":
                        attr = get_clean_np(child)
                    elif dep == "obl":
                        obl = get_clean_np(child)
                        for sub in child.children:
                            if sub.dep_ == "case":
                                obl_prep = PREP_MAP.get(sub.lemma_.lower(), sub.text.upper())

                verb_lemma = token.lemma_.upper()
                pred = VERB_MAP.get(verb_lemma, verb_lemma)

                if subj and attr and subj != attr:
                    triples.append((subj, pred, attr))
                elif subj and obj and subj != obj:
                    triples.append((subj, pred, obj))
                elif subj and obl and obl_prep and subj != obl:
                    triples.append((subj, f"{pred}_{obl_prep}", obl))

    return triples


def extract_batch(texts):
    """Process multiple texts efficiently with nlp.pipe."""
    all_triples = []
    seen = set()

    for doc in nlp.pipe(texts, batch_size=50):
        for sent in doc.sents:
            for token in sent:
                # Copular
                if token.dep_ == "ROOT" and token.pos_ in ("NOUN", "PROPN", "ADJ"):
                    has_cop = False
                    cop_lemma = "SER"
                    for child in token.children:
                        if child.dep_ == "cop":
                            has_cop = True
                            cop_lemma = VERB_MAP.get(child.lemma_.upper(), "ES")
                            break
                    if has_cop:
                        subj = None
                        for child in token.children:
                            if child.dep_ in ("nsubj", "nsubj:pass"):
                                subj = get_clean_np(child)
                                break
                        if subj and subj != token.text.upper():
                            t = (subj, cop_lemma, get_clean_np(token))
                            if t not in seen:
                                seen.add(t)
                                all_triples.append(t)

                # Verbal
                elif token.pos_ == "VERB" or (token.pos_ == "AUX" and token.dep_ != "cop"):
                    subj = obj = attr = obl = obl_prep = None
                    for child in token.children:
                        dep = child.dep_
                        if dep in ("nsubj", "nsubj:pass"):
                            subj = get_clean_np(child)
                        elif dep in ("obj", "dobj"):
                            obj = get_clean_np(child)
                        elif dep == "attr":
                            attr = get_clean_np(child)
                        elif dep == "obl":
                            obl = get_clean_np(child)
                            for sub in child.children:
                                if sub.dep_ == "case":
                                    obl_prep = PREP_MAP.get(sub.lemma_.lower(), sub.text.upper())

                    verb_lemma = token.lemma_.upper()
                    pred = VERB_MAP.get(verb_lemma, verb_lemma)

                    if subj and attr and subj != attr:
                        t = (subj, pred, attr)
                    elif subj and obj and subj != obj:
                        t = (subj, pred, obj)
                    elif subj and obl and obl_prep and subj != obl:
                        t = (subj, f"{pred}_{obl_prep}", obl)
                    else:
                        continue

                    if t not in seen:
                        seen.add(t)
                        all_triples.append(t)

    return all_triples


if __name__ == "__main__":
    test_sentences = [
        "España es un país soberano situado en Europa.",
        "Su capital es Madrid.",
        "Antonio programa en Harbour.",
        "Harbour es un lenguaje xBase.",
        "El compila con hbmk2.",
        "Madrid tiene una población de tres millones.",
        "Barcelona produce vino desde el siglo X.",
        "Google fue fundado por Larry Page y Sergey Brin.",
        "Python se utiliza para aprendizaje automático.",
        "España pertenece a la Unión Europea.",
        "La Habana es la capital de Cuba.",
        "Cuba tiene una población de once millones.",
        "El río Amazonas fluye por Brasil.",
        "Microsoft desarrolla Windows y Office.",
        "Linux fue creado por Linus Torvalds.",
    ]

    print("=== spaCy Triple Extractor Test ===\n")
    for sent in test_sentences:
        triples = extract_triples_spacy(sent)
        print(f"  \"{sent}\"")
        for s, p, o in triples:
            print(f"    {s:35s} --{p:22s}--> {o}")
        if not triples:
            print(f"    (no triples)")
        print()
