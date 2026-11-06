<p align="center"><img src="assets/banner.png" alt="liboml banner" width="460"></p>

## Build & Install
```bash
git clone https://github.com/laffeu/liboml.git
cd liboml
make liburing
make && make install
```

## Code example

```c
#include <stdio.h>
#include <stdlib.h>
#include "liboml.h"

int main()
{
    /* Load sample.oml with changes from changes.oml. */
    table *cfg = oml_get("sample.oml");

    /* Get the section with top-level keys. */
    const section *root = oml_section(cfg, NULL);

    /* Find telemetry_config inside falcon_9. */
    const section *falcon_9 = oml_section(cfg, "falcon_9");
    const section *telemetry = oml_nested(cfg, falcon_9, "telemetry_config");

    const entry *title = oml_value(root, "name");
    const entry *description = oml_value(root, "description");

    /* Print the title and description if both are present. */
    if (title && title->value && description && description->value) {
        printf("%s\n%s\n", title->value, description->value);
    }

    /* Print the Falcon 9 missions. */
    const entry *missions = oml_value(falcon_9, "missions");

    if (missions && missions->array) {
        puts("Missions:");
        for (size_t i = 0; missions->array[i] != NULL; i++) {
            printf("- %s\n", missions->array[i]);
        }
    }

    const entry *format = oml_value(telemetry, "format");
    const int *rate = oml_int(telemetry, "sampling_rate");
    const bool *enabled = oml_bool(telemetry, "enabled");

    /* Print telemetry if all three values are present. */
    if (format && format->value && rate && enabled) {
        printf("Falcon 9\nFormat: %s\nSampling rate: %d\nEnabled: %s\n",
               format->value, *rate, *enabled ? "true" : "false");
    }

    /* Free rate and enabled even if nothing was printed. */
    free((void *)rate);
    free((void *)enabled);

    /* Free the config, its sections, and its entries. */
    oml_free(cfg);
    return 0;
}
```