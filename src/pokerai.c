#include "pokerai.h"

#define AI_WIN  1
#define AI_LOSE 0

//Close enough?
#define THREAD_ID ((unsigned int)pthread_self() % 100)

/*
 * Spawn Monte Carlo threads to simulate poker games
 * ai: the AI which should spawn the threads
 */
static
void SpawnMonteCarloThreads(PokerAI *ai);

/*
 * Simulate games for the given AI
 * _ai: a void pointer to a PokerAI pointer
 * return: NULL (pthread requirement)
 */
static
void *SimulateGames(void *_ai);

/*
 * Simulate a single poker game for the given AI
 * ai: the poker AI to simulate games for
 * return: 1 on AI win, 0 on AI lose
 */
static
int SimulateSingleGame(PokerAI *ai);

/*
 * Randomly draw a card from the deck
 * and remove that card from the deck
 * deck: the deck to draw a card from
 * psize: a pointer to the size of the deck
 * return: a random card from the deck
 */
static
int draw(int *deck, int *psize);

/*
 * Calculate the maximum opponent score
 * from the given list of opponent hands
 * opponents: an array of poker hands
 * numopponents: the number of opponents in the array
 * numcards: the size of each hand
 * return: the score of the best hand
 */
static
int BestOpponentHand(int **opponents, int numopponents, int numcards);

/*
 * Create a new PokerAI
 *
 * num_threads: number of threads this AI should use
 * timeout: how long (in milliseconds) each thread may simulate games
 * return a new PokerAI
 */
PokerAI *CreatePokerAI(int num_threads, int timeout)
{
    PokerAI *ai = malloc(sizeof(PokerAI));

    //Allocate worker thread members
    ai->num_threads = num_threads;
    ai->timeout = timeout;
    ai->threads = malloc(sizeof(pthread_t) * num_threads);
    pthread_mutex_init(&ai->mutex, NULL);

    return ai;
}

/*
 * Destroy the PokerAI and all associated memory
 * ai: the PokerAI to destroy
 */
void DestroyPokerAI(PokerAI *ai)
{
    if (!ai) return;

    pthread_mutex_destroy(&ai->mutex);
    free(ai->threads);
    free(ai);
}

/*
 * Set debug logging to the given level
 * and output to the given FILE
 * ai: the AI to set logging for
 * level: the logging level of output
 * file: the FILE where output should be logged
 */
void SetLogging(PokerAI *ai, LOGLEVEL level, FILE *file)
{
    ai->loglevel = level;
    ai->logfile = file;
}

/*
 * Update the given PokerAI's game state
 * ai: the PokerAI to update
 * new_state: JSON representation of the new game state
 */
void UpdateGameState(PokerAI *ai, cJSON *new_state)
{
    ai->action.type = ACTION_UNSET;
    SetGameState(&ai->game, new_state);
}

/*
 * Return whether or not it is the AI's turn to act
 * ai: the PokerAI to update
 * return: true if it is the AI's turn to act
 */
bool MyTurn(PokerAI *ai)
{
    return ai->game.your_turn;
}

/*
 * Use Monte Carlo simulation to determine the best action to take
 * ai: the AI that is predicting the best action
 * return: a string representation of the best action to take
 */
char *GetBestAction(PokerAI *ai)
{
    double winprob = 0;
    ai->games_won = 0;
    ai->games_simulated = 0;

    SpawnMonteCarloThreads(ai);
    winprob = ((double) ai->games_won) / ai->games_simulated;

    if (ai->loglevel == DEBUG)
    {
        fprintf(ai->logfile, "Simulated %d games.\n", ai->games_simulated);
        fprintf(ai->logfile, "Win probability: %lf\n", winprob);
    }

    //TODO: Make a good AI
    if (winprob > 0.5)
    {
        ai->action.type = BET;
        ai->action.amount = ai->game.stack * winprob;
    }
    else if (winprob > 0.25)
    {
        ai->action.type = CALL;
    }
    else
    {
        ai->action.type = FOLD;
    }

    return ActionGetString(&ai->action);
}

/*
 * Print the AI's decision to the given FILE
 * ai: the AI that is making the decision
 * file: the FILE where output should be written
 */
void WriteAction(PokerAI *ai, FILE *file)
{
    switch(ai->action.type)
    {
    case FOLD:
        fprintf(file, "ACTION:\tFOLDING\n");
        break;

    case CALL:
        fprintf(file, "ACTION:\tCALLING\n");
        break;

    case BET:
        fprintf(file, "ACTION:\tBETTING %d\n", ai->action.amount);
        break;

    default:
        fprintf(file, "No action set\n");
        break;
    }
}

/*
 * Spawn Monte Carlo threads to simulate poker games
 * ai: the AI which should spawn the threads
 */
