#include "webidl/Generator.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

typedef struct
{
    const char* name;
    const char* kind;
    const char* base;
    yyjson_val** members;
    size_t member_count;
    const char** mixins;
    size_t mixin_count;
    bool visiting;
} IdlInterface;

struct IdlModel
{
    yyjson_doc** documents;
    size_t document_count;
    IdlInterface* interfaces;
    size_t interface_count;
    char** symbols;
    size_t symbol_count;
    char error[256];
};

static bool fail(IdlModel* model, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(model->error, sizeof(model->error), format, arguments);
    va_end(arguments);
    return false;
}

static bool append(void** array, size_t* count, size_t item_size, const void* item)
{
    void* grown = realloc(*array, (*count + 1) * item_size);
    if (!grown)
    {
        return false;
    }
    *array = grown;
    memcpy((char*)grown + *count * item_size, item, item_size);
    ++*count;
    return true;
}

static const char* string_field(yyjson_val* object, const char* key)
{
    const char* value = yyjson_get_str(yyjson_obj_get(object, key));
    return value ? value : "";
}

static bool is_kind(yyjson_val* object, const char* kind)
{
    return strcmp(string_field(object, "type"), kind) == 0;
}

static IdlInterface* find_interface(IdlModel* model, const char* name)
{
    for (size_t i = 0; i < model->interface_count; ++i)
    {
        if (strcmp(model->interfaces[i].name, name) == 0)
        {
            return &model->interfaces[i];
        }
    }
    return NULL;
}

static IdlInterface* ensure_interface(IdlModel* model, const char* name)
{
    IdlInterface* existing = find_interface(model, name);
    if (existing)
    {
        return existing;
    }
    IdlInterface added = {.name = name, .kind = "interface", .base = ""};
    if (!append((void**)&model->interfaces, &model->interface_count, sizeof(added), &added))
    {
        fail(model, "Out of memory collecting interfaces");
        return NULL;
    }
    return &model->interfaces[model->interface_count - 1];
}

static bool collect_definition(IdlModel* model, yyjson_val* definition)
{
    if (is_kind(definition, "includes"))
    {
        const char* target = string_field(definition, "target");
        const char* mixin = string_field(definition, "includes");
        if (!*target || !*mixin)
        {
            return fail(model, "An includes statement is missing its target or mixin");
        }
        IdlInterface* interface = ensure_interface(model, target);
        return interface &&
               (append(
                    (void**)&interface->mixins, &interface->mixin_count, sizeof(mixin), &mixin) ||
                fail(model, "Out of memory collecting mixins"));
    }
    const char* kind = string_field(definition, "type");
    if (strcmp(kind, "interface") && strcmp(kind, "interface mixin") && strcmp(kind, "namespace") &&
        strcmp(kind, "dictionary"))
    {
        /* Types are intentionally erased to handles. Typedefs, enums and
         * callback declarations therefore do not need C representations. */
        return true;
    }
    const char* name = string_field(definition, "name");
    if (!*name || !yyjson_is_arr(yyjson_obj_get(definition, "members")))
    {
        return fail(model, "Invalid interface definition: %s", name);
    }
    IdlInterface* interface = ensure_interface(model, name);
    if (!interface)
    {
        return false;
    }
    interface->kind = kind;
    const char* base = string_field(definition, "inheritance");
    if (*base)
    {
        if (*interface->base && strcmp(interface->base, base))
        {
            return fail(model, "Conflicting base interfaces for %s", name);
        }
        interface->base = base;
    }
    size_t index, maximum;
    yyjson_val* member;
    yyjson_arr_foreach(yyjson_obj_get(definition, "members"), index, maximum, member)
    {
        if (!append((void**)&interface->members, &interface->member_count, sizeof(member), &member))
        {
            return fail(model, "Out of memory collecting members");
        }
    }
    return true;
}

IdlModel* idl_model_create(void)
{
    return calloc(1, sizeof(IdlModel));
}

void idl_model_destroy(IdlModel* model)
{
    if (!model)
    {
        return;
    }
    for (size_t i = 0; i < model->interface_count; ++i)
    {
        free(model->interfaces[i].members);
        free(model->interfaces[i].mixins);
    }
    for (size_t i = 0; i < model->document_count; ++i)
    {
        yyjson_doc_free(model->documents[i]);
    }
    for (size_t i = 0; i < model->symbol_count; ++i)
    {
        free(model->symbols[i]);
    }
    free(model->interfaces);
    free(model->documents);
    free(model->symbols);
    free(model);
}

