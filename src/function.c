
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "utilities.h"
#include "function.h"
#include "evaluate.h"
#include "error.h"
#include "resolve.h"

builtInFunction_t builtInFunctionSet[] = {
    {{FUNCTION_TYPE_BUILT_IN, 2}, (int8_t *)"SET", BUILT_IN_FUNCTION_SET},
    {{FUNCTION_TYPE_BUILT_IN, 2}, (int8_t *)"TYPE", BUILT_IN_FUNCTION_TYPE},
    {{FUNCTION_TYPE_BUILT_IN, 3}, (int8_t *)"CONVERT", BUILT_IN_FUNCTION_CONVERT},
    {{FUNCTION_TYPE_BUILT_IN, 2}, (int8_t *)"SIZE", BUILT_IN_FUNCTION_SIZE},
    {{FUNCTION_TYPE_BUILT_IN, 2}, (int8_t *)"RESIZE", BUILT_IN_FUNCTION_RESIZE},
    {{FUNCTION_TYPE_BUILT_IN, 2}, (int8_t *)"IF", BUILT_IN_FUNCTION_IF},
    {{FUNCTION_TYPE_BUILT_IN, 1}, (int8_t *)"LOOP", BUILT_IN_FUNCTION_LOOP},
    {{FUNCTION_TYPE_BUILT_IN, 2}, (int8_t *)"THROW", BUILT_IN_FUNCTION_THROW},
    {{FUNCTION_TYPE_BUILT_IN, 3}, (int8_t *)"CATCH", BUILT_IN_FUNCTION_CATCH},
    {{FUNCTION_TYPE_BUILT_IN, 1}, (int8_t *)"PRINT", BUILT_IN_FUNCTION_PRINT},
    {{FUNCTION_TYPE_BUILT_IN, 1}, (int8_t *)"PROMPT", BUILT_IN_FUNCTION_PROMPT}
};

hyperValueList_t emptyArgumentList = {NULL, 0};

builtInFunction_t *findBuiltInFunctionByName(int8_t *name) {
    int32_t tempLength = sizeof(builtInFunctionSet) / sizeof(*builtInFunctionSet);
    for (int32_t index = 0; index < tempLength; index++) {
        builtInFunction_t *tempFunction = builtInFunctionSet + index;
        if (strcmp((char *)name, (char *)tempFunction->name) == 0) {
            return tempFunction;
        }
    }
    return NULL;
}

void checkInvocationArgumentList(baseFunction_t *function, hyperValueList_t **argumentList) {
    if (*argumentList == NULL) {
        *argumentList = &emptyArgumentList;
    }
    int32_t argumentAmount = function->argumentAmount;
    if ((*argumentList)->length > argumentAmount) {
        if (argumentAmount == 1) {
            THROW_BUILT_IN_ERROR(DATA_ERROR_CONSTANT, "Expected at most 1 argument.");
        } else {
            THROW_BUILT_IN_ERROR(
                DATA_ERROR_CONSTANT,
                "Expected at most %d arguments.",
                argumentAmount
            );
        }
    }
}

hyperValue_t getArgument(hyperValueList_t *argumentList, int32_t index) {
    if (index < argumentList->length) {
        return argumentList->valueArray[index];
    }
    hyperValue_t tempValue;
    tempValue.type = HYPER_VALUE_TYPE_VALUE;
    tempValue.value.type = VALUE_TYPE_VOID;
    return tempValue;
}

// Note: This will throw an error if the hyper value
// contains an alias with a bad index.
value_t getResolvedArgument(hyperValueList_t *argumentList, int32_t index) {
    hyperValue_t tempValue = getArgument(argumentList, index);
    return readValueFromHyperValue(tempValue);
}

