"""
C Knowledge Extractor — Parses .h and .c files into semantic triples.
Extracts: functions, parameters, return types, structs, enums, typedefs,
          includes, macros, and relationships between them.
"""
import re
import os
import glob


def extract_from_header(filepath):
    """Extract knowledge triples from a C header file."""
    triples = []
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    filename = os.path.splitext(os.path.basename(filepath))[0].upper()

    # 1. Extract #include dependencies
    for m in re.finditer(r'#include\s+[<"]([^>"]+)[>"]', content):
        dep = m.group(1).replace('.h', '').upper()
        triples.append((filename, 'INCLUYE', dep))

    # 2. Extract typedef struct
    for m in re.finditer(r'typedef\s+struct\s*\{([^}]+)\}\s*(\w+)', content, re.DOTALL):
        struct_name = m.group(2).upper()
        body = m.group(1)

        # Extract fields
        for field in re.finditer(r'(\w+)\s+(\w+)\s*;', body):
            field_type = field.group(1).upper()
            field_name = field.group(2).upper()
            triples.append((struct_name, 'TIENE_CAMPO', field_name))
            triples.append((field_name, 'ES_TIPO', field_type))

    # 3. Extract struct definitions (non-typedef)
    for m in re.finditer(r'struct\s+(\w+)\s*\{([^}]+)\}', content, re.DOTALL):
        struct_name = m.group(1).upper()
        body = m.group(2)
        triples.append((filename, 'DEFINE_STRUCT', struct_name))

        for field in re.finditer(r'(\w+)\s+(\w+)\s*;', body):
            field_type = field.group(1).upper()
            field_name = field.group(2).upper()
            triples.append((struct_name, 'TIENE_CAMPO', field_name))

    # 4. Extract #define constants
    for m in re.finditer(r'#define\s+(\w+)\s+(\d+)', content):
        macro = m.group(1).upper()
        value = m.group(2)
        triples.append((filename, 'DEFINE_CONSTANTE', macro))

    # 5. Extract enum values
    for m in re.finditer(r'enum\s+(\w+)?\s*\{([^}]+)\}', content, re.DOTALL):
        enum_name = m.group(1)
        body = m.group(2)
        for val in re.finditer(r'(\w+)', body):
            v = val.group(1).upper()
            if v not in ('ENUM',) and not v.isdigit():
                if enum_name:
                    triples.append((enum_name.upper(), 'TIENE_VALOR', v))
                else:
                    triples.append((filename, 'DEFINE_ENUM', v))

    # 6. Extract function prototypes
    # Pattern: return_type function_name(param_type param_name, ...)
    func_pattern = re.compile(
        r'(?:(?:static|extern|inline|const)+\s+)*'
        r'(\w[\w\s\*]*?)\s+'  # return type
        r'(\w+)\s*'           # function name
        r'\(([^)]*)\)',       # parameters
        re.MULTILINE
    )

    for m in func_pattern.finditer(content):
        ret_type = m.group(1).strip()
        func_name = m.group(2).upper()
        params_raw = m.group(3).strip()

        # Skip macros, preprocessor, and non-function matches
        if func_name.startswith('_') or ret_type.startswith('#'):
            continue
        if func_name in ('IF', 'WHILE', 'FOR', 'SWITCH', 'RETURN', 'SIZEOF'):
            continue

        # Clean return type
        ret_clean = re.sub(r'\s+', '_', ret_type).upper().strip('*')

        triples.append((func_name, 'RETORNA', ret_clean))
        triples.append((filename, 'DECLARA_FUNCION', func_name))

        # Extract parameters
        if params_raw and params_raw != 'void':
            for param in params_raw.split(','):
                param = param.strip()
                parts = param.split()
                if len(parts) >= 2:
                    p_type = parts[0].upper()
                    p_name = parts[-1].upper().strip('*')
                    triples.append((func_name, 'RECIBE_PARAMETRO', p_name))
                    triples.append((p_name, 'ES_TIPO', p_type))

    return triples


