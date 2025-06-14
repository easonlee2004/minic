///
/// @file Antlr4CSTVisitor.cpp
/// @brief Antlr4的具体语法树的遍历产生AST
/// @author zenglj (zenglj@live.com)
/// @version 1.1
/// @date 2024-11-23
///
/// @copyright Copyright (c) 2024
///
/// @par 修改日志:
/// <table>
/// <tr><th>Date       <th>Version <th>Author  <th>Description
/// <tr><td>2024-09-29 <td>1.0     <td>zenglj  <td>新建
/// <tr><td>2024-11-23 <td>1.1     <td>zenglj  <td>表达式版增强
/// </table>
///

#include <string>

#include "Antlr4CSTVisitor.h"
#include "AST.h"
#include "AttrType.h"

#define Instanceof(res, type, var) auto res = dynamic_cast<type>(var)

/// @brief 构造函数
MiniCCSTVisitor::MiniCCSTVisitor()
{}

/// @brief 析构函数
MiniCCSTVisitor::~MiniCCSTVisitor()
{}

/// @brief 遍历CST产生AST
/// @param root CST语法树的根结点
/// @return AST的根节点
ast_node * MiniCCSTVisitor::run(MiniCParser::CompileUnitContext * root)
{
    return std::any_cast<ast_node *>(visitCompileUnit(root));
}

/// @brief 非终结运算符compileUnit的遍历
/// @param ctx CST上下文
std::any MiniCCSTVisitor::visitCompileUnit(MiniCParser::CompileUnitContext * ctx)
{
    // compileUnit: (funcDef | varDecl)* EOF

    // 请注意这里必须先遍历全局变量后遍历函数。肯定可以确保全局变量先声明后使用的规则，但有些情况却不能检查出。
    // 事实上可能函数A后全局变量B后函数C，这时在函数A中是不能使用变量B的，需要报语义错误，但目前的处理不会。
    // 因此在进行语义检查时，可能追加检查行号和列号，如果函数的行号/列号在全局变量的行号/列号的前面则需要报语义错误
    // TODO 请追加实现。

    ast_node * temp_node;
    ast_node * compileUnitNode = create_contain_node(ast_operator_type::AST_OP_COMPILE_UNIT);

    // 可能多个变量，因此必须循环遍历
    for (auto varCtx: ctx->varDecl()) {

        // 变量函数定义
        temp_node = std::any_cast<ast_node *>(visitVarDecl(varCtx));
        (void) compileUnitNode->insert_son_node(temp_node);
    }

    // 可能有多个函数，因此必须循环遍历
    for (auto funcCtx: ctx->funcDef()) {

        // 变量函数定义
        temp_node = std::any_cast<ast_node *>(visitFuncDef(funcCtx));
        (void) compileUnitNode->insert_son_node(temp_node);
    }

    return compileUnitNode;
}

/// @brief 非终结运算符funcDef的遍历
/// @param ctx CST上下文
std::any MiniCCSTVisitor::visitFuncDef(MiniCParser::FuncDefContext * ctx)
{
    // 识别的文法产生式：funcDef : T_INT T_ID T_L_PAREN T_R_PAREN block;

    // 函数返回类型，终结符
    type_attr funcReturnType{BasicType::TYPE_INT, (int64_t) ctx->T_INT()->getSymbol()->getLine()};

    // 创建函数名的标识符终结符节点，终结符
    char * id = strdup(ctx->T_ID()->getText().c_str());

    var_id_attr funcId{id, (int64_t) ctx->T_ID()->getSymbol()->getLine()};

    // 形参结点目前没有，设置为空指针
    ast_node * formalParamsNode = nullptr;

    // 遍历block结点创建函数体节点，非终结符
    auto blockNode = std::any_cast<ast_node *>(visitBlock(ctx->block()));

    // 创建函数定义的节点，孩子有类型，函数名，语句块和形参(实际上无)
    // create_func_def函数内会释放funcId中指向的标识符空间，切记，之后不要再释放，之前一定要是通过strdup函数或者malloc分配的空间
    return create_func_def(funcReturnType, funcId, blockNode, formalParamsNode);
}

