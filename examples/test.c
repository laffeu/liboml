#include <stdio.h>
#include "liboml.h"

int main()
{

    table *config = oml_get("configuration.oml");

    const entry *name = oml_bykey(config, "user", "name");
    if (name) printf("%s\n", name->value);

    const entry *globalarray = oml_bykey(config, NULL, "globalarray");
    if (globalarray)     for (int i = 0; globalarray->array[i]; i++)
        puts(globalarray->array[i]);

    oml_free(config);
}