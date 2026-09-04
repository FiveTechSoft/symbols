# fetch_wikidata_triples.py
from SPARQLWrapper import SPARQLWrapper, JSON
import csv
import os

sparql = SPARQLWrapper("https://query.wikidata.org/sparql")
sparql.addCustomHttpHeader("User-Agent", "SymbolicLLM/1.0 (antonio@example.org)")

# Ejemplo: Extraer países, capitales, continentes, monedas, lenguajes, profesiones
query = """
SELECT ?subjLabel ?propLabel ?objLabel WHERE {
  {
    ?subj wdt:P31/wdt:P279* wd:Q6256 ;      # Países
          ?prop ?obj .
    VALUES ?prop { wdt:P36 wdt:P30 wdt:P37 wdt:P38 } # capital, continente, idioma, moneda
  } UNION {
    ?subj wdt:P31/wdt:P279* wd:Q5 ;         # Personas notables
          wdt:P106 ?obj .                   # ocupación/profesión
  } UNION {
    ?subj wdt:P31/wdt:P279* wd:Q16521 ;     # Taxones / seres vivos
          wdt:P171 ?obj .                   # taxón superior (ES / pertenece a)
  }
  SERVICE wikibase:label { bd:serviceParam wikibase:language "es". }
}
LIMIT 50000
"""

sparql.setQuery(query)
sparql.setReturnFormat(JSON)
results = sparql.query().convert()

os.makedirs("data/samples", exist_ok=True)

with open("data/samples/wikidata_50k.tsv", "w", encoding="utf-8", newline="") as f:
    writer = csv.writer(f, delimiter="\t")
    count = 0
    for row in results["results"]["bindings"]:
        s = row.get("subjLabel", {}).get("value", "").upper().replace(" ", "_")
        p = row.get("propLabel", {}).get("value", "").upper().replace(" ", "_")
        o = row.get("objLabel", {}).get("value", "").upper().replace(" ", "_")
        
        # Filtrar URIs sin etiqueta traducida (Q12345)
        if s and p and o and not s.startswith("Q") and not o.startswith("Q"):
            writer.writerow([s, p, o])
            count += 1

print(f"Written {count} triples to data/samples/wikidata_50k.tsv")
