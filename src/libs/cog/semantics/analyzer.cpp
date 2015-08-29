#include "analyzer.hpp"
#include "collect_labels.hpp"
#include "collect_symbols.hpp"
#include "code_visitor.hpp"
#include "flow_visitor.hpp"

void gorc::cog::perform_semantic_analysis(script &out_script,
                                          ast::translation_unit &tu,
                                          verb_table const &verbs,
                                          constant_table const &constants,
                                          ast::factory &ast_factory)
{
    // Collect labels used in the script, for later steps
    seen_label_set seen_labels;
    seen_label_set called_labels;
    collect_labels_visitor clv(seen_labels, called_labels);
    ast::visit(clv, tu.code);

    // Collect and analyze symbol definitions
    collect_symbols_visitor csv(out_script,
                                seen_labels);
    ast::visit(csv, tu.symbols);

    // Add all exported messages to called labels
    for(auto const &sym : out_script.symbols) {
        if(sym.type == value_type::message) {
            called_labels.insert(sym.name);
        }
    }

    // Analyze code and fold constants
    code_visitor cv(out_script,
                    ast_factory,
                    constants,
                    verbs,
                    seen_labels,
                    /* inside loop */ false,
                    /* inside block */ false);
    ast::visit(cv, tu.code);

    // Analyze execution flow
    flow_visitor dcv(out_script,
                     called_labels,
                     /* inside dead code */ true,
                     /* inside loop */ false,
                     /* containing label */ "");
    ast::visit(dcv, tu.code);
}