double convertNativeTypeToConstant(int8_t nativeType) {
    if (nativeType == VALUE_TYPE_NUMBER) {
        return NUMBER_TYPE_CONSTANT;
    }
    if (nativeType == VALUE_TYPE_STRING) {
        return STRING_TYPE_CONSTANT;
    }
    if (nativeType == VALUE_TYPE_LIST) {
        return LIST_TYPE_CONSTANT;
    }
    if (nativeType == VALUE_TYPE_BUILT_IN_FUNCTION
            || nativeType == VALUE_TYPE_CUSTOM_FUNCTION) {
        return FUNCTION_TYPE_CONSTANT;
    }
    if (nativeType == VALUE_TYPE_VOID) {
        return VOID_TYPE_CONSTANT;
    }
    return -1;
}

value_t convertValueToType(value_t value, double typeConstant) {
    double tempTypeConstant = convertNativeTypeToConstant(value.type);
    if (tempTypeConstant == typeConstant) {
        return copyValue(value);
    }
    value_t output;
    output.type = VALUE_TYPE_VOID;
    if (typeConstant == NUMBER_TYPE_CONSTANT) {
        if (value.type == VALUE_TYPE_STRING) {
            int8_t *tempText = value.heapValue->vector.data;
            double tempNumber;
            int32_t tempResult = sscanf((char *)tempText, "%lf", &tempNumber);
            if (tempResult < 1) {
                THROW_BUILT_IN_ERROR(
                    DATA_ERROR_CONSTANT,
                    "Malformed number string."
                );
                return output;
            }
            output.type = VALUE_TYPE_NUMBER;
            output.numberValue = tempNumber;
            return output;
        }
        THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Cannot convert value to number.");
        return output;
    }
    if (typeConstant == STRING_TYPE_CONSTANT) {
        return convertValueToString(value, true);
    }
    if (typeConstant == LIST_TYPE_CONSTANT) {
        // I thought about string to list conversion on a character basis, but
        // it wouldn't make sense in the context of list to string conversion.
        // It also wouldn't be very useful.
        THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Cannot convert value to list.");
        return output;
    }
    if (typeConstant == FUNCTION_TYPE_CONSTANT) {
        THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Cannot convert value to function.");
        return output;
    }
    if (typeConstant == VOID_TYPE_CONSTANT) {
        THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Cannot convert value to void.");
        return output;
    }
    THROW_BUILT_IN_ERROR(NUMBER_ERROR_CONSTANT, "Invalid type constant.");
    return output;
}

