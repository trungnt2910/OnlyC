#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

extern "C"
{
#include "webidl/Generator.h"
}

class Given_IdlGenerator : public testing::Test
{
protected:
    IdlModel* model = nullptr;
    FILE* header = nullptr;
    FILE* source = nullptr;
    FILE* coverage = nullptr;

    void SetUp() override
    {
        model = idl_model_create();
        header = tmpfile();
        source = tmpfile();
        coverage = tmpfile();
        ASSERT_NE(model, nullptr);
        ASSERT_NE(header, nullptr);
        ASSERT_NE(source, nullptr);
        ASSERT_NE(coverage, nullptr);
    }

    void TearDown() override
    {
        idl_model_destroy(model);
        if (header)
        {
            fclose(header);
        }
        if (source)
        {
            fclose(source);
        }
        if (coverage)
        {
            fclose(coverage);
        }
    }

    bool add(const char* json)
    {
        return idl_model_add_json(model, json, strlen(json));
    }

    bool contains(FILE* file, const char* needle)
    {
        fflush(file);
        long length = ftell(file);
        char* text = static_cast<char*>(calloc(static_cast<size_t>(length) + 1, 1));
        if (!text)
        {
            return false;
        }
        rewind(file);
        bool result =
            fread(text, 1, static_cast<size_t>(length), file) == static_cast<size_t>(length) &&
            strstr(text, needle) != nullptr;
        free(text);
        fseek(file, 0, SEEK_END);
        return result;
    }
};

TEST_F(Given_IdlGenerator, When_JsonIsMalformed_Then_ParseFailsWithLocation)
{
    const char* json = "{bad";

    bool success = add(json);

    EXPECT_FALSE(success);
    EXPECT_NE(strstr(idl_model_error(model), "byte"), nullptr);
}

TEST_F(Given_IdlGenerator, When_WebrefSchemaIsMissing_Then_ParseFails)
{
    const char* json = "{}";

    bool success = add(json);

    EXPECT_FALSE(success);
    EXPECT_NE(strstr(idl_model_error(model), "idlNames"), nullptr);
}

TEST_F(Given_IdlGenerator, When_OptionalArgumentsExist_Then_OmittedArgumentsStayOmitted)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{"Document":{"type":"interface","name":"Document",
      "members":[{"type":"operation","name":"createElement","arguments":[
        {"name":"tag"},{"name":"options","optional":true}]}]}},"idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_TRUE(success);
    EXPECT_TRUE(
        contains(header, "dom_Document_createElement_1(Dom_Document self, DomValue argument0)"));
    EXPECT_TRUE(contains(header, "dom_Document_createElement_2("));
    EXPECT_TRUE(contains(source, "\"createElement\", arguments, 1, NULL, 0)"));
    EXPECT_FALSE(contains(header, "dom_Document_createElement_0("));
}

TEST_F(Given_IdlGenerator, When_MethodIsVariadic_Then_ZeroAndManyArgumentsAreSupported)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{"Element":{"type":"interface","name":"Element",
      "members":[{"type":"operation","name":"replaceChildren","arguments":[
        {"name":"nodes","variadic":true}]}]}},"idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_TRUE(success);
    EXPECT_TRUE(contains(header, "dom_Element_replaceChildren_0(Dom_Element self)"));
    EXPECT_TRUE(contains(header,
                         "dom_Element_replaceChildren_0v(Dom_Element self, const DomValue "
                         "*rest, size_t rest_count)"));
    EXPECT_TRUE(contains(source, "\"replaceChildren\", NULL, 0, NULL, 0)"));
}

TEST_F(Given_IdlGenerator, When_StaticMethodExists_Then_GlobalNameKeepsItsCase)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{"URL":{"type":"interface","name":"URL",
      "members":[{"type":"operation","special":"static","name":"createObjectURL",
      "arguments":[{"name":"blob"}]}]}},"idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_TRUE(success);
    EXPECT_TRUE(contains(header, "dom_URL_createObjectURL_1(DomValue argument0)"));
    EXPECT_TRUE(contains(source, "bridge_global(\"URL\")"));
    EXPECT_FALSE(contains(source, "bridge_global(\"url\")"));
}