def extract_from_source(filepath):
    """Extract knowledge triples from a C source file."""
    triples = []
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    filename = os.path.splitext(os.path.basename(filepath))[0].upper()

    # 1. Extract function definitions (with body)
    func_def_pattern = re.compile(
        r'(?:(?:static|extern|inline|const)+\s+)*'
        r'(\w[\w\s\*]*?)\s+'
        r'(\w+)\s*'
        r'\(([^)]*)\)\s*\{',
        re.MULTILINE
    )

    for m in func_def_pattern.finditer(content):
        ret_type = m.group(1).strip()
        func_name = m.group(2).upper()
        params_raw = m.group(3).strip()

        if func_name.startswith('_') or ret_type.startswith('#'):
            continue
        if func_name in ('IF', 'WHILE', 'FOR', 'SWITCH', 'RETURN', 'SIZEOF'):
            continue

        ret_clean = re.sub(r'\s+', '_', ret_type).upper().strip('*')
        triples.append((func_name, 'RETORNA', ret_clean))
        triples.append((filename, 'IMPLEMENTA', func_name))

    # 2. Extract function calls within the file
    # Find calls: word( where word is a known function pattern
    for m in re.finditer(r'(\w+)\s*\(', content):
        caller = m.group(1).upper()
        if caller in ('IF', 'WHILE', 'FOR', 'SWITCH', 'RETURN', 'SIZEOF',
                       'PRINTF', 'SCANF', 'MALLOC', 'FREE', 'STRLEN',
                       'MEMCPY', 'FOPEN', 'FCLOSE'):
            continue

    # 3. Extract string literals as potential knowledge
    for m in re.finditer(r'"([^"]{5,80})"', content):
        text = m.group(1)
        # Skip format strings and comments
        if '%' not in text and not text.startswith('===') and not text.startswith('---'):
            # Could be used as documentation
            pass

    return triples


def extract_from_directory(dirpath, extensions=('.h', '.c')):
    """Extract triples from all C files in a directory."""
    all_triples = []
    seen = set()

    for ext in extensions:
        pattern = os.path.join(dirpath, '**', f'*{ext}')
        for filepath in glob.glob(pattern, recursive=True):
            if ext == '.h':
                triples = extract_from_header(filepath)
            else:
                triples = extract_from_source(filepath)

            for t in triples:
                if t not in seen:
                    seen.add(t)
                    all_triples.append(t)

    return all_triples


def triples_to_tsv(triples, filepath):
    """Write triples to TSV file."""
    with open(filepath, 'w', encoding='utf-8') as f:
        for s, p, o in sorted(triples):
            f.write(f'{s}\t{p}\t{o}\n')
    return len(triples)


# ============================================
# C Standard Library Knowledge Base
# ============================================