const char* idl_model_error(const IdlModel* model)
{
    return model ? model->error : "Out of memory creating model";
}

bool idl_model_add_json(IdlModel* model, const char* json, size_t length)
{
    yyjson_read_err error;
    yyjson_doc* document = yyjson_read_opts((char*)json, length, 0, NULL, &error);
    if (!document)
    {
        return fail(model, "Invalid JSON at byte %zu: %s", error.pos, error.msg);
    }
    if (!append((void**)&model->documents, &model->document_count, sizeof(document), &document))
    {
        yyjson_doc_free(document);
        return fail(model, "Out of memory collecting JSON documents");
    }
    yyjson_val* parsed = yyjson_obj_get(yyjson_doc_get_root(document), "idlparsed");
    yyjson_val* names = yyjson_obj_get(parsed, "idlNames");
    yyjson_val* extended = yyjson_obj_get(parsed, "idlExtendedNames");
    if (!yyjson_is_obj(names) || !yyjson_is_obj(extended))
    {
        return fail(model, "Expected Webref idlparsed.idlNames and idlExtendedNames objects");
    }
    size_t index, maximum;
    yyjson_val *key, *value;
    yyjson_obj_foreach(names, index, maximum, key, value)
    {
        if (!collect_definition(model, value))
        {
            return false;
        }
    }
    yyjson_obj_foreach(extended, index, maximum, key, value)
    {
        if (!yyjson_is_arr(value))
        {
            return fail(model, "Expected array of partial definitions");
        }
        size_t part_index, part_maximum;
        yyjson_val* part;
        yyjson_arr_foreach(value, part_index, part_maximum, part)
        {
            if (!collect_definition(model, part))
            {
                return false;
            }
        }
    }
    return true;
}

static void identifier(char* destination, size_t capacity, const char* name)
{
    size_t i = 0;
    for (; name[i] && i + 1 < capacity; ++i)
    {
        unsigned char character = (unsigned char)name[i];
        destination[i] = isalnum(character) || character == '_' ? (char)character : '_';
    }
    destination[i] = '\0';
}

static void literal(FILE* stream, const char* text)
{
    fputc('"', stream);
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
    {
        if (*p == '"' || *p == '\\')
        {
            fputc('\\', stream);
        }
        if (*p < 32 || *p >= 127)
        {
            fprintf(stream, "\\%03o", *p);
        }
        else
        {
            fputc(*p, stream);
        }
    }
    fputc('"', stream);
}

/* Returns 0 for an existing overload after type erasure, 1 for a new symbol,
 * and -1 for failure. All declarations remain visible in coverage.tsv. */
static int remember_symbol(IdlModel* model, const char* symbol)
{
    for (size_t i = 0; i < model->symbol_count; ++i)
    {
        if (strcmp(model->symbols[i], symbol) == 0)
        {
            return 0;
        }
    }
    char* copy = malloc(strlen(symbol) + 1);
    if (!copy)
    {
        fail(model, "Out of memory recording symbols");
        return -1;
    }
    strcpy(copy, symbol);
    if (!append((void**)&model->symbols, &model->symbol_count, sizeof(copy), &copy))
    {
        free(copy);
        fail(model, "Out of memory recording symbols");
        return -1;
    }
    return 1;
}

static void emit_receiver(FILE* source, const IdlInterface* owner, bool is_static)
{
    if (is_static)
    {
        fputs("bridge_global(", source);
        literal(source, owner->name);
        fputc(')', source);
    }
    else
    {
        fputs("self", source);
    }
}

