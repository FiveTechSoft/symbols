/* Generic add-field-and-aggregate operators on unrelated layouts. */
#include "c_edit_ops.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static int g_pass, g_fail;
#define CHECK(c, m) do { if (c) g_pass++; else { g_fail++; printf("FAIL: %s\n", m); } } while (0)
static const char *ext1_paths[] = { "inventario.h", "inventario.c", "main.c" };
static const char *ext1_srcs[] = {
        "#ifndef INVENTARIO_H\n"
        "#define INVENTARIO_H\n"
        "\n"
        "typedef struct {\n"
        "    const char *nombre;\n"
        "    double precio;\n"
        "} Producto;\n"
        "\n"
        "void inventario_agregar(Producto p);\n"
        "void inventario_mostrar(void);\n"
        "\n"
        "#endif\n"
        "",
        "#include <stdio.h>\n"
        "#include \"inventario.h\"\n"
        "\n"
        "#define MAX_PRODUCTOS 16\n"
        "\n"
        "static Producto items[MAX_PRODUCTOS];\n"
        "static int n = 0;\n"
        "\n"
        "void inventario_agregar(Producto p)\n"
        "{\n"
        "    if (n < MAX_PRODUCTOS)\n"
        "        items[n++] = p;\n"
        "}\n"
        "\n"
        "void inventario_mostrar(void)\n"
        "{\n"
        "    for (int i = 0; i < n; i++)\n"
        "        printf(\"%s %.2f\\n\", items[i].nombre, items[i].precio);\n"
        "}\n"
        "",
        "#include \"inventario.h\"\n"
        "\n"
        "int main(void)\n"
        "{\n"
        "    inventario_agregar((Producto){\"teclado\", 25.0});\n"
        "    inventario_agregar((Producto){\"ratón\", 12.5});\n"
        "    inventario_agregar((Producto){\"monitor\", 180.0});\n"
        "    inventario_mostrar();\n"
        "    return 0;\n"
        "}\n"
        "" };
static const char *ext2_paths[] = { "store.h", "store.c", "main.c" };
static const char *ext2_srcs[] = {
        "#ifndef STORE_H\n"
        "#define STORE_H\n"
        "struct Item {\n"
        "	char name[32];\n"
        "	float price;\n"
        "};\n"
        "void store_print(void);\n"
        "#endif\n"
        "",
        "#include <stdio.h>\n"
        "#include \"store.h\"\n"
        "struct Item catalog[] = {\n"
        "	{\"keyboard\", 30.0f},\n"
        "	{\"mouse\", 12.5f},\n"
        "	{\"monitor\", 199.0f},\n"
        "};\n"
        "int catalog_len = 3;\n"
        "void store_print(void)\n"
        "{\n"
        "	for (int i = 0; i < catalog_len; i++)\n"
        "		printf(\"%s %.2f\\n\", catalog[i].name, catalog[i].price);\n"
        "}\n"
        "",
        "#include \"store.h\"\n"
        "int main(void)\n"
        "{\n"
        "	store_print();\n"
        "	return 0;\n"
        "}\n"
        "" };
static const char *l1_paths[] = { "app.c" };
static const char *l1_srcs[] = {
        "#include <stdio.h>\n"
        "typedef struct {\n"
        "    const char *label;\n"
        "    int id;\n"
        "    double cost;\n"
        "} Part;\n"
        "static Part parts[] = {\n"
        "    { \"bolt\", 1, 0.25 },\n"
        "    { \"nut\", 2, 0.10 },\n"
        "};\n"
        "#define NPARTS 2\n"
        "int main(void)\n"
        "{\n"
        "    for (int i = 0; i < NPARTS; i++) printf(\"%s %.2f\\n\", parts[i].label, parts[i].cost);\n"
        "    return 0;\n"
        "}\n"
        "" };
static const char *l2_paths[] = { "fruta.h", "fruta.c", "main.c" };
static const char *l2_srcs[] = {
        "#ifndef FRUTA_H\n"
        "#define FRUTA_H\n"
        "struct fruta { char nombre[20]; float precio; };\n"
        "extern struct fruta cesta[];\n"
        "extern const int ncesta;\n"
        "void listar(void);\n"
        "#endif\n"
        "",
        "#include <stdio.h>\n"
        "#include \"fruta.h\"\n"
        "struct fruta cesta[] = { {\"manzana\", 0.5f}, {\"pera\", 0.75f}, {\"kiwi\", 0.3f}, {\"uva\", 2.0f} };\n"
        "const int ncesta = 4;\n"
        "void listar(void) { for (int i = 0; i < ncesta; i++) printf(\"%s\\n\", cesta[i].nombre); }\n"
        "",
        "#include \"fruta.h\"\n"
        "int main(void) { listar(); return 0; }\n"
        "" };