C_STDLIB_TRIPLES = [
    # Memory
    ('MALLOC', 'RETORNA', 'PUNTERO_VOID'),
    ('MALLOC', 'RECIBE_PARAMETRO', 'TAMANO'),
    ('MALLOC', 'NECESITA', 'SIZEOF'),
    ('CALLOC', 'RETORNA', 'PUNTERO_VOID'),
    ('CALLOC', 'INICIALIZA', 'CERO'),
    ('REALLOC', 'RETORNA', 'PUNTERO_VOID'),
    ('REALLOC', 'CAMBIA_TAMANO', 'BLOQUE_MEMORIA'),
    ('FREE', 'LIBERA', 'MEMORIA_DINAMICA'),
    ('FREE', 'RECIBE_PARAMETRO', 'PUNTERO'),

    # String
    ('STRCPY', 'COPIA', 'CADENA'),
    ('STRCPY', 'RECIBE_PARAMETRO', 'DESTINO'),
    ('STRCPY', 'RECIBE_PARAMETRO', 'ORIGEN'),
    ('STRNCPY', 'COPIA', 'CADENA'),
    ('STRNCPY', 'LIMITA', 'N_CARACTERES'),
    ('STRLEN', 'RETORNA', 'ENTERO'),
    ('STRLEN', 'CUENTA', 'LONGITUD_CADENA'),
    ('STRCMP', 'COMPARA', 'CADENAS'),
    ('STRCAT', 'CONCATENA', 'CADENAS'),
    ('STRCHR', 'BUSCA', 'CARACTER_EN_CADENA'),
    ('STRSTR', 'BUSCA', 'SUBCADENA'),

    # I/O
    ('PRINTF', 'IMPRIME', 'FORMATO'),
    ('PRINTF', 'ESCRIBE', 'STDOUT'),
    ('FPRINTF', 'ESCRIBE', 'ARCHIVO'),
    ('SPRINTF', 'ESCRIBE', 'BUFFER'),
    ('SNPRINTF', 'ESCRIBE', 'BUFFER'),
    ('SNPRINTF', 'LIMITA', 'N_CARACTERES'),
    ('SCANF', 'LEE', 'ENTRADA_ESTANDAR'),
    ('FSCANF', 'LEE', 'ARCHIVO'),
    ('FGETS', 'LEE', 'LINEA_DE_ARCHIVO'),
    ('FOPEN', 'ABRE', 'ARCHIVO'),
    ('FOPEN', 'RETORNA', 'PUNTERO_FILE'),
    ('FCLOSE', 'CIERRA', 'ARCHIVO'),
    ('FREAD', 'LEE', 'DATOS_BINARIOS'),
    ('FWRITE', 'ESCRIBE', 'DATOS_BINARIOS'),

    # Math
    ('ABS', 'RETORNA', 'VALOR_ABSOLUTO'),
    ('FABS', 'RETORNA', 'VALOR_ABSOLUTO'),
    ('SQRT', 'RETORNA', 'RAIZ_CUADRADA'),
    ('POW', 'RETORNA', 'POTENCIA'),
    ('CEIL', 'RETORNA', 'REDONDEO_ARRIBA'),
    ('FLOOR', 'RETORNA', 'REDONDEO_ABAJO'),
    ('ROUND', 'RETORNA', 'REDONDEO'),
    ('LOG', 'RETORNA', 'LOGARITMO'),
    ('LOG10', 'RETORNA', 'LOGARITMO_BASE_10'),
    ('SIN', 'RETORNA', 'SENO'),
    ('COS', 'RETORNA', 'COSENO'),
    ('TAN', 'RETORNA', 'TANGENTE'),

    # Conversion
    ('ATOI', 'CONVIERTE', 'CADENA_A_ENTERO'),
    ('ATOF', 'CONVIERTE', 'CADENA_A_FLOAT'),
    ('STRTOI', 'CONVIERTE', 'CADENA_A_ENTERO'),
    ('STRTOF', 'CONVIERTE', 'CADENA_A_FLOAT'),

    # Type sizes
    ('SIZEOF', 'RETORNA', 'TAMANO_BYTES'),
    ('SIZEOF', 'MIDE', 'TIPO_DATO'),
    ('SIZEOF', 'MIDE', 'STRUCT'),

    # Control flow (as knowledge)
    ('IF', 'EVALUA', 'CONDICION'),
    ('WHILE', 'REPITE', 'BUCLE'),
    ('FOR', 'ITERA', 'CONTADOR'),
    ('SWITCH', 'SELECCIONA', 'CASO'),
    ('RETURN', 'DEVUELVE', 'VALOR'),

    # Pointers
    ('PUNTERO', 'APUNTA_A', 'DIRECCION_MEMORIA'),
    ('PUNTERO', 'DEREFERENCIA', '*'),
    ('REFERENCIA', 'OBTIENE', 'DIRECCION'),
    ('ARRAY', 'CONTIENE', 'ELEMENTOS'),
]

