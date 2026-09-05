#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <model.bin> <subject> <relation> <object>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *model_path = argv[1];
    const char *subject_name = argv[2];
    const char *relation_name = argv[3];
    const char *object_name = argc > 4 ? argv[4] : NULL;

    printf("========================================================\n");
    printf("  WIKIPEDIA STORED-FACT TEST\n");
    printf("========================================================\n\n");

    /* Load model */
    printf("Loading model: %s\n", model_path);
    MODEL *model = ModelLoad(model_path);
    if (model == NULL)
    {
        fprintf(stderr, "Error loading model.\n");
        return EXIT_FAILURE;
    }

    printf("  Symbols  : %u\n", SymbolCount(model->graph->symbols));
    printf("  Relations: %u\n\n", RelationCount(model->graph->relations));

    /* Find symbols */
    SYMBOL_ID subj = SymbolFind(model->graph->symbols, subject_name);
    SYMBOL_ID rel = SymbolFind(model->graph->symbols, relation_name);

    if (subj == SYMBOL_INVALID)
    {
        printf("Symbol '%s' not found in graph.\n", subject_name);
        ModelDestroy(model);
        return EXIT_FAILURE;
    }
    if (rel == SYMBOL_INVALID)
    {
        printf("Symbol '%s' not found in graph.\n", relation_name);
        ModelDestroy(model);
        return EXIT_FAILURE;
    }

    /* Direct query */
    printf("--- Direct Query: %s --%s--> ? ---\n", subject_name, relation_name);
    RELATION *results[32];
    uint32_t count = GraphQuerySubjectRelation(model->graph, subj, rel, results, 32);

    if (count == 0)
    {
        printf("  No direct relations found.\n");
    }
    else
    {
        for (uint32_t i = 0; i < count; i++)
        {
            const SYMBOL *obj_sym = SymbolGet(model->graph->symbols, results[i]->object);
            printf("  %s --%s--> %s (count=%llu, weight=%.3f)\n",
                   subject_name, relation_name,
                   obj_sym ? obj_sym->name : "?",
                   (unsigned long long)results[i]->count,
                   results[i]->weight);
        }
    }
    printf("\n");

    /* Stored-fact check if object provided: exists or not, nothing derived */
    if (object_name)
    {
        SYMBOL_ID obj = SymbolFind(model->graph->symbols, object_name);
        if (obj == SYMBOL_INVALID)
        {
            printf("Symbol '%s' not found. Trying fuzzy...\n", object_name);
            ModelDestroy(model);
            return EXIT_SUCCESS;
        }

        printf("--- Stored: %s --%s--> %s ---\n", subject_name, relation_name, object_name);

        RELATION *stored = GraphFindRelation(model->graph, subj, rel, obj);
        if (stored)
        {
            printf("  STORED (count=%llu, weight=%.3f)\n",
                   (unsigned long long)stored->count, stored->weight);
        }
        else
        {
            printf("  NOT STORED (no such relation)\n");
        }
        printf("\n");
    }

    /* List all known facts about subject */
    printf("--- All facts about %s ---\n", subject_name);
    RELATION *all[64];
    uint32_t total = GraphQuerySubject(model->graph, subj, all, 64);
    for (uint32_t i = 0; i < total; i++)
    {
        const SYMBOL *rel_sym = SymbolGet(model->graph->symbols, all[i]->relation);
        const SYMBOL *obj_sym = SymbolGet(model->graph->symbols, all[i]->object);
        printf("  %s --%s--> %s\n",
               subject_name,
               rel_sym ? rel_sym->name : "?",
               obj_sym ? obj_sym->name : "?");
    }

    ModelDestroy(model);

    printf("\n========================================================\n");
    printf("  Done.\n");
    printf("========================================================\n");

    return EXIT_SUCCESS;
}
