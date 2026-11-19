<p align="center"><img src="assets/banner.png" alt="liboml banner" width="460"></p>

## Library features

- Nested sections, arrays, and multiline strings
- Typed getters for integers, floating-point numbers, and booleans
- `append:` adds settings from other files and rejects duplicates
- `override:` replaces existing values, including values in nested sections
- Concurrent config reads with a buffered file I/O fallback

## Build & Install
```bash
cmake -S .
cmake --build .
cmake --install .
```

## Code example

```c
#include <stdio.h>
#include "liboml.h"

int main(void)
{
    /* Load sample.oml with changes from changes.oml. */
    table *cfg = oml_get("sample.oml");
    if (!cfg) {
        fprintf(stderr, "Failed to load sample.oml\n");
        return 1;
    }

    /* Get the section with top-level keys. */
    const section *root = oml_section(cfg, undefined);

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

    /* Print telemetry if all three values are present and valid. */
    const entry *format = oml_value(telemetry, "format");
    int rate;
    bool enabled;

    if (format && format->value &&
        oml_int(telemetry, "sampling_rate", &rate) &&
        oml_bool(telemetry, "enabled", &enabled)) {
        printf("Falcon 9\nFormat: %s\nSampling rate: %d\nEnabled: %s\n",
               format->value, rate, enabled ? "true" : "false");
    }

    oml_free(cfg);
    return 0;
}
```