static CeoPlan P; static char O[10][8192];
static void Expect(const char *label, const char *issue, const char **paths, const char **srcs, int n, double total, const char *must)
{
    int h = CeoPlanAddFieldAndTotal(issue, paths, srcs, n, &P);
    char m[256]; snprintf(m, sizeof m, "%s: plan (%s)", label, h ? "ok" : P.reason);
    CHECK(h > 0, m);
    if (!h) return;
    snprintf(m, sizeof m, "%s: expected total %.2f got %.2f", label, total, P.expected_total);
    CHECK(fabs(P.expected_total - total) < 0.005, m);
    snprintf(m, sizeof m, "%s: plan applies", label);
    CHECK(CeoApplyPlan(&P, srcs, n, O, sizeof O[0]), m);
    int found = 0; for (int f = 0; f < n; f++) if (strstr(O[f], must)) found = 1;
    snprintf(m, sizeof m, "%s: output contains %s", label, must);
    CHECK(found, m);
}
static void Abstain(const char *label, const char *issue, const char **paths, const char **srcs, int n)
{
    char m[256]; snprintf(m, sizeof m, "%s: abstains", label);
    CHECK(CeoPlanAddFieldAndTotal(issue, paths, srcs, n, &P) == 0 && P.reason[0], m);
}
static const char *l3_paths[] = { "shop.c" };
static const char *l3_srcs[] = { "#include <stdio.h>\nstruct row { const char *sku; double cost; double weight; };\nstatic struct row rows[] = { {\"a1\", 2.0, 0.5}, {\"b2\", 3.0, 1.5} };\nint main(void) { printf(\"%s\\n\", rows[0].sku); return 0; }\n" };
int main(void)
{
    const char *c4 = "Añade un campo stock al producto, con stock 4 para el teclado, 10 para el ratón y 2 para el monitor, y una función que devuelva el valor total del inventario (precio multiplicado por stock de cada producto). Actualiza el main para que muestre ese total.";
    Expect("ext1/case4", c4, ext1_paths, ext1_srcs, 3, 585.0, "inventario_total");
    Expect("ext1/name-first", "Agrega existencias a cada producto: teclado 4, ratón 10 y monitor 2. Quiero ver también el valor total del inventario.", ext1_paths, ext1_srcs, 3, 585.0, "existencias");
    Expect("ext2/english", "Add a quantity in stock to each item (keyboard 4, mouse 10, monitor 2) and show the total inventory value.", ext2_paths, ext2_srcs, 3, 643.0, "{\"mouse\", 12.5f, 10}");
    Expect("ext2/order-fallback", c4, ext2_paths, ext2_srcs, 3, 643.0, "store_total");
    CHECK(P.order_assumed == 1, "ext2/order-fallback: assumption flagged");
    Expect("l1/single-file-typedef", "Add stock: bolt 100, nut 250, and print the total inventory value.", l1_paths, l1_srcs, 1, 50.0, "double parts_total(void);");
    Expect("l2/one-line-main", "hay 10 manzanas, 4 peras, 20 kiwis y 1 uva; quiero el valor total del inventario", l2_paths, l2_srcs, 3, 16.0, "listar(); printf(");
    CHECK(strcmp(P.total_func, "cesta_total") == 0, "l2: name from collection when no shared prefix");
    Abstain("ext2/missing-values", "Añade existencias: teclado 4 y ratón 10, y muestra el valor total del inventario.", ext2_paths, ext2_srcs, 3);
    Abstain("l2/count-mismatch", "Añade unidades: manzana 10, pera 4 y muestra el valor total.", l2_paths, l2_srcs, 3);
    Abstain("l3/two-floats-ambiguous", "a1 5, b2 7: show the total value", l3_paths, l3_srcs, 1);
    CHECK(strstr(P.reason, "cost, weight") != NULL, "l3: reason names the ambiguous members");
    CHECK(!CeoIsAddFieldAndTotalRequest("que diria un pirata del siglo XVIII acerca de la mecanica cuantica ?"), "pirate is not an edit request");
    CHECK(!CeoIsAddFieldAndTotalRequest("ejecuta ps aux"), "shell is not an edit request");
    printf("  TEST RESULTS: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