/// @brief 非终结运算符block的遍历
/// @param ctx CST上下文
std::any MiniCCSTVisitor::visitBlock(MiniCParser::BlockContext * ctx)
{
    // 识别的文法产生式：block : T_L_BRACE blockItemList? T_R_BRACE';
    if (!ctx->blockItemList()) {
        // 语句块没有语句

        // 为了方便创建一个空的Block节点
        return create_contain_node(ast_operator_type::AST_OP_BLOCK);
    }

    // 语句块含有语句

    // 内部创建Block节点，并把语句加入，这里不需要创建Block节点
    return visitBlockItemList(ctx->blockItemList());
}

/// @brief 非终结运算符blockItemList的遍历
/// @param ctx CST上下文
std::any MiniCCSTVisitor::visitBlockItemList(MiniCParser::BlockItemListContext * ctx)
{
    // 识别的文法产生式：blockItemList : blockItem +;
    // 正闭包 循环 至少一个blockItem
    auto block_node = create_contain_node(ast_operator_type::AST_OP_BLOCK);

    for (auto blockItemCtx: ctx->blockItem()) {

        // 非终结符，需遍历
        auto blockItem = std::any_cast<ast_node *>(visitBlockItem(blockItemCtx));

        // 插入到块节点中
        (void) block_node->insert_son_node(blockItem);
    }

    return block_node;
}

///
/// @brief 非终结运算符blockItem的遍历
/// @param ctx CST上下文
///
std::any MiniCCSTVisitor::visitBlockItem(MiniCParser::BlockItemContext * ctx)
{
    // 识别的文法产生式：blockItem : statement | varDecl
    if (ctx->statement()) {
        // 语句识别

        return visitStatement(ctx->statement());
    } else if (ctx->varDecl()) {
        return visitVarDecl(ctx->varDecl());
    }

    return nullptr;
}

/// @brief 非终结运算符statement中的遍历
/// @param ctx CST上下文
std::any MiniCCSTVisitor::visitStatement(MiniCParser::StatementContext * ctx)
{
    // 识别的文法产生式：statement: T_ID T_ASSIGN expr T_SEMICOLON  # assignStatement
    // | T_RETURN expr T_SEMICOLON # returnStatement
    // | block  # blockStatement
    // | expr ? T_SEMICOLON #expressionStatement;
    if (Instanceof(assignCtx, MiniCParser::AssignStatementContext *, ctx)) {
        return visitAssignStatement(assignCtx);
    } else if (Instanceof(returnCtx, MiniCParser::ReturnStatementContext *, ctx)) {
        return visitReturnStatement(returnCtx);
    } else if (Instanceof(blockCtx, MiniCParser::BlockStatementContext *, ctx)) {
        return visitBlockStatement(blockCtx);
    } else if (Instanceof(exprCtx, MiniCParser::ExpressionStatementContext *, ctx)) {
        return visitExpressionStatement(exprCtx);
    } else if (Instanceof(ifCtx, MiniCParser::IfStatementContext *, ctx)) {
        return visitIfStatement(ifCtx);
    } else if (Instanceof(whileCtx, MiniCParser::WhileStatementContext *, ctx)) {
        return visitWhileStatement(whileCtx);
    } else if (Instanceof(breakCtx, MiniCParser::BreakStatementContext *, ctx)) {
        return visitBreakStatement(breakCtx);
    } else if (Instanceof(continueCtx, MiniCParser::ContinueStatementContext *, ctx)) {
        return visitContinueStatement(continueCtx);
    }

    return nullptr;
}

///
/// @brief 非终结运算符statement中的returnStatement的遍历
/// @param ctx CST上下文
///
std::any MiniCCSTVisitor::visitReturnStatement(MiniCParser::ReturnStatementContext * ctx)
{
    // 识别的文法产生式：returnStatement -> T_RETURN expr T_SEMICOLON

    // 非终结符，表达式expr遍历
    auto exprNode = std::any_cast<ast_node *>(visitExpr(ctx->expr()));

    // 创建返回节点，其孩子为Expr
    return create_contain_node(ast_operator_type::AST_OP_RETURN, exprNode);
}

