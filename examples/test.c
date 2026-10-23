#include "liboml.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    table *config = oml_get("examples/configuration.oml");

    if (!config)
        return EXIT_FAILURE;

    /* Global configuration */

    const section *global = oml_section(config, "__undefined");
    const entry *title = oml_key(global, "name");
    const entry *description = oml_key(global, "description");

    if (title)
        printf("%s\n", title->value);

    if (description)
        printf("%s\n", description->value);

    /* Falcon 9 */

    const section *falcon = oml_section(config, "falcon_9");
    const entry *falcon_name = oml_key(falcon, "name");
    const entry *engines = oml_key(falcon, "engines");

    if (falcon_name)
        printf("Rocket: %s\n", falcon_name->value);

    if (engines && engines->array) {
        printf("Engines:\n");

        for (size_t i = 0; engines->array[i]; i++)
            printf("  - %s\n", engines->array[i]);
    }

    /* Starship */

    const section *starship = oml_section(config, "starship");
    const entry *starship_name = oml_key(starship, "name");
    const entry *objectives = oml_key(starship, "objectives");

    if (starship_name)
        printf("Rocket: %s\n", starship_name->value);

    if (objectives && objectives->array) {
        printf("Objectives:\n");

        for (size_t i = 0; objectives->array[i]; i++)
            printf("  - %s\n", objectives->array[i]);
    }

    /* Mission control telemetry */

    const section *mission_control =
        oml_section(config, "mission_control");

    const entry *telemetry_enabled =
        oml_key(mission_control, "telemetry");

    if (telemetry_enabled)
        printf("Telemetry enabled: %s\n", telemetry_enabled->value);

    const section *telemetry_config =
        oml_nested(config, mission_control, "telemetry_config");

    const entry *sampling_rate =
        oml_key(telemetry_config, "sampling_rate");

    const entry *format =
        oml_key(telemetry_config, "format");

    if (sampling_rate)
        printf("Telemetry sampling rate: %s\n", sampling_rate->value);

    if (format)
        printf("Telemetry format: %s\n", format->value);

    /* Storage database */

    const section *storage = oml_section(config, "storage");
    const section *database = oml_nested(config, storage, "database");

    const entry *db_engine = oml_key(database, "engine");
    const entry *db_file = oml_key(database, "file");
    const entry *journal_mode = oml_key(database, "journal_mode");

    if (db_engine)
        printf("Database engine: %s\n", db_engine->value);

    if (db_file)
        printf("Database file: %s\n", db_file->value);

    if (journal_mode)
        printf("Database journal mode: %s\n", journal_mode->value);

    oml_free(config);
    return EXIT_SUCCESS;
}