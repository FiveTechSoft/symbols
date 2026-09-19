/* ============================================================
   test_agent_core: Verification of Agentic AI Core & Tool Contracts
   in a Real Development Scenario (OpenCode-compatible).
   Verifies:
     1. Formal tool contract integrity (grep, find, view, replace, command).
     2. Deterministic serialization/deserialization on wire protocol.
     3. Complete perception-action-observation loop in a bug repair scenario.
     4. Abductive error diagnosis and recovery upon build failure.
     5. Strict fail-closed termination via ACTION_FINAL.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"
#include "agent_action.h"
#include "agent_core.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(int condition, const char *name)
{
    if (condition)
    {
        printf("  [PASS] %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n", name);
        g_fail++;
    }
}

int main(void)
{
    printf("======================================================================\n");
    printf("  TEST & VERIFICACION: AGENTIC AI CORE & CONTRATOS DE TOOLS (OPENCODE)\n");
    printf("======================================================================\n\n");

    /* --- TEST 1: CONTRATOS FORMALES DE HERRAMIENTAS OPENCODE --- */
    printf("--- TEST 1: Verificacion de Contratos Formales de Herramientas ---\n");
    const TOOL_CONTRACT_SPEC *c_grep = AgentGetToolContract(OP_TOOL_GREP_SEARCH);
    const TOOL_CONTRACT_SPEC *c_view = AgentGetToolContract(OP_TOOL_VIEW_FILE);
    const TOOL_CONTRACT_SPEC *c_repl = AgentGetToolContract(OP_TOOL_REPLACE_CONTENT);
    const TOOL_CONTRACT_SPEC *c_cmd  = AgentGetToolContract(OP_TOOL_RUN_COMMAND);

    check(strcmp(c_grep->name, "grep_search") == 0 && c_grep->is_mutating == 0,
          "Contrato grep_search: Solo lectura, busqueda por patron");
    check(strcmp(c_view->name, "view_file") == 0 && c_view->is_mutating == 0,
          "Contrato view_file: Solo lectura, inspeccion de archivo");
    check(strcmp(c_repl->name, "replace_file_content") == 0 && c_repl->is_mutating == 1,
          "Contrato replace_file_content: Mutacion quirurgica en disco");
    check(strcmp(c_cmd->name, "run_command") == 0 && c_cmd->is_mutating == 1,
          "Contrato run_command: Mutacion de entorno / compilacion");

    /* --- TEST 2: SERIALIZACION DETERMINISTA DEL PROTOCOLO DE ACCION --- */
    printf("\n--- TEST 2: Protocolo de Accion y Observacion (Wire Protocol) ---\n");
    AgentAction act_out;
    memset(&act_out, 0, sizeof(act_out));
    act_out.type = ACTION_TOOL_CALL;
    strncpy(act_out.tool, "grep_search", sizeof(act_out.tool) - 1);
    strncpy(act_out.args, "Query='MySymbol' SearchPath='src'", sizeof(act_out.args) - 1);
    strncpy(act_out.prompt, "Buscando simbolo en el proyecto", sizeof(act_out.prompt) - 1);

    char wire_buf[1024];
    int ok_fmt = AgentActionFormat(&act_out, wire_buf, sizeof(wire_buf));
    printf("  Mensaje formateado al harness:\n    \"%s\"\n", wire_buf);
    check(ok_fmt == 1 && strstr(wire_buf, "ACTION: TOOL=grep_search") != NULL,
          "Formateo de accion a protocolo de texto determinista");

    AgentAction act_parsed;
    int ok_parse = AgentActionParse(wire_buf, &act_parsed);
    check(ok_parse == 1 && strcmp(act_parsed.tool, "grep_search") == 0 &&
          strcmp(act_parsed.args, "Query='MySymbol' SearchPath='src'") == 0,
          "Parseo inverso exacto sin perdida de argumentos");

    /* --- TEST 3: ESCENARIO REAL DE REPARACION DE BUG MULTI-PASO --- */
    printf("\n--- TEST 3: Escenario Real: Reparacion Autonoma de Bug en 5 Pasos ---\n");
    AGENT_SESSION session;
    AgentSessionInit(&session, "Fix undefined OldFunction bug", "OldFunction", "mingw32-make test");

    /* PASO 1: Agente decide buscar el simbolo */
    AgentAction act1;
    AgentDecideNextAction(&session, NULL, NULL, &act1);
    printf("  [PASO 1] Agente emite: TOOL=%s, ARGS=\"%s\"\n", act1.tool, act1.args);
    check(strcmp(act1.tool, "grep_search") == 0, "Paso 1: Agente emite grep_search para localizar simbolo");

    /* Simular respuesta del harness a grep */
    AgentObservation obs1;
    memset(&obs1, 0, sizeof(obs1));
    strncpy(obs1.tool, "grep_search", sizeof(obs1.tool) - 1);
    obs1.exit_code = 0;
    strncpy(obs1.output, "src/engine.c:42: void OldFunction(void);", sizeof(obs1.output) - 1);

    AgentProcessObservation(&session, NULL, NULL, &obs1);
    check(strcmp(session.target_file, "src/engine.c") == 0 && session.target_line == 42,
          "Asimilacion 1: Extrajo correctamente archivo y linea objetivo");

    /* PASO 2: Agente decide leer el archivo */
    AgentAction act2;
    AgentDecideNextAction(&session, NULL, NULL, &act2);
    printf("  [PASO 2] Agente emite: TOOL=%s, ARGS=\"%s\"\n", act2.tool, act2.args);
    check(strcmp(act2.tool, "view_file") == 0, "Paso 2: Agente emite view_file para inspeccionar codigo");

    /* Simular respuesta del harness a view_file */
    AgentObservation obs2;
    memset(&obs2, 0, sizeof(obs2));
    strncpy(obs2.tool, "view_file", sizeof(obs2.tool) - 1);
    obs2.exit_code = 0;
    strncpy(obs2.output, "42: void OldFunction(void);\n43: { return; }\n", sizeof(obs2.output) - 1);

    AgentProcessObservation(&session, NULL, NULL, &obs2);
    check(session.state == AGENT_STATE_APPLYING_FIX,
          "Asimilacion 2: Diagnostico completado, formula el parche");

    /* PASO 3: Agente emite el parche quirurgico */
    AgentAction act3;
    AgentDecideNextAction(&session, NULL, NULL, &act3);
    printf("  [PASO 3] Agente emite: TOOL=%s, ARGS=\"%s\"\n", act3.tool, act3.args);
    check(strcmp(act3.tool, "replace_file_content") == 0, "Paso 3: Agente emite replace_file_content");

    /* Simular respuesta del harness a replace_file_content */
    AgentObservation obs3;
    memset(&obs3, 0, sizeof(obs3));
    strncpy(obs3.tool, "replace_file_content", sizeof(obs3.tool) - 1);
    obs3.exit_code = 0;
    strncpy(obs3.output, "Successfully replaced 1 occurrence.", sizeof(obs3.output) - 1);

    AgentProcessObservation(&session, NULL, NULL, &obs3);
    check(session.state == AGENT_STATE_VERIFYING_BUILD,
          "Asimilacion 3: Parche aplicado, pasa a fase de verificacion de compilacion");

    /* PASO 4: Agente ejecuta el comando de verificacion */
    AgentAction act4;
    AgentDecideNextAction(&session, NULL, NULL, &act4);
    printf("  [PASO 4] Agente emite: TOOL=%s, ARGS=\"%s\"\n", act4.tool, act4.args);
    check(strcmp(act4.tool, "run_command") == 0, "Paso 4: Agente emite run_command para validar");

    /* Simular verificacion exitosa en harness */
    AgentObservation obs4;
    memset(&obs4, 0, sizeof(obs4));
    strncpy(obs4.tool, "run_command", sizeof(obs4.tool) - 1);
    obs4.exit_code = 0;
    strncpy(obs4.output, "100% tests passed, 0 tests failed.", sizeof(obs4.output) - 1);

    AgentProcessObservation(&session, NULL, NULL, &obs4);
    check(session.state == AGENT_STATE_COMPLETED,
          "Asimilacion 4: Build exitoso (exit=0), objetivo cumplido");

    /* PASO 5: Agente emite ACTION_FINAL */
    AgentAction act5;
    AgentDecideNextAction(&session, NULL, NULL, &act5);
    printf("  [PASO 5] Agente emite: FINAL: \"%s\"\n", act5.prompt);
    check(act5.type == ACTION_FINAL && strstr(act5.prompt, "verified") != NULL,
          "Paso 5: Agente concluye formalmente con ACTION_FINAL");

    /* --- TEST 4: RECUPERACION ANTE FALLOS (ABDUCCION POR EXIT != 0) --- */
    printf("\n--- TEST 4: Recuperacion ante Fallos y Abduccion de Errores ---\n");
    AGENT_SESSION session_fail;
    AgentSessionInit(&session_fail, "Fix compiler error", "BuggySym", "gcc test.c");
    session_fail.state = AGENT_STATE_VERIFYING_BUILD;

    AgentObservation obs_fail;
    memset(&obs_fail, 0, sizeof(obs_fail));
    strncpy(obs_fail.tool, "run_command", sizeof(obs_fail.tool) - 1);
    obs_fail.exit_code = 1; /* Fallo */
    strncpy(obs_fail.output, "error: unknown type name 'uint32_t'", sizeof(obs_fail.output) - 1);

    AgentProcessObservation(&session_fail, NULL, NULL, &obs_fail);
    check(session_fail.state == AGENT_STATE_DIAGNOSING_ERROR,
          "Resiliencia: exit_code != 0 no aborta; activa estado de diagnostico");

    AgentAction act_diag;
    AgentDecideNextAction(&session_fail, NULL, NULL, &act_diag);
    printf("  Agente diagnosticando: TOOL=%s, ARGS=\"%s\"\n", act_diag.tool, act_diag.args);
    check(strcmp(act_diag.tool, "view_file") == 0,
          "Recuperacion: Agente inspecciona cabeceras del archivo para corregir includes");

    printf("\n======================================================================\n");
    printf("  RESUMEN TEST: %d PASS, %d FAIL\n", g_pass, g_fail);
    printf("======================================================================\n");

    return g_fail > 0 ? 1 : 0;
}