/// @brief 非终结运算符expr的遍历
/// @param ctx CST上下文
std::any MiniCCSTVisitor::visitExpr(MiniCParser::ExprContext * ctx)
{
    // 识别产生式：expr: addExp;

    return visitAddExp(ctx->addExp());
}

std::any MiniCCSTVisitor::visitAssignStatement(MiniCParser::AssignStatementContext * ctx)
{
    // 识别文法产生式：assignStatement: lVal T_ASSIGN expr T_SEMICOLON

    // 赋值左侧左值Lval遍历产生节点
    auto lvalNode = std::any_cast<ast_node *>(visitLVal(ctx->lVal()));

    // 赋值右侧expr遍历
    auto exprNode = std::any_cast<ast_node *>(visitExpr(ctx->expr()));

    // 创建一个AST_OP_ASSIGN类型的中间节点，孩子为Lval和Expr
    return ast_node::New(ast_operator_type::AST_OP_ASSIGN, lvalNode, exprNode, nullptr);
}

std::any MiniCCSTVisitor::visitBlockStatement(MiniCParser::BlockStatementContext * ctx)
{
    // 识别文法产生式 blockStatement: block

    return visitBlock(ctx->block());
}

std::any MiniCCSTVisitor::visitAddExp(MiniCParser::AddExpContext * ctx)
{
    // 识别的文法产生式：addExp : unaryExp (addOp unaryExp)*;

    if (ctx->addOp().empty()) {

        // 没有addOp运算符，则说明闭包识别为0，只识别了第一个非终结符unaryExp
        return visitUnaryExp(ctx->unaryExp()[0]);
    }

    ast_node *left, *right;

    auto opsCtxVec = ctx->addOp();

    for (int k = 0; k < (int) opsCtxVec.size(); k++) {

        // 获取运算符
        ast_operator_type op = std::any_cast<ast_operator_type>(visitAddOp(opsCtxVec[k]));

        if (k == 0) {

            // 左操作数
            left = std::any_cast<ast_node *>(visitUnaryExp(ctx->unaryExp()[k]));
        }

        // 右操作数
        right = std::any_cast<ast_node *>(visitUnaryExp(ctx->unaryExp()[k + 1]));

        // 新建结点作为下一个运算符的右操作符
        left = ast_node::New(op, left, right, nullptr);
    }

    return left;
}

std::any MiniCCSTVisitor::visitMulExp(MiniCParser::MulExpContext * ctx)
{
    if (ctx->mulOp().empty()) {
        return visitUnaryExp(ctx->unaryExp()[0]);
    }

    ast_node *left, *right;

    auto opsCtxVec = ctx->mulOp();

    for (int k = 0; k < (int) opsCtxVec.size(); k++) {

        // 获取运算符
        ast_operator_type op = std::any_cast<ast_operator_type>(visitMulOp(opsCtxVec[k]));

        if (k == 0) {
            // 左操作数
            left = std::any_cast<ast_node *>(visitUnaryExp(ctx->unaryExp()[k]));
        }

        // 右操作数
        right = std::any_cast<ast_node *>(visitUnaryExp(ctx->unaryExp()[k + 1]));

        // 新建结点作为下一个运算符的右操作符
        left = ast_node::New(op, left, right, nullptr);
    }

    return left;
}

std::any MiniCCSTVisitor::visitAddOp(MiniCParser::AddOpContext * ctx)
{
    // 识别的文法产生式：addOp : T_ADD | T_SUB

    if (ctx->T_ADD()) {
        return ast_operator_type::AST_OP_ADD;
    } else {
        return ast_operator_type::AST_OP_SUB;
    }
}

std::any MiniCCSTVisitor::visitMulOp(MiniCParser::MulOpContext * ctx)
{
    // 识别的文法产生式：mulOp : T_MUL | T_DIV | T_MOD
    if (ctx->T_MUL()) {
        return ast_operator_type::AST_OP_MUL;
    } else if (ctx->T_DIV()) {
        return ast_operator_type::AST_OP_DIV;
    } else {
        return ast_operator_type::AST_OP_MOD;
    }
}

