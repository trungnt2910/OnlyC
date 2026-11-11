#include <stdlib.h>
#include <string.h>

#include "webidl/Generator.h"

static bool add_file(IdlModel* model, const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END))
    {
        if (file)
        {
            fclose(file);
        }
        return false;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET))
    {
        fclose(file);
        return false;
    }
    char* text = malloc((size_t)length + 1);
    bool result = text && fread(text, 1, (size_t)length, file) == (size_t)length &&
                  idl_model_add_json(model, text, (size_t)length);
    free(text);
    fclose(file);
    return result;
}

static FILE* output_file(const char* directory, const char* name)
{
    size_t capacity = strlen(directory) + strlen(name) + 2;
    char* path = malloc(capacity);
    if (!path)
    {
        return NULL;
    }
    snprintf(path, capacity, "%s/%s", directory, name);
    FILE* file = fopen(path, "wb");
    free(path);
    return file;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: idlgen OUTPUT_DIRECTORY WEBREF_JSON...\n");
        return 1;
    }
    IdlModel* model = idl_model_create();
    if (!model)
    {
        return 1;
    }
    for (int i = 2; i < argc; ++i)
    {
        if (!add_file(model, argv[i]))
        {
            fprintf(stderr, "%s: %s\n", argv[i], idl_model_error(model));
            idl_model_destroy(model);
            return 1;
        }
    }
    FILE* header = output_file(argv[1], "web_dom.h");
    FILE* source = output_file(argv[1], "web_dom.c");
    FILE* coverage = output_file(argv[1], "coverage.tsv");
    bool result = header && source && coverage && idl_model_emit(model, header, source, coverage);
    if (header && fclose(header))
    {
        result = false;
    }
    if (source && fclose(source))
    {
        result = false;
    }
    if (coverage && fclose(coverage))
    {
        result = false;
    }
    if (!result)
    {
        fprintf(stderr, "Generation failed: %s\n", idl_model_error(model));
    }
    idl_model_destroy(model);
    return result ? 0 : 1;
}