static bool emit_property(IdlModel* model,
                          const IdlInterface* owner,
                          yyjson_val* member,
                          FILE* header,
                          FILE* source,
                          FILE* coverage,
                          bool setter)
{
    char owner_id[256], member_id[256], symbol[600];
    const char* name = string_field(member, "name");
    identifier(owner_id, sizeof(owner_id), owner->name);
    identifier(member_id, sizeof(member_id), name);
    snprintf(symbol, sizeof(symbol), "dom_%s_%s_%s", owner_id, member_id, setter ? "set" : "get");
    int added = remember_symbol(model, symbol);
    if (added <= 0)
    {
        return added == 0;
    }
    bool is_static = strcmp(string_field(member, "special"), "static") == 0 ||
                     strcmp(owner->kind, "namespace") == 0;
    char parameters[320];
    if (is_static)
    {
        snprintf(parameters, sizeof(parameters), "%s", setter ? "DomValue value" : "void");
    }
    else
    {
        snprintf(parameters,
                 sizeof(parameters),
                 "Dom_%s self%s",
                 owner_id,
                 setter ? ", DomValue value" : "");
    }
    const char* result = setter ? "void" : "DomValue";
    fprintf(header, "%s %s(%s);\n", result, symbol, parameters);
    fprintf(source,
            "%s %s(%s) {\n    %sbridge_%s(",
            result,
            symbol,
            parameters,
            setter ? "" : "return ",
            setter ? "set" : "get");
    emit_receiver(source, owner, is_static);
    fputs(", ", source);
    literal(source, name);
    fprintf(source, "%s);\n}\n\n", setter ? ", value" : "");
    fprintf(coverage, "%s\t%s\t%s\t%s\n", owner->name, name, setter ? "setter" : "getter", symbol);
    return true;
}

static void
emit_parameters(FILE* stream, const char* owner_id, bool is_static, size_t count, bool variadic)
{
    bool separator = false;
    if (!is_static)
    {
        fprintf(stream, "Dom_%s self", owner_id);
        separator = true;
    }
    for (size_t i = 0; i < count; ++i)
    {
        fprintf(stream, "%sDomValue argument%zu", separator ? ", " : "", i);
        separator = true;
    }
    if (variadic)
    {
        fprintf(stream, "%sconst DomValue *rest, size_t rest_count", separator ? ", " : "");
    }
    else if (!separator)
    {
        fputs("void", stream);
    }
}

static bool emit_call(IdlModel* model,
                      const IdlInterface* owner,
                      yyjson_val* member,
                      FILE* header,
                      FILE* source,
                      FILE* coverage,
                      size_t count,
                      bool variadic)
{
    bool constructor = is_kind(member, "constructor");
    const char* name = constructor ? "new" : string_field(member, "name");
    char owner_id[256], member_id[256], symbol[600];
    identifier(owner_id, sizeof(owner_id), owner->name);
    identifier(member_id, sizeof(member_id), name);
    snprintf(
        symbol, sizeof(symbol), "dom_%s_%s_%zu%s", owner_id, member_id, count, variadic ? "v" : "");
    int added = remember_symbol(model, symbol);
    if (added <= 0)
    {
        return added == 0;
    }
    bool is_static = constructor || strcmp(string_field(member, "special"), "static") == 0 ||
                     strcmp(owner->kind, "namespace") == 0;
    fprintf(header, "DomValue %s(", symbol);
    emit_parameters(header, owner_id, is_static, count, variadic);
    fputs(");\n", header);
    fprintf(source, "DomValue %s(", symbol);
    emit_parameters(source, owner_id, is_static, count, variadic);
    fputs(") {\n", source);
    if (count)
    {
        fputs("    const DomValue arguments[] = {", source);
        for (size_t i = 0; i < count; ++i)
        {
            fprintf(source, "%sargument%zu", i ? ", " : "", i);
        }
        fputs("};\n", source);
    }
    fprintf(source, "    return bridge_%s(", constructor ? "construct" : "call");
    if (constructor)
    {
        literal(source, owner->name);
    }
    else
    {
        emit_receiver(source, owner, is_static);
        fputs(", ", source);
        literal(source, name);
    }
    fprintf(source,
            ", %s, %zu, %s);\n}\n\n",
            count ? "arguments" : "NULL",
            count,
            variadic ? "rest, rest_count" : "NULL, 0");
    fprintf(coverage,
            "%s\t%s\t%s\t%s\n",
            owner->name,
            name,
            constructor ? "constructor" : "method",
            symbol);
    return true;
}