std::any MiniCCSTVisitor::visitUnaryOp(MiniCParser::UnaryOpContext * ctx)
{
    return ast_operator_type::AST_OP_NEG;
}

std::any MiniCCSTVisitor::visitUnaryExp(MiniCParser::UnaryExpContext * ctx)
{
    // 识别文法产生式：unaryExp: primaryExp | T_ID T_L_PAREN realParamList? T_R_PAREN;

    if (ctx->primaryExp()) {
        // 普通表达式
        return visitPrimaryExp(ctx->primaryExp());
    } else if (ctx->T_ID()) {

        // 创建函数调用名终结符节点
        ast_node * funcname_node = ast_node::New(ctx->T_ID()->getText(), (int64_t) ctx->T_ID()->getSymbol()->getLine());

        // 实参列表
        ast_node * paramListNode = nullptr;

        // 函数调用
        if (ctx->realParamList()) {
            // 有参数
            paramListNode = std::any_cast<ast_node *>(visitRealParamList(ctx->realParamList()));
        }

        // 创建函数调用节点，其孩子为被调用函数名和实参，
        return create_func_call(funcname_node, paramListNode);
    } else {
        return nullptr;
    }
}

std::any MiniCCSTVisitor::visitPrimaryExp(MiniCParser::PrimaryExpContext * ctx)
{
    // 识别文法产生式 primaryExp: T_L_PAREN expr T_R_PAREN | T_DIGIT | lVal;

    ast_node * node = nullptr;

    if (ctx->T_DIGIT()) {
        // 无符号整型字面量
        // 识别 primaryExp: T_DIGIT

        std::string text = ctx->T_DIGIT()->getText();
        uint32_t val = 0;
        if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
            val = std::stoul(text, nullptr, 16); // 十六进制
        } else if (text.size() > 1 && text[0] == '0') {
            val = std::stoul(text, nullptr, 8);  // 八进制
        } else {
            val = std::stoul(text, nullptr, 10); // 十进制
        }
        int64_t lineNo = (int64_t) ctx->T_DIGIT()->getSymbol()->getLine();
        node = ast_node::New(digit_int_attr{val, lineNo});
    } else if (ctx->lVal()) {
        // 具有左值的表达式
        // 识别 primaryExp: lVal
        node = std::any_cast<ast_node *>(visitLVal(ctx->lVal()));
    } else if (ctx->expr()) {
        // 带有括号的表达式
        // primaryExp: T_L_PAREN expr T_R_PAREN
        node = std::any_cast<ast_node *>(visitExpr(ctx->expr()));
    }

    return node;
}

std::any MiniCCSTVisitor::visitLVal(MiniCParser::LValContext * ctx)
{
    // 识别文法产生式：lVal: T_ID;
    // 获取ID的名字
    auto varId = ctx->T_ID()->getText();

    // 获取行号
    int64_t lineNo = (int64_t) ctx->T_ID()->getSymbol()->getLine();

    return ast_node::New(varId, lineNo);
}

std::any MiniCCSTVisitor::visitVarDecl(MiniCParser::VarDeclContext * ctx)
{
    // varDecl: basicType varDef (T_COMMA varDef)* T_SEMICOLON;

    // 声明语句节点
    ast_node * stmt_node = create_contain_node(ast_operator_type::AST_OP_DECL_STMT);

    // 类型节点
    type_attr typeAttr = std::any_cast<type_attr>(visitBasicType(ctx->basicType()));

    for (auto & varCtx: ctx->varDef()) {
        // 变量名节点
        ast_node * id_node = std::any_cast<ast_node *>(visitVarDef(varCtx));

        // 创建类型节点
        ast_node * type_node = create_type_node(typeAttr);

        // 创建变量定义节点
        ast_node * decl_node = ast_node::New(ast_operator_type::AST_OP_VAR_DECL, type_node, id_node, nullptr);

        // 插入到变量声明语句
        (void) stmt_node->insert_son_node(decl_node);
    }

    return stmt_node;
}

std::any MiniCCSTVisitor::visitVarDef(MiniCParser::VarDefContext * ctx)
{
    // varDef: T_ID;

    auto varId = ctx->T_ID()->getText();

    // 获取行号
    int64_t lineNo = (int64_t) ctx->T_ID()->getSymbol()->getLine();

    return ast_node::New(varId, lineNo);
}

