
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "utilities.h"
#include "resolve.h"
#include "expression.h"
#include "statement.h"
#include "function.h"
#include "variable.h"
#include "error.h"

numberConstant_t numberConstantSet[] = {
    {(int8_t *)"TRUE", 1},
    {(int8_t *)"FALSE", 0},
    {(int8_t *)"NUMBER_TYPE", NUMBER_TYPE_CONSTANT},
    {(int8_t *)"STRING_TYPE", STRING_TYPE_CONSTANT},
    {(int8_t *)"LIST_TYPE", LIST_TYPE_CONSTANT},
    {(int8_t *)"FUNCTION_TYPE", FUNCTION_TYPE_CONSTANT},
    {(int8_t *)"VOID_TYPE", VOID_TYPE_CONSTANT},
    {(int8_t *)"ERROR_CHANNEL", ERROR_CHANNEL_CONSTANT}
};

vector_t numberConstantList;

void initializeNumberConstants() {
    createEmptyVector(&numberConstantList, sizeof(numberConstant_t));
    int32_t tempLength = sizeof(numberConstantSet) / sizeof(*numberConstantSet);
    for (int32_t index = 0; index < tempLength; index++) {
        numberConstant_t *tempConstant = numberConstantSet + index;
        pushVectorElement(&numberConstantList, tempConstant);
    }
    addErrorConstantsToNumberConstants(&numberConstantList);
}

numberConstant_t *findNumberConstantByName(int8_t *name) {
    for (int32_t index = 0; index < numberConstantList.length; index++) {
        numberConstant_t *tempConstant = findVectorElement(&numberConstantList, index);
        if (strcmp((char *)name, (char *)tempConstant->name) == 0) {
            return tempConstant;
        }
    }
    return NULL;
}

baseScopeVariable_t *resolveIdentifierInScope(scope_t *scope, int8_t *identifier) {
    baseScopeVariable_t *tempVariable = scopeFindVariable(scope, identifier, NULL);
    if (tempVariable != NULL) {
        return tempVariable;
    }
    if (scope->parentScope == NULL) {
        return NULL;
    }
    tempVariable = resolveIdentifierInScope(scope->parentScope, identifier);
    if (tempVariable == NULL) {
        return NULL;
    }
    if (tempVariable->type == SCOPE_VARIABLE_TYPE_IMPORT) {
        // Do not create parent variables for imported variables.
        // Imported variables are always global.
        return tempVariable;
    }
    return scopeAddParentVariable(scope, identifier, tempVariable->scopeIndex);
}

baseExpression_t *resolveIdentifierExpression(
    scope_t *scope,
    identifierExpression_t *identifierExpression
) {
    int8_t *tempName = identifierExpression->name;
    baseScopeVariable_t *tempVariable = resolveIdentifierInScope(scope, tempName);
    if (tempVariable != NULL) {
        return createVariableExpression(tempVariable);
    }
    builtInFunction_t *tempFunction = findBuiltInFunctionByName(tempName);
    if (tempFunction != NULL) {
        value_t tempValue;
        tempValue.type = VALUE_TYPE_BUILT_IN_FUNCTION;
        tempValue.builtInFunction = tempFunction;
        return createConstantExpression(tempValue);
    }
    numberConstant_t *tempConstant = findNumberConstantByName(tempName);
    if (tempConstant != NULL) {
        value_t tempValue;
        tempValue.type = VALUE_TYPE_NUMBER;
        tempValue.numberValue = tempConstant->value;
        return createConstantExpression(tempValue);
    }
    if (strcmp((char *)tempName, "VOID") == 0) {
        value_t tempValue;
        tempValue.type = VALUE_TYPE_VOID;
        return createConstantExpression(tempValue);
    }
    THROW_BUILT_IN_ERROR(
        PARSE_ERROR_CONSTANT,
        "Unknown identifier \"%s\".",
        tempName
    );
    return NULL;
}

baseExpression_t *resolveNamespaceExpression(
    scope_t *scope,
    binaryExpression_t *binaryExpression
) {
    if (binaryExpression->operand1->type != EXPRESSION_TYPE_IDENTIFIER
            || binaryExpression->operand2->type != EXPRESSION_TYPE_IDENTIFIER) {
        THROW_BUILT_IN_ERROR(PARSE_ERROR_CONSTANT, "Expected identifier.");
        return NULL;
    }
    identifierExpression_t *operand1 = (identifierExpression_t *)(binaryExpression->operand1);
    identifierExpression_t *operand2 = (identifierExpression_t *)(binaryExpression->operand2);
    int8_t *namespaceName = operand1->name;
    int8_t *variableName = operand2->name;
    script_t *tempScript = scope->script;
    namespace_t *tempNamespace = scriptFindNamespace(tempScript, namespaceName);
    if (tempNamespace == NULL) {
        THROW_BUILT_IN_ERROR(
            PARSE_ERROR_CONSTANT,
            "Unknown namespace \"%s\".",
            namespaceName
        );
        return NULL;
    }
    baseScopeVariable_t *tempVariable = scopeAddNamespaceVariable(
        &(tempScript->topLevelFunction->scope),
        variableName,
        tempNamespace
    );
    return createVariableExpression(tempVariable);
}