static bool emit_member(IdlModel* model,
                        const IdlInterface* owner,
                        yyjson_val* member,
                        FILE* header,
                        FILE* source,
                        FILE* coverage,
                        bool inherited)
{
    const char* name = string_field(member, "name");
    if (is_kind(member, "attribute") || is_kind(member, "field") || is_kind(member, "const"))
    {
        if (!*name)
        {
            return fail(model, "Unnamed property in %s", owner->name);
        }
        if (!emit_property(model, owner, member, header, source, coverage, false))
        {
            return false;
        }
        if (!yyjson_is_true(yyjson_obj_get(member, "readonly")) && !is_kind(member, "const"))
        {
            return emit_property(model, owner, member, header, source, coverage, true);
        }
        return true;
    }
    bool constructor = is_kind(member, "constructor");
    if (constructor && inherited)
    {
        return true;
    }
    if (!constructor && (!is_kind(member, "operation") || !*name))
    {
        fprintf(
            coverage, "%s\t%s\tskipped:%s\t-\n", owner->name, name, string_field(member, "type"));
        return true;
    }
    yyjson_val* arguments = yyjson_obj_get(member, "arguments");
    if (!yyjson_is_arr(arguments))
    {
        return fail(model, "Missing argument list for %s.%s", owner->name, name);
    }
    size_t total = yyjson_arr_size(arguments);
    bool variadic =
        total && yyjson_is_true(yyjson_obj_get(yyjson_arr_get_last(arguments), "variadic"));
    size_t fixed = total - (variadic ? 1 : 0);
    size_t required = fixed;
    while (required &&
           yyjson_is_true(yyjson_obj_get(yyjson_arr_get(arguments, required - 1), "optional")))
    {
        --required;
    }
    /* Keep omitted arguments omitted. Explicit null and undefined remain values
     * and are never substituted for a shorter call. Same-arity overloads share
     * a wrapper because every argument is represented by a JavaScript handle. */
    for (size_t count = required; count <= fixed; ++count)
    {
        if (!emit_call(model, owner, member, header, source, coverage, count, false))
        {
            return false;
        }
    }
    return !variadic || emit_call(model, owner, member, header, source, coverage, fixed, true);
}

static bool emit_interface(IdlModel* model,
                           IdlInterface* owner,
                           IdlInterface* part,
                           FILE* header,
                           FILE* source,
                           FILE* coverage,
                           bool inherited)
{
    if (part->visiting)
    {
        return fail(model, "Cyclic inheritance or mixin inclusion at %s", part->name);
    }
    part->visiting = true;
    for (size_t i = 0; i < part->member_count; ++i)
    {
        if (!emit_member(model, owner, part->members[i], header, source, coverage, inherited))
        {
            part->visiting = false;
            return false;
        }
    }
    for (size_t i = 0; i <= part->mixin_count; ++i)
    {
        const char* name = i == part->mixin_count ? part->base : part->mixins[i];
        if (!*name)
        {
            continue;
        }
        IdlInterface* parent = find_interface(model, name);
        if (!parent)
        {
            fprintf(coverage, "%s\t%s\texternal-base-or-mixin\t-\n", owner->name, name);
        }
        else if (!emit_interface(model, owner, parent, header, source, coverage, true))
        {
            part->visiting = false;
            return false;
        }
    }
    part->visiting = false;
    return true;
}

bool idl_model_emit(IdlModel* model, FILE* header, FILE* source, FILE* coverage)
{
    for (size_t i = 0; i < model->symbol_count; ++i)
    {
        free(model->symbols[i]);
    }
    free(model->symbols);
    model->symbols = NULL;
    model->symbol_count = 0;
    fputs(
        "/* Generated by idlgen from Webref. Do not edit. */\n"
        "#ifndef GENERATED_WEB_DOM_H\n#define GENERATED_WEB_DOM_H\n#include \"interop/Bridge.h\"\n"
        "\n",
        header);
    fputs("/* Generated by idlgen from Webref. Do not edit. */\n#include \"web_dom.h\"\n\n",
          source);
    fputs("interface\tmember\tstatus\tsymbol\n", coverage);
    for (size_t i = 0; i < model->interface_count; ++i)
    {
        char name[256];
        identifier(name, sizeof(name), model->interfaces[i].name);
        fprintf(header, "typedef DomValue Dom_%s;\n", name);
    }
    for (size_t i = 0; i < model->interface_count; ++i)
    {
        IdlInterface* interface = &model->interfaces[i];
        fprintf(header, "\n/* %s */\n", interface->name);
        if (!emit_interface(model, interface, interface, header, source, coverage, false))
        {
            return false;
        }
    }
    fputs("\n#endif\n", header);
    if (ferror(header) || ferror(source) || ferror(coverage))
    {
        return fail(model, "Could not write generated bindings");
    }
    return true;
}
