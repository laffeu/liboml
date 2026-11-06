<p align="center"><img src="assets/banner.png" alt="liboml banner" width="460"></p>

## Build & Install
```bash
git clone https://github.com/laffeu/liboml.git
cd liboml
make && make install
```

## Code example

```c
#include "liboml.h"
#include <stdio.h>

int main(void)
{
    table *cfg = oml_get("sample.oml");
    if (!cfg)
        return 1;

    const section *global = oml_section(cfg, "__undefined");
    const entry *title = oml_value(global, "name");

    if (title && title->value)
        printf("%s\n", title->value);

    const section *falcon = oml_section(cfg, "falcon_9");
    const entry *name = oml_value(falcon, "name");
    const entry *engines = oml_value(falcon, "engines");

    if (name && name->value)
        printf("Rocket: %s\n", name->value);

    if (engines && engines->array) {
        for (size_t i = 0; engines->array[i]; i++)
            printf("Engine: %s\n", engines->array[i]);
    }

    const section *mission = oml_section(cfg, "mission_control");
    const section *telemetry = oml_nested(cfg, mission, "telemetry_config");
    const entry *rate = oml_value(telemetry, "sampling_rate");

    if (rate && rate->value)
        printf("Sampling rate: %s\n", rate->value);

    oml_free(cfg);
    return 0;
}
```