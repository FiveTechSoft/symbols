#ifndef AGENT_ACTION_H
#define AGENT_ACTION_H

#include <stddef.h>

#define AGENT_NAME_MAX   32
#define AGENT_ARGS_MAX  512
#define AGENT_MSG_MAX   256
#define AGENT_OBS_MAX   1024

typedef enum {
    ACTION_FINAL = 0,    /* Respuesta conversacional terminada (no requiere harness) */
    ACTION_TOOL_CALL,   /* Petición al harness para ejecutar una herramienta */
    ACTION_ABSTAIN      /* Fail-closed: el motor no sabe qué hacer o rechaza */
} AgentActionType;

typedef struct {
    AgentActionType type;
    char tool[AGENT_NAME_MAX];   /* "cmd", "powershell", "gcc", "fs_read" */
    char args[AGENT_ARGS_MAX];   /* Línea de argumentos normalizada */
    char prompt[AGENT_MSG_MAX];  /* Explicación determinista para el usuario/log */
} AgentAction;

typedef struct {
    char tool[AGENT_NAME_MAX];
    int exit_code;               /* 0 = éxito, != 0 código de error del SO */
    char output[AGENT_OBS_MAX];  /* stdout/stderr capturado por el harness */
} AgentObservation;

/* Serialización y deserialización determinista (sin dependencias externas) */
int AgentActionFormat(const AgentAction *act, char *buf, size_t sz);
int AgentActionParse(const char *line, AgentAction *act);

int AgentObservationFormat(const AgentObservation *obs, char *buf, size_t sz);
int AgentObservationParse(const char *line, AgentObservation *obs);

#endif /* AGENT_ACTION_H */
