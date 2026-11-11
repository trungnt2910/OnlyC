#ifndef ONLYC_WEBIDL_GENERATOR_H
#define ONLYC_WEBIDL_GENERATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct IdlModel IdlModel;

IdlModel* idl_model_create(void);
void idl_model_destroy(IdlModel* model);
bool idl_model_add_json(IdlModel* model, const char* json, size_t length);
bool idl_model_emit(IdlModel* model, FILE* header, FILE* source, FILE* coverage);
const char* idl_model_error(const IdlModel* model);

#endif