void invokeBuiltInFunction(
    builtInFunction_t *builtInFunction,
    hyperValueList_t *argumentList
) {
    checkInvocationArgumentList(&(builtInFunction->base), &argumentList);
    if (hasThrownError) {
        return;
    }
    switch (builtInFunction->number) {
        case BUILT_IN_FUNCTION_SET:
        {
            value_t tempValue = getResolvedArgument(argumentList, 1);
            if (hasThrownError) {
                return;
            }
            writeValueToLocation(getArgument(argumentList, 0), tempValue);
            break;
        }
        case BUILT_IN_FUNCTION_TYPE:
        {
            value_t tempValue = getResolvedArgument(argumentList, 1);
            if (hasThrownError) {
                return;
            }
            value_t tempResult;
            tempResult.type = VALUE_TYPE_NUMBER;
            tempResult.numberValue = convertNativeTypeToConstant(tempValue.type);
            writeValueToLocation(getArgument(argumentList, 0), tempResult);
            break;
        }
        case BUILT_IN_FUNCTION_CONVERT:
        {
            value_t tempValue = getResolvedArgument(argumentList, 1);
            value_t tempType = getResolvedArgument(argumentList, 2);
            if (hasThrownError) {
                return;
            }
            if (tempType.type != VALUE_TYPE_NUMBER) {
                THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Type must be a number.");
                return;
            }
            value_t tempResult = convertValueToType(tempValue, tempType.numberValue);
            if (hasThrownError) {
                return;
            }
            writeValueToLocation(getArgument(argumentList, 0), tempResult);
            break;
        }
        case BUILT_IN_FUNCTION_IF:
        {
            value_t tempCondition = getResolvedArgument(argumentList, 0);
            value_t tempHandle = getResolvedArgument(argumentList, 1);
            if (hasThrownError) {
                return;
            }
            if (tempCondition.type != VALUE_TYPE_NUMBER) {
                THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Condition must be a number.");
                return;
            }
            if (tempCondition.numberValue != 0) {
                invokeFunction(tempHandle, NULL, false);
            }
            break;
        }
        case BUILT_IN_FUNCTION_LOOP:
        {
            value_t tempHandle = getResolvedArgument(argumentList, 0);
            while (!hasThrownError) {
                invokeFunction(tempHandle, NULL, false);
            }
            break;
        }
        case BUILT_IN_FUNCTION_THROW:
        {
            value_t tempChannel = getResolvedArgument(argumentList, 0);
            value_t tempValue = getResolvedArgument(argumentList, 1);
            if (hasThrownError) {
                return;
            }
            if (tempChannel.type != VALUE_TYPE_NUMBER) {
                THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Channel must be a number.");
                return;
            }
            throwError((int32_t)(tempChannel.numberValue), tempValue);
            break;
        }
        case BUILT_IN_FUNCTION_CATCH:
        {
            hyperValue_t tempDestination = getArgument(argumentList, 0);
            value_t tempChannel = getResolvedArgument(argumentList, 1);
            value_t tempHandle = getResolvedArgument(argumentList, 2);
            if (hasThrownError) {
                return;
            }
            if (tempChannel.type != VALUE_TYPE_NUMBER) {
                THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Channel must be a number.");
                return;
            }
            invokeFunction(tempHandle, NULL, false);
            if (hasThrownError) {
                if ((int32_t)(tempChannel.numberValue) == thrownErrorChannel) {
                    hasThrownError = false;
                    writeValueToLocation(tempDestination, thrownErrorValue);
                    unlockValue(&thrownErrorValue);
                }
            } else {
                value_t tempValue;
                tempValue.type = VALUE_TYPE_VOID;
                writeValueToLocation(tempDestination, tempValue);
            }
            break;
        }
        case BUILT_IN_FUNCTION_PRINT:
        {
            value_t tempValue = getResolvedArgument(argumentList, 0);
            if (hasThrownError) {
                return;
            }
            value_t stringValue = convertValueToString(tempValue, false);
            printf("%s\n", stringValue.heapValue->vector.data);
            deleteValueIfUnreferenced(&stringValue);
            break;
        }
        // TODO: Implement more built-in functions.
        
        default:
        {
            break;
        }
    }
}

heapValue_t *createFunctionHandle(heapValue_t *frame, customFunction_t *customFunction) {
    customFunctionHandle_t *tempHandle = malloc(sizeof(customFunctionHandle_t));
    tempHandle->function = customFunction;
    scope_t *tempScope = &(customFunction->scope);
    int32_t tempLength = tempScope->parentVariableAmount;
    hyperValue_t *tempLocationArray = malloc(sizeof(hyperValue_t) * tempLength);
    tempHandle->locationList.valueArray = tempLocationArray;
    tempHandle->locationList.length = tempLength;
    int32_t scopeIndex = 0;
    int32_t locationIndex = 0;
    while (scopeIndex < tempScope->variableList.length && locationIndex < tempLength) {
        scopeVariable_t *tempVariable;
        getVectorElement(&tempVariable, &(tempScope->variableList), scopeIndex);
        scopeIndex += 1;
        if (tempVariable->parentScopeIndex < 0) {
            continue;
        }
        hyperValue_t tempLocation = getFrameVariableLocation(
            frame,
            tempVariable->parentScopeIndex
        );
        tempLocationArray[locationIndex] = tempLocation;
        addHyperValueReference(&tempLocation);
        locationIndex += 1;
    }
    heapValue_t *output = createHeapValue(VALUE_TYPE_CUSTOM_FUNCTION);
    output->customFunctionHandle = tempHandle;
    return output;
}

