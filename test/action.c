#include "action.h"

/*
 * Set the action to fold
 * action: the Action to set
 */
void ActionSetFold(Action *action)
{
    action->type = FOLD;
    action->amount = 0;
}

/*
 * Set the action to call
 * action: the Action to set
 */
void ActionSetCall(Action *action)
{
    action->type = CALL;
    action->amount = 0;
}

/*
 * Set the action to bet the given amount
 * action: the Action to set
 * amount: the size of the bet
 */
void ActionSetBet(Action *action, int amount)
{
    action->type = BET;
    action->amount = amount;
}

/*
 * Get the string representation of the action
 * action: the Action to access
 */
char *ActionGetString(Action *action)
{
    switch (action->type)
    {
    case FOLD:
        sprintf(action->string, "action_name=fold");
        break;

    case CALL:
        sprintf(action->string, "action_name=call");
        break;

    case BET:
        sprintf(action->string, "action_name=bet&amount=%d", action->amount);
        break;

    default:
        //Should not be here! Action name has not been set
        sprintf(action->string, "action_name=fold");
        break;
    }

    return action->string;
}