TEST_F(Given_IdlGenerator, When_PartialMixinDefinesActiveElement_Then_DocumentGetsTheGetter)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{
      "Document":{"type":"interface","name":"Document","members":[]},
      "Focus":{"type":"interface mixin","name":"Focus","members":[]}},
      "idlExtendedNames":{"Document":[{"type":"includes","target":"Document","includes":"Focus"}]}}})"));
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{},"idlExtendedNames":{"Focus":[
      {"type":"interface mixin","name":"Focus","partial":true,"members":[
        {"type":"attribute","name":"activeElement","readonly":true}]}]}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_TRUE(success);
    EXPECT_TRUE(contains(header, "dom_Document_activeElement_get(Dom_Document self)"));
    EXPECT_FALSE(contains(header, "dom_Document_activeElement_set("));
}

TEST_F(Given_IdlGenerator, When_InterfaceInherits_Then_ItGetsMembersWithoutBaseConstructors)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{
      "Node":{"type":"interface","name":"Node","members":[
        {"type":"constructor","arguments":[]},{"type":"attribute","name":"textContent"}]},
      "Element":{"type":"interface","name":"Element","inheritance":"Node","members":[]}},
      "idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_TRUE(success);
    EXPECT_TRUE(contains(header, "dom_Element_textContent_set("));
    EXPECT_TRUE(contains(header, "dom_Node_new_0(void)"));
    EXPECT_FALSE(contains(header, "dom_Element_new_0("));
}

TEST_F(Given_IdlGenerator, When_InheritanceCycles_Then_GenerationFails)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{
      "A":{"type":"interface","name":"A","inheritance":"B","members":[]},
      "B":{"type":"interface","name":"B","inheritance":"A","members":[]}},"idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_FALSE(success);
    EXPECT_NE(strstr(idl_model_error(model), "Cyclic"), nullptr);
}

TEST_F(Given_IdlGenerator, When_TypesAreErased_Then_SameArityOverloadsShareOneWrapper)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{"A":{"type":"interface","name":"A","members":[
      {"type":"operation","name":"accept","arguments":[{"name":"text"}]},
      {"type":"operation","name":"accept","arguments":[{"name":"node"}]}]}},"idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);
    fflush(header);
    rewind(header);
    char line[1024];
    size_t matches = 0;
    while (fgets(line, sizeof(line), header))
    {
        matches += strstr(line, "dom_A_accept_1(") != nullptr;
    }

    EXPECT_TRUE(success);
    EXPECT_EQ(matches, 1u);
}

TEST_F(Given_IdlGenerator, When_UnsupportedMemberExists_Then_CoverageReportsIt)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{"A":{"type":"interface","name":"A",
      "members":[{"type":"iterable"}]}},"idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_TRUE(success);
    EXPECT_TRUE(contains(coverage, "skipped:iterable"));
}

TEST_F(Given_IdlGenerator, When_BaseIsOutsideInputSet_Then_CoverageReportsIt)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{"A":{"type":"interface","name":"A",
      "inheritance":"External","members":[]}},"idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_TRUE(success);
    EXPECT_TRUE(contains(coverage, "external-base-or-mixin"));
}

TEST_F(Given_IdlGenerator, When_OperationHasNoArgumentsArray_Then_GenerationFails)
{
    ASSERT_TRUE(add(R"({"idlparsed":{"idlNames":{"A":{"type":"interface","name":"A",
      "members":[{"type":"operation","name":"broken"}]}},"idlExtendedNames":{}}})"));

    bool success = idl_model_emit(model, header, source, coverage);

    EXPECT_FALSE(success);
    EXPECT_NE(strstr(idl_model_error(model), "argument list"), nullptr);
}