heapValue_t *functionHandleCreateFrame(customFunctionHandle_t *functionHandle) {
    scope_t *tempScope = &(functionHandle->function->scope);
    hyperValue_t *tempLocationArray = functionHandle->locationList.valueArray;
    int32_t tempLength = (int32_t)(tempScope->variableList.length);
    hyperValue_t *tempValueArray = malloc(sizeof(hyperValue_t) * tempLength);
    for (int32_t index = 0; index < tempLength; index++) {
        hyperValue_t tempValue;
        tempValue.type = HYPER_VALUE_TYPE_VALUE;
        tempValue.value.type = VALUE_TYPE_VOID;
        tempValueArray[index] = tempValue;
    }
    int32_t scopeIndex = 0;
    int32_t locationIndex = 0;
    while (scopeIndex < tempLength) {
        scopeVariable_t *scopeVariable;
        getVectorElement(&scopeVariable, &(tempScope->variableList), scopeIndex);
        if (scopeVariable->parentScopeIndex >= 0) {
            hyperValue_t *tempLocation = tempLocationArray + locationIndex;
            swapHyperValueReference(tempValueArray + scopeIndex, tempLocation);
            locationIndex += 1;
        }
        scopeIndex += 1;
    }
    heapValue_t *output = createHeapValue(VALUE_TYPE_FRAME);
    output->frameVariableList.valueArray = tempValueArray;
    output->frameVariableList.length = tempLength;
    return output;
}

// Output will be locked.
heapValue_t *invokeFunctionHandle(
    heapValue_t *functionHandle,
    hyperValueList_t *argumentList
) {
    customFunctionHandle_t *tempHandle = functionHandle->customFunctionHandle;
    customFunction_t *customFunction = tempHandle->function;
    checkInvocationArgumentList(&(customFunction->base), &argumentList);
    if (hasThrownError) {
        return NULL;
    }
    heapValue_t *tempFrame = functionHandleCreateFrame(tempHandle);
    lockHeapValue(tempFrame);
    for (int32_t index = 0; index < argumentList->length; index++) {
        swapHyperValueReference(
            tempFrame->frameVariableList.valueArray + index,
            argumentList->valueArray + index
        );
    }
    for (int64_t index = 0; index < customFunction->statementList.length; index++) {
        baseStatement_t *tempStatement;
        getVectorElement(&tempStatement, &(customFunction->statementList), index);
        evaluateStatement(tempFrame, tempStatement);
        if (hasThrownError) {
            addBodyPosToStackTrace(&(tempStatement->bodyPos));
            break;
        }
    }
    return tempFrame;
}

// Output will be locked.
hyperValue_t invokeFunction(
    value_t functionValue,
    hyperValueList_t *argumentList,
    int8_t hasDestinationArgument
) {
    hyperValue_t output;
    output.type = HYPER_VALUE_TYPE_VALUE;
    output.value.type = VALUE_TYPE_VOID;
    if (functionValue.type == VALUE_TYPE_BUILT_IN_FUNCTION) {
        if (hasDestinationArgument) {
            hyperValue_t *tempLocation = argumentList->valueArray + 0;
            tempLocation->type = HYPER_VALUE_TYPE_POINTER;
            tempLocation->valuePointer = &(output.value);
        }
        invokeBuiltInFunction(functionValue.builtInFunction, argumentList);
        lockValue(&(output.value));
        removeValueReference(&(output.value));
        return output;
    }
    if (functionValue.type == VALUE_TYPE_CUSTOM_FUNCTION) {
        heapValue_t *tempFrame = invokeFunctionHandle(functionValue.heapValue, argumentList);
        if (hasDestinationArgument) {
            output = getFrameVariableLocation(tempFrame, 0);
            lockHyperValue(&output);
        }
        unlockHeapValue(tempFrame);
        return output;
    }
    THROW_BUILT_IN_ERROR(TYPE_ERROR_CONSTANT, "Expected function handle.");
    return output;
}


