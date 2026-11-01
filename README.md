<img src="assets/banner.png" alt="liboml banner" width="460">

## Build & Install
```bash
git clone https://github.com/laffeu/liboml.git
cd liboml
make && make install
```

## API Reference

| Function | Description |
|---|---|
| `oml_get(filename)` | Load and parse an OML configuration file |
| `oml_section(table, name)` | Retrieve a top-level or global section |
| `oml_nested(table, parent, name)` | Retrieve a nested section from its parent |
| `oml_value(section, key)` | Retrieve an entry by its key |
| `oml_free(table)` | Free all allocated configuration resources |

## Code example

### C

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

### C++

```c++
#include "loml.hpp"
#include <iostream>
#include <string>
#include <vector>

int main()
{
    loml::Table cfg("sample.oml");

    if (auto title = cfg.get("__undefined", "name"))
        std::cout << *title << std::endl;

    if (auto name = cfg.get("falcon_9", "name"))
        std::cout << "Rocket: " << *name << std::endl;

    if (auto engines = cfg.get<std::vector<std::string>>(
            "falcon_9", "engines")) {
        for (const auto &engine : *engines)
            std::cout << "Engine: " << engine << std::endl;
    }
    
    if (auto rate = cfg.get(
            {"mission_control", "telemetry_config"},
            "sampling_rate"))
        std::cout << "Sampling rate: " << *rate << std::endl;

    return 0;
}
```