#include "Schema.hpp"

#include <string>

namespace log_surgeon {
Schema::Schema() {
    m_schema_ast = std::make_unique<SchemaAST>();
}

Schema::Schema(std::string const& schema_file_path)
        : m_schema_ast{SchemaParser::try_schema_file(schema_file_path)} {}

auto Schema::append_var(std::string_view const var_schema) const -> void {
    auto const schema_ast = SchemaParser::try_schema_string(var_schema);
    m_schema_ast->append_schema_var(std::move(schema_ast->m_schema_vars[0]));
}

auto Schema::insert_var(std::string_view const var_schema, uint32_t const priority) const
        -> void {
    auto const schema_ast = SchemaParser::try_schema_string(var_schema);
    m_schema_ast->insert_schema_var(std::move(schema_ast->m_schema_vars[0]), priority);
}
}  // namespace log_surgeon