static
void SpawnMonteCarloThreads(PokerAI *ai)
{
    if (ai->loglevel == DEBUG)
    {
        fprintf(ai->logfile, "Spawning Monte Carlo threads.\n");
    }

    //Spawn threads to perform Monte Carlo simulations
    for (int i = 0; i < ai->num_threads; i++)
    {
        pthread_create(&ai->threads[i], NULL, SimulateGames, ai);
    }

    //Wait until each thread has finished simulating games
    for (int i = 0; i < ai->num_threads; i++)
    {
        pthread_join(ai->threads[i], NULL);
    }

    if (ai->loglevel == DEBUG)
    {
        fprintf(ai->logfile, "All Monte Carlo threads finished.\n");
    }
}

/*
 * Simulate games for the given AI
 * _ai: a void pointer to a PokerAI pointer
 * return: NULL (pthread requirement)
 */
static
void *SimulateGames(void *_ai)
{
    PokerAI *ai = (PokerAI *)_ai;
    Timer timer;

    if (ai->loglevel == DEBUG)
    {
        fprintf(ai->logfile, "[Thread %u] starting\n", THREAD_ID);
    }

    int simulated = 0;
    int won = 0;

    StartTimer(&timer);
    //Only check the timer after every 5 simulations
    while (1)
    {
        if (simulated % 1000 == 0 && GetElapsedTime(&timer) > ai->timeout)
        {
            break;
        }

        won += SimulateSingleGame(ai);
        simulated++;
    }

    if (ai->loglevel == DEBUG)
    {
        fprintf(ai->logfile, "[Thread %u] done\t(simulated %d games)\n", THREAD_ID, simulated);
    }

    //Lock the AI mutex and update the totals
    pthread_mutex_lock(&ai->mutex);
    ai->games_won += won;
    ai->games_simulated += simulated;
    pthread_mutex_unlock(&ai->mutex);

    return NULL;
}

/*
 * Simulate a single poker game for the given AI
 * ai: the poker AI to simulate games for
 * return: AI_WIN on AI win or AI_LOSE on AI lose
 */
static
int SimulateSingleGame(PokerAI *ai)
{
    GameState *game = &ai->game;
    int me[NUM_HAND + NUM_COMMUNITY];
    int *opponents[MAX_OPPONENTS];
    int community[NUM_COMMUNITY];
    int myscore;
    int bestopponent;

    for (int i = 0; i < game->num_opponents; i++)
    {
        opponents[i] = malloc(sizeof(*opponents[i]) * (NUM_HAND + NUM_COMMUNITY));
    }

    //Create a deck as a randomized queue data structure
    int deck[52] = {0};
    int decksize = 0;
    for (int i = 0; i < NUM_DECK; i++)
    {
        if (game->deck[i])
        {
            //Cards are 1 indexed
            deck[decksize] = i + 1;
            decksize++;
        }
    }

    //Add the known community cards to the simulation community list
    for (int i = 0; i < game->communitysize; i++)
    {
        community[i] = game->community[i];
    }

    //Distribute the rest of the community cards
    for (int i = game->communitysize; i < NUM_COMMUNITY; i++)
    {
        community[i] = draw(deck, &decksize);
    }

    //Give each opponent their cards
    for (int opp = 0; opp < game->num_opponents; opp++)
    {
        //Personal cards
        for (int i = 0; i < NUM_HAND; i++)
        {
            opponents[opp][i] = draw(deck, &decksize);
        }

        //Community cards
        for (int i = NUM_HAND; i < NUM_COMMUNITY + NUM_HAND; i++)
        {
            opponents[opp][i] = community[i - NUM_HAND];
        }
    }

    //Put my cards into a new array
    for (int i = 0; i < NUM_HAND; i++)
    {
        me[i] = game->hand[i];
    }
    for (int i = NUM_HAND; i < NUM_HAND + NUM_COMMUNITY; i++)
    {
        me[i] = community[i - NUM_HAND];
    }

    //See who won
    myscore = GetHandValue(me, NUM_HAND + NUM_COMMUNITY);
    bestopponent = BestOpponentHand(opponents, game->num_opponents, NUM_HAND + NUM_COMMUNITY);
    if (myscore > bestopponent)
    {
        return AI_WIN;
    }
    else
    {
        return AI_LOSE;
    }
}

/*
 * Randomly draw a card from the deck
 * and remove that card from the deck
 * deck: the deck to draw a card from
 * psize: a pointer to the size of the deck
 * return: a random card from the deck
 */
static
int draw(int *deck, int *psize)
{
    int index = rand() % *psize;
    int value = deck[index];
    deck[index] = deck[*psize - 1];
    *psize -= 1;

    return value;
}

/*
 * Calculate the maximum opponent score
 * from the given list of opponent hands
 * opponents: an array of poker hands
 * numopponents: the number of opponents in the array
 * numcards: the size of each hand
 * return: the score of the best hand
 */
static
int BestOpponentHand(int **opponents, int numopponents, int numcards)
{
    int max = 0;
    int score;
    for (int i = 0; i < numopponents; i++)
    {
        score = GetHandValue(opponents[i], numcards);
        if (score > max)
        {
            max = score;
        }
    }

    return max;
}
