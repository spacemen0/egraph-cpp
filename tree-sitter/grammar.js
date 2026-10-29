
const PREC = {
    ADD: 1,
    MULTIPLY: 2,
    UNARY: 3,
};

const PROPERTY_KEYWORDS = [
    'symmetric',
    'orthogonal',
    'orthonormal',
    'identity',
    'zero',
    'upper_triangular',
    'lower_triangular',
    'diagonal',
    'positive_definite',
    'positive_semi_definite',
    'full_rank',
    'non_singular',
    'permutation',
    'tall',
    'wide',
];

const RESERVED_KEYWORDS = [
    'symbols',
    'expressions',
    'operations',
    'Matrix',
    'Scalar',
    'ColVector',
    'RowVector',
    ...PROPERTY_KEYWORDS,
    'trans',
    'inv',
    'extract_best',
    'extract_all',
    'evaluate',
    'effort',
    'low',
    'medium',
    'high',
];

module.exports = grammar({
    name: 'symbolic_dsl',

    extras: $ => [/\s/],

    word: $ => $.identifier,

    // Tree-sitter >= 0.25:
    // All language keywords are globally reserved.
    reserved: {
        global: $ => RESERVED_KEYWORDS,
    },

    rules: {
        program: $ => seq(
            $.symbols_block,
            $.expressions_block,
            $.operations_block,
        ),

        // Lexical rules

        identifier: $ => /[A-Za-z_][A-Za-z0-9_]*/,

        // Unsigned decimal scalar.
        // Unary negation is handled by the parser.
        double: $ => /[0-9]+\.[0-9]+/,

        // File paths only.
        // Backslashes are literal; no escape sequences.
        string_literal: $ => /"[^"\r\n]*"/,

        // Block 1: Symbols

        symbols_block: $ => seq(
            'symbols',
            '{',
            repeat($.symbol_decl),
            '}',
            optional(';'),
        ),

        symbol_decl: $ => seq(
            $.identifier,
            ':',
            $.type_spec,
            optional($.property_flags),
            ';',
        ),

        type_spec: $ => choice(
            'Matrix',
            'Scalar',
            'ColVector',
            'RowVector',
        ),

        property_flags: $ => seq(
            '{',
            $.property_literal,
            repeat(seq(',', $.property_literal)),
            '}',
        ),

        property_literal: $ => choice(
            ...PROPERTY_KEYWORDS,
        ),

        // Block 2: Expressions

        expressions_block: $ => seq(
            'expressions',
            '{',
            repeat($.expression_decl),
            '}',
            optional(';'),
        ),

        expression_decl: $ => seq(
            $.identifier,
            '=',
            $.expr,
            ';',
        ),

        expr: $ => choice(
            $.binary_expression,
            $.unary_expression,
            $.primary,
        ),

        // Binary operators are left-associative.
        // Multiplication binds more tightly than addition.
        binary_expression: $ => choice(
            prec.left(PREC.ADD, seq(
                field('left', $.expr),
                field('operator', choice('+', '-')),
                field('right', $.expr),
            )),

            prec.left(PREC.MULTIPLY, seq(
                field('left', $.expr),
                field('operator', choice('*', '/')),
                field('right', $.expr),
            )),
        ),

        // A single unary minus before a primary.
        unary_expression: $ => prec(
            PREC.UNARY,
            seq(
                field('operator', '-'),
                field('argument', $.primary),
            ),
        ),

        primary: $ => choice(
            seq(
                $.identifier,
                optional($.property_flags),
            ),
            $.double,
            $.matrix_op,
            seq('(', $.expr, ')'),
        ),

        matrix_op: $ => seq(
            choice('trans', 'inv'),
            '(',
            $.expr,
            ')',
        ),

        // Block 3: Operation directives

        operations_block: $ => seq(
            'operations',
            '{',
            repeat($.operation_call),
            '}',
            optional(';'),
        ),

        operation_call: $ => choice(
            $.extract_call,
            $.evaluate_call,
        ),

        extract_call: $ => seq(
            choice('extract_best', 'extract_all'),
            '(',
            $.identifier,
            optional(seq(',', $.call_options)),
            ')',
            ';',
        ),

        evaluate_call: $ => seq(
            'evaluate',
            '(',
            $.identifier,
            optional(seq(',', $.call_options)),
            ')',
            ';',
        ),

        call_options: $ => choice(
            seq(
                $.effort_spec,
                optional(seq(',', $.data_bindings)),
            ),
            $.data_bindings,
        ),

        effort_spec: $ => seq(
            optional(seq('effort', '=')),
            $.effort_level,
        ),

        effort_level: $ => choice(
            'low',
            'medium',
            'high',
        ),

        data_bindings: $ => seq(
            '{',
            $.data_binding_list,
            '}',
        ),

        data_binding_list: $ => seq(
            $.data_binding,
            repeat(seq(',', $.data_binding)),
        ),

        data_binding: $ => seq(
            $.identifier,
            '=',
            $.string_literal,
        ),
    },
});
