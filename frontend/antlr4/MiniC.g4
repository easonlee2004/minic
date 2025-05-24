grammar MiniC;

// 词法规则名总是以大写字母开头

// 语法规则名总是以小写字母开头

// 每个非终结符尽量多包含闭包、正闭包或可选符等的EBNF范式描述

// 若非终结符由多个产生式组成，则建议在每个产生式的尾部追加# 名称来区分，详细可查看非终结符statement的描述

// 语法规则描述：EBNF范式

// 源文件编译单元定义
compileUnit: (funcDef | varDecl)* EOF;

// 函数定义，目前不支持形参，也不支持返回void类型等
funcDef: T_INT T_ID T_L_PAREN T_R_PAREN block;

// 语句块看用作函数体，这里允许多个语句，并且不含任何语句
block: T_L_BRACE blockItemList? T_R_BRACE;

// 每个ItemList可包含至少一个Item
blockItemList: blockItem+;

// 每个Item可以是一个语句，或者变量声明语句
blockItem: statement | varDecl;

// 变量声明，目前不支持变量含有初值
varDecl: basicType varDef (T_COMMA varDef)* T_SEMICOLON;

// 基本类型
basicType: T_INT;

// 变量定义
varDef: T_ID;

// 目前语句支持return和赋值语句
// 增加if while break continue
statement:
	T_RETURN expr T_SEMICOLON										# returnStatement
	| lVal T_ASSIGN expr T_SEMICOLON								# assignStatement
	| block															# blockStatement
	| expr? T_SEMICOLON												# expressionStatement
	| T_IF T_L_PAREN expr T_R_PAREN statement (T_ELSE statement)?	# ifStatement
	| T_WHILE T_L_PAREN expr T_R_PAREN statement					# whileStatement
	| T_BREAK T_SEMICOLON											# breakStatement
	| T_CONTINUE T_SEMICOLON										# continueStatement;

// 表达式定义
expr: logicOrExp;

logicOrExp: logicAndExp (T_LOR logicAndExp)*;
logicAndExp: eqExp (T_LAND eqExp)*;
eqExp: relExp ((T_EQ | T_NE) relExp)*;
relExp: addExp ((T_LT | T_GT | T_LE | T_GE) addExp)*;

addExp: mulExp (addOp mulExp)*;
mulExp: unaryExp (mulOp unaryExp)*;

// 一元表达式
unaryExp: 
    primaryExp
    | T_ID T_L_PAREN realParamList? T_R_PAREN
    | (T_SUB)* primaryExp
    | unaryOp unaryExp
	| T_LNOT unaryExp;

// 运算符定义
addOp: T_ADD | T_SUB;
mulOp: T_MUL | T_DIV | T_MOD;
eqOp: T_EQ | T_NE;
relOp: T_GT | T_LT | T_GE | T_LE;

// 一元运算符
unaryOp: T_SUB;

// 基本表达式：括号表达式、整数、左值表达式
primaryExp: T_L_PAREN expr T_R_PAREN | T_DIGIT | lVal;

// 实参列表
realParamList: expr (T_COMMA expr)*;

// 左值表达式
lVal: T_ID;

// 用正规式来进行词法规则的描述

T_L_PAREN: '(';
T_R_PAREN: ')';
T_SEMICOLON: ';';
T_L_BRACE: '{';
T_R_BRACE: '}';

T_ASSIGN: '=';
T_COMMA: ',';

T_ADD: '+';
T_SUB: '-';

// 乘除模运算符
T_MUL: '*';
T_DIV: '/';
T_MOD: '%';
// 关系运算符
T_LT: '<';
T_GT: '>';
T_LE: '<=';
T_GE: '>=';
T_EQ: '==';
T_NE: '!=';

// 逻辑运算符
T_LAND: '&&';
T_LOR: '||';
T_LNOT: '!';
T_LOGIC_AND: '&&';
T_LOGIC_OR: '||';
T_LOGIC_NOT: '!';

// 要注意关键字同样也属于T_ID，因此必须放在T_ID的前面，否则会识别成T_ID
T_RETURN: 'return';
T_INT: 'int';
T_VOID: 'void';

// 控制流关键字
T_IF: 'if';
T_ELSE: 'else';
T_WHILE: 'while';
T_BREAK: 'break';
T_CONTINUE: 'continue';

T_ID: [a-zA-Z_][a-zA-Z0-9_]*;

// 修改T_DIGIT规则，匹配不同进制
T_DIGIT: '0' [xX] [0-9a-fA-F]+   // 十六进制
       | '0' [0-7]+              // 八进制
       | [1-9][0-9]*             // 十进制
       | '0'                     // 单独处理0
       ;

/* 空白符丢弃 */
WS: [ \r\n\t]+ -> skip;
/* 注释处理 */
COMMENT: '//' ~[\r\n]* -> skip;
BLOCK_COMMENT: '/*' .*? '*/' -> skip;