std::any MiniCCSTVisitor::visitBasicType(MiniCParser::BasicTypeContext * ctx)
{
    // basicType: T_INT;
    type_attr attr{BasicType::TYPE_VOID, -1};
    if (ctx->T_INT()) {
        attr.type = BasicType::TYPE_INT;
        attr.lineno = (int64_t) ctx->T_INT()->getSymbol()->getLine();
    }

    return attr;
}

std::any MiniCCSTVisitor::visitRealParamList(MiniCParser::RealParamListContext * ctx)
{
    // 识别的文法产生式：realParamList : expr (T_COMMA expr)*;

    auto paramListNode = create_contain_node(ast_operator_type::AST_OP_FUNC_REAL_PARAMS);

    for (auto paramCtx: ctx->expr()) {

        auto paramNode = std::any_cast<ast_node *>(visitExpr(paramCtx));

        paramListNode->insert_son_node(paramNode);
    }

    return paramListNode;
}

std::any MiniCCSTVisitor::visitExpressionStatement(MiniCParser::ExpressionStatementContext * ctx)
{
    // 识别文法产生式  expr ? T_SEMICOLON #expressionStatement;
    if (ctx->expr()) {
        // 表达式语句

        // 遍历expr非终结符，创建表达式节点后返回
        return visitExpr(ctx->expr());
    } else {
        // 空语句

        // 直接返回空指针，需要再把语句加入到语句块时要注意判断，空语句不要加入
        return nullptr;
    }
}

/// 逻辑与关系表达式

/// @brief 非终结符EqExp的分析
/// @param ctx CST上下文
/// @return AST的节点
std::any MiniCCSTVisitor::visitEqExp(MiniCParser::EqExpContext * ctx)
{
    // 识别的文法产生式：eqExp : relExp (eqOp relExp)*
    if (ctx->eqOp().empty()) {
        return visitRelExp(ctx->relExp()[0]);
    }

    ast_node *left, *right;

    auto opsCtxVec = ctx->eqOp();
    for (int k = 0; k < (int)opsCtxVec.size(); k++) {
        ast_operator_type op = std::any_cast<ast_operator_type>(visitEqOp(opsCtxVec[k]));

        if (k == 0) {
            left = std::any_cast<ast_node *>(visitRelExp(ctx->relExp()[k]));
        }

        right = std::any_cast<ast_node *>(visitRelExp(ctx->relExp()[k + 1]));
        left = ast_node::New(op, left, right, nullptr);
    }

    return left;
}

/// @brief 非终结符RelExp的分析
/// @param ctx CST上下文
/// @return AST的节点
std::any MiniCCSTVisitor::visitRelExp(MiniCParser::RelExpContext * ctx)
{
    // 识别的文法产生式：relExp : addExp (relOp addExp)*
    if (ctx->relOp().empty()) {
        return visitAddExp(ctx->addExp()[0]);
    }

    ast_node *left, *right;

    auto opsCtxVec = ctx->relOp();
    for (int k = 0; k < (int)opsCtxVec.size(); k++) {
        ast_operator_type op = std::any_cast<ast_operator_type>(visitRelOp(opsCtxVec[k]));

        if (k == 0) {
            left = std::any_cast<ast_node *>(visitAddExp(ctx->addExp()[k]));
        }

        right = std::any_cast<ast_node *>(visitAddExp(ctx->addExp()[k + 1]));
        left = ast_node::New(op, left, right, nullptr);
    }

    return left;
}

/// @brief 非终结符EqOp的分析
/// @param ctx CST上下文
/// @return AST操作符类型
std::any MiniCCSTVisitor::visitEqOp(MiniCParser::EqOpContext * ctx)
{
    // 识别的文法产生式：eqOp : T_EQ | T_NEQ
    if (ctx->T_EQ()) {
        return ast_operator_type::AST_OP_EQ;
    } else {
        return ast_operator_type::AST_OP_NEQ;
    }
}