# C language concepts
C_CONCEPT_TRIPLES = [
    # Types
    ('INT', 'ES_TIPO', 'ENTERO'),
    ('FLOAT', 'ES_TIPO', 'DECIMAL'),
    ('DOUBLE', 'ES_TIPO', 'DECIMAL_DOBLE'),
    ('CHAR', 'ES_TIPO', 'CARACTER'),
    ('VOID', 'ES_TIPO', 'SIN_VALOR'),
    ('BOOL', 'ES_TIPO', 'BOOLEANO'),
    ('SIZE_T', 'ES_TIPO', 'TAMANO'),
    ('UINT32_T', 'ES_TIPO', 'ENTERO_SIN_SIGNO'),
    ('INT32_T', 'ES_TIPO', 'ENTERO_CON_SIGNO'),
    ('UINT64_T', 'ES_TIPO', 'ENTERO_GRANDE'),

    # Concepts
    ('VARIABLE', 'ALMACENA', 'DATO'),
    ('CONSTANTE', 'NO_CAMBIA', 'VALOR'),
    ('PUNTERO', 'ALMACENA', 'DIRECCION'),
    ('ARRAY', 'ALMACENA', 'COLECCION'),
    ('STRUCT', 'AGRUPA', 'CAMPOS'),
    ('ENUM', 'DEFINE', 'CONSTANTES_NOMBRE'),
    ('UNION', 'SOBREPONE', 'CAMPOS'),
    ('TYPEDEF', 'ALIAS', 'TIPO'),

    # Patterns
    ('MALLOC', 'REQUIERE', 'FREE'),
    ('FOPEN', 'REQUIERE', 'FCLOSE'),
    ('BUCLE', 'EVITA', 'INFINITO'),
    ('PUNTERO_NULO', 'CAUSA', 'SEGFAULT'),
    ('DESABORDAMIENTO', 'CAUSA', 'BUG'),
    ('MEMORY_LEAK', 'CAUSA', 'AGOTAMIENTO_MEMORIA'),
]


if __name__ == "__main__":
    import sys

    print("=== C Knowledge Extractor ===\n")

    # 1. Extract from our own codebase
    print("1. Extracting from codebase (include/ and src/)...")
    codebase_triples = extract_from_directory('.', extensions=('.h',))
    # Also parse src/*.c for function implementations
    src_triples = extract_from_directory('.', extensions=('.c',))
    codebase_triples.extend(src_triples)
    print(f"   Codebase: {len(codebase_triples)} triples\n")

    # 2. Add C standard library knowledge
    print("2. Adding C standard library knowledge...")
    stdlib_triples = C_STDLIB_TRIPLES
    print(f"   stdlib: {len(stdlib_triples)} triples\n")

    # 3. Add C language concepts
    print("3. Adding C language concepts...")
    concept_triples = C_CONCEPT_TRIPLES
    print(f"   concepts: {len(concept_triples)} triples\n")

    # 4. Combine all
    all_triples = []
    seen = set()
    for t in codebase_triples + stdlib_triples + concept_triples:
        if t not in seen:
            seen.add(t)
            all_triples.append(t)

    print(f"TOTAL: {len(all_triples)} unique triples\n")

    # 5. Write TSV
    os.makedirs('data/samples', exist_ok=True)
    outfile = 'data/samples/c_knowledge.tsv'
    triples_to_tsv(all_triples, outfile)
    print(f"Saved to {outfile}\n")

    # 6. Show samples
    from collections import Counter
    pred_counts = Counter()
    for s, p, o in all_triples:
        pred_counts[p] += 1

    print("Predicate distribution:")
    for pred, count in pred_counts.most_common(20):
        print(f"  {pred:25s} {count:5d}")

    print(f"\nSample triples:")
    for s, p, o in all_triples[:15]:
        print(f"  {s:30s} {p:25s} {o}")
