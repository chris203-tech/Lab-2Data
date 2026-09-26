Both the delimiter checker and the postfix evaluator rely on a stack because each needs immediate access to the most recently pushed item. 
In the delimiter checker every stack entry is an opening delimiter such as (, [, or { that has not yet been closed, and closing delimiters 
must appear in the reverse order of the openings, so the top of the stack is always the one that must be matched next; for example, 
in ([{}]) the { must be closed by } before the [ can be closed by ].In the postfix evaluator each stack entry is an operand or
an intermediate result still waiting to be used, and when an operator is encountered it must combine the two most recent values; for example, in 8 3 2 * + the * multiplies the top two items 2 and 3 to produce 6 before the + combines 8 with that result. In both algorithms the top of the stack therefore holds the most immediate unfinished piece of work, the innermost unmatched delimiter or the most recent operand,
making LIFO access essential.M