void resolveIdentifiersInExpression(
    scope_t *scope,
    baseExpression_t **expression
) {
    baseExpression_t *tempExpression = *expression;
    switch (tempExpression->type) {
        case EXPRESSION_TYPE_IDENTIFIER:
        {
            baseExpression_t *tempResult = resolveIdentifierExpression(
                scope,
                (identifierExpression_t *)tempExpression
            );
            if (tempResult != NULL) {
                *expression = tempResult;
            }
            break;
        }
        case EXPRESSION_TYPE_LIST:
        {
            listExpression_t *listExpression = (listExpression_t *)tempExpression;
            for (int64_t index = 0; index < listExpression->expressionList.length; index++) {
                baseExpression_t **tempElementExpression = findVectorElement(
                    &(listExpression->expressionList),
                    index
                );
                resolveIdentifiersInExpression(scope, tempElementExpression);
                if (hasThrownError) {
                    return;
                }
            }
            break;
        }
        case EXPRESSION_TYPE_UNARY:
        {
            unaryExpression_t *unaryExpression = (unaryExpression_t *)tempExpression;
            resolveIdentifiersInExpression(scope, &(unaryExpression->operand));
            break;
        }
        case EXPRESSION_TYPE_BINARY:
        {
            binaryExpression_t *binaryExpression = (binaryExpression_t *)tempExpression;
            if (binaryExpression->operator->number == OPERATOR_NAMESPACE) {
                baseExpression_t *tempResult = resolveNamespaceExpression(
                    scope,
                    binaryExpression
                );
                if (tempResult != NULL) {
                    *expression = tempResult;
                }
            } else {
                resolveIdentifiersInExpression(scope, &(binaryExpression->operand1));
                if (hasThrownError) {
                    return;
                }
                resolveIdentifiersInExpression(scope, &(binaryExpression->operand2));
            }
            break;
        }
        case EXPRESSION_TYPE_FUNCTION:
        {
            resolveIdentifiersInFunction(
                ((customFunctionExpression_t *)tempExpression)->customFunction
            );
            break;
        }
        case EXPRESSION_TYPE_INDEX:
        {
            indexExpression_t *indexExpression = (indexExpression_t *)tempExpression;
            resolveIdentifiersInExpression(scope, &(indexExpression->sequence));
            if (hasThrownError) {
                return;
            }
            resolveIdentifiersInExpression(scope, &(indexExpression->index));
            break;
        }
        case EXPRESSION_TYPE_INVOCATION:
        {
            invocationExpression_t *invocationExpression = (invocationExpression_t *)tempExpression;
            resolveIdentifiersInExpression(scope, &(invocationExpression->function));
            if (hasThrownError) {
                return;
            }
            for (int64_t index = 0; index < invocationExpression->argumentList.length; index++) {
                baseExpression_t **tempArgumentExpression = findVectorElement(
                    &(invocationExpression->argumentList),
                    index
                );
                resolveIdentifiersInExpression(scope, tempArgumentExpression);
                if (hasThrownError) {
                    return;
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }
    (*expression)->script = scope->script;
}

void resolveIdentifiersInStatement(scope_t *scope, baseStatement_t *statement) {
    switch(statement->type) {
        case STATEMENT_TYPE_INVOCATION:
        {
            invocationStatement_t *invocationStatement = (invocationStatement_t *)statement;
            resolveIdentifiersInExpression(scope, &(invocationStatement->function));
            if (hasThrownError) {
                return;
            }
            for (int32_t index = 0; index < invocationStatement->argumentList.length; index++) {
                baseExpression_t **tempExpression = findVectorElement(
                    &(invocationStatement->argumentList),
                    index
                );
                resolveIdentifiersInExpression(scope, tempExpression);
                if (hasThrownError) {
                    return;
                }
            }
            break;
        }
        case STATEMENT_TYPE_NAMESPACE_IMPORT:
        case STATEMENT_TYPE_VARIABLE_IMPORT:
        {
            baseImportStatement_t *importStatement = (baseImportStatement_t *)statement;
            resolveIdentifiersInExpression(scope, &(importStatement->path));
            break;
        }
        default:
        {
            break;
        }
    }
}

void resolveIdentifiersInFunction(customFunction_t *function) {
    for (int64_t index = 0; index < function->statementList.length; index++) {
        baseStatement_t *tempStatement;
        getVectorElement(&tempStatement, &(function->statementList), index);
        resolveIdentifiersInStatement(&(function->scope), tempStatement);
        if (hasThrownError) {
            if (getStackTraceLength() <= 0) {
                addBodyPosToStackTrace(&(tempStatement->bodyPos));
            }
            return;
        }
    }
}