/// @brief 非终结符RelOp的分析
/// @param ctx CST上下文
/// @return AST操作符类型
std::any MiniCCSTVisitor::visitRelOp(MiniCParser::RelOpContext * ctx)
{
    // 识别的文法产生式：relOp : T_LT | T_GT | T_LE | T_GE
    if (ctx->T_LT()) {
        return ast_operator_type::AST_OP_LT;
    } else if (ctx->T_GT()) {
        return ast_operator_type::AST_OP_GT;
    } else if (ctx->T_LE()) {
        return ast_operator_type::AST_OP_LE;
    } else {
        return ast_operator_type::AST_OP_GE;
    }
}

/// @brief 非终结符LogicOrExp的分析
/// @param ctx CST上下文
/// @return AST的节点
std::any MiniCCSTVisitor::visitLogicOrExp(MiniCParser::LogicOrExpContext * ctx)
{
    if (ctx->T_OR().empty()) {
        return visitLogicAndExp(ctx->logicAndExp()[0]);
    }

    ast_node *left, *right;

    for (int k = 0; k < (int)ctx->T_OR().size(); k++) {
        if (k == 0) {
            left = std::any_cast<ast_node *>(visitLogicAndExp(ctx->logicAndExp()[k]));
        }

        right = std::any_cast<ast_node *>(visitLogicAndExp(ctx->logicAndExp()[k + 1]));
        left = ast_node::New(ast_operator_type::AST_OP_LOGIC_OR, left, right, nullptr);
    }

    return left;
}

/// @brief 非终结符LogicAndExp的分析
/// @param ctx CST上下文
/// @return AST的节点
std::any MiniCCSTVisitor::visitLogicAndExp(MiniCParser::LogicAndExpContext * ctx)
{
    // 识别的文法产生式：logicAndExp : eqExp (T_AND eqExp)*
    if (ctx->T_AND().empty()) {
        return visitEqExp(ctx->eqExp()[0]);
    }

    ast_node *left, *right;

    for (int k = 0; k < (int)ctx->T_AND().size(); k++) {
        if (k == 0) {
            left = std::any_cast<ast_node *>(visitEqExp(ctx->eqExp()[k]));
        }

        right = std::any_cast<ast_node *>(visitEqExp(ctx->eqExp()[k + 1]));
        left = ast_node::New(ast_operator_type::AST_OP_LOGIC_AND, left, right, nullptr);
    }

    return left;
}

/// 控制流

std::any MiniCCSTVisitor::visitIfStatement(MiniCParser::IfStatementContext * ctx)
{
    // if语句
    // 获取条件表达式
    auto condition = std::any_cast<ast_node *>(visitExpr(ctx->expr()));
    
    // 获取 then 语句块
    auto thenBlock = std::any_cast<ast_node *>(visitStatement(ctx->statement(0)));
    
    if (ctx->T_ELSE()) {
        // if-else
        auto elseBlock = std::any_cast<ast_node *>(visitStatement(ctx->statement(1)));
        return ast_node::New(ast_operator_type::AST_OP_IF_ELSE, condition, thenBlock, elseBlock, nullptr);
    } else {
        // 单独的if无else
        return ast_node::New(ast_operator_type::AST_OP_IF, condition, thenBlock, nullptr);
    }
}

std::any MiniCCSTVisitor::visitWhileStatement(MiniCParser::WhileStatementContext * ctx)
{
    // while语句
    // 获取条件表达式
    auto condition = std::any_cast<ast_node *>(visitExpr(ctx->whileStatement()->expr()));

    // 循环体
    auto body = std::any_cast<ast_node *>(visitStatement(ctx->whileStatement()->statement()));

    // while节点
    return ast_node::New(ast_operator_type::AST_OP_WHILE, condition, body, nullptr);
}

std::any MiniCCSTVisitor::visitBreakStatement(MiniCParser::BreakStatementContext * ctx)
{
    // break语句
    return ast_node::New(ast_operator_type::AST_OP_BREAK, nullptr);
}

std::any MiniCCSTVisitor::visitContinueStatement(MiniCParser::ContinueStatementContext * ctx)
{
    // continue语句
    return ast_node::New(ast_operator_type::AST_OP_CONTINUE, nullptr);
}