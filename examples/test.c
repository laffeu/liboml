#include "liboml.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    table *config = oml_get("examples/configuration.oml");
    if (!config)
        return EXIT_FAILURE;

    const section *global = oml_bysec(config, "__undefined");
    const entry *title = oml_bykey(global, "name");
    const entry *description = oml_bykey(global, "description");

    if (title)
        printf("%s\n", title->value);

    if (description)
        printf("%s\n", description->value);

    /* Falcon 9 */

    const section *falcon = oml_bysec(config, "falcon_9");
    const entry *falcon_name = oml_bykey(falcon, "name");
    const entry *engines = oml_bykey(falcon, "engines");

    if (falcon_name)
        printf("Rocket: %s\n", falcon_name->value);

    if (engines && engines->array) {
        printf("Engines:\n");

        for (size_t i = 0; engines->array[i]; i++)
            printf("  - %s\n", engines->array[i]);
    }

    /* Starship */

    const section *starship = oml_bysec(config, "starship");
    const entry *starship_name = oml_bykey(starship, "name");
    const entry *objectives = oml_bykey(starship, "objectives");

    if (starship_name)
        printf("Rocket: %s\n", starship_name->value);

    if (objectives && objectives->array) {
        printf("Objectives:\n");

        for (size_t i = 0; objectives->array[i]; i++)
            printf("  - %s\n", objectives->array[i]);
    }

    oml_free(config);
    return EXIT_SUCCESS;
}