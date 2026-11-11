#include "app/controller/ApplicationController.h"

#include <ctype.h>
#include <string.h>

#include "strings/Resources.h"

int application_controller_request_file(ApplicationController* controller,
                                        const char* name,
                                        int size,
                                        int count)
{
    ApplicationState* state = controller->state;
    if (state->import_in_progress)
    {
        return 0;
    }
    if (count != 1)
    {
        application_state_notice(state, ONLYC_STRING_ERROR_ONE_FILE);
        return 0;
    }
    const char* extension = strrchr(name, '.');
    char normalized[6] = {0};
    if (extension && strlen(extension) < sizeof(normalized))
    {
        for (size_t i = 0; extension[i]; ++i)
        {
            normalized[i] = (char)tolower((unsigned char)extension[i]);
        }
    }
    if (strcmp(normalized, ".yaml") && strcmp(normalized, ".yml"))
    {
        application_state_notice(state, ONLYC_STRING_ERROR_YAML_EXTENSION);
        return 0;
    }
    if (size <= 0 || (unsigned)size > MAX_IMPORT)
    {
        application_state_notice(state, ONLYC_STRING_ERROR_IMPORT_SIZE);
        return 0;
    }
    state->import_in_progress = 1;
    return 1;
}
