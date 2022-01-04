#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "ControlPointStore.h"

#define PROMPT_MAX_CONTROLS 64

#define DAY_TIME(_week, _day, _min) (((_week) * 7 * 1440 + (((_day) + 2) % 7) * 1440 + (_min)) * 60ul)

typedef struct
{
    int lineNumber;
    Pinetime::Controllers::ControlPointStore store;

    // config
   	unsigned short version;
	unsigned short windowSize;
	unsigned short minimumInterval;
    Pinetime::Controllers::control_point_packed_t controlPoints[PROMPT_MAX_CONTROLS];
    Pinetime::Controllers::control_point_packed_t scratch[PROMPT_MAX_CONTROLS];

    unsigned short lastFakeWeek;
    unsigned short lastDayOfWeek;
    unsigned short lastTimeOfDay;

    int success;
    int fails;
} test_state_t;

int processLine(test_state_t *state, const char *line)
{
    printf("\n%d> %s -- ", state->lineNumber, line);

    if (!strncmp(line, "RESET", 5))
    {
        state->store.Reset();
    }
    else if (!strncmp(line, "SET", 3))
    {
        int index;
        unsigned int days, timeMinutes;
        unsigned int value;
        const int volume = 0;
        if (sscanf(line, "SET %u %u %u %u", &index, &days, &timeMinutes, &value) != 4)
        {
            fprintf(stderr, "ERROR: Unable to parse 'SET' command (index, days, time, value): %s\n", line);
            return -1;
        }
        unsigned int time = timeMinutes * 60;
        Pinetime::Controllers::ControlPoint controlPoint = Pinetime::Controllers::ControlPoint(true, days, value, volume, time);
        state->store.SetScratch(index, controlPoint);
    }
    else if (!strncmp(line, "CLEAR", 5))
    {
        state->store.ClearScratch();
    }
    else if (!strncmp(line, "SAVE", 4))
    {
        unsigned int version = 0;
        if (sscanf(line, "SAVE %u", &version) != 1)
        {
            fprintf(stderr, "ERROR: Unable to parse 'SAVE' command (version): %s\n", line);
            return -1;
        }
        state->store.CommitScratch(version);
    }
    else if (!strncmp(line, "CUETEST", 4))
    {
        unsigned int day, timeMinutes;
        int value;
        if (sscanf(line, "CUETEST %u %u %d", &day, &timeMinutes, &value) != 3)
        {
            fprintf(stderr, "ERROR: Unable to parse 'CUETEST' command (day, time, value): %s\n", line);
            return -1;
        }

        unsigned int time = timeMinutes * 60;
        Pinetime::Controllers::ControlPoint controlPoint = state->store.CueValue(day, time);
        unsigned int cueValue = controlPoint.GetInterval();
        if (cueValue == value || (!controlPoint.IsEnabled() && value < 0))
        {
            state->success++;
        }
        else
        {
            printf("FAIL @%d: Mismatch on expected CUETEST value. Got %u, expected %u (day %u, time %u)\n", state->lineNumber, cueValue, value, day, timeMinutes);
            state->fails++;
        }

        // Increment week if otherwise would go back in time
        if (day < state->lastDayOfWeek || (day == state->lastDayOfWeek && time < state->lastTimeOfDay)) {
            state->lastFakeWeek++;
        }
        state->lastDayOfWeek = day;
        state->lastTimeOfDay = time;
    }
    else if (!strncmp(line, "BREAK", 5))
    {
        fprintf(stderr, "WARNING: Will terminate early at: %s\n", line);
        return 1;
    }
    else if (!strncmp(line, "#", 1) || line[0] == '\r' || line[0] == '\n' || line[0] == '\0')
    {
        // Ignore blank lines and comments
    }
    else
    {
        fprintf(stderr, "ERROR: Unhandled command line: %s\n", line);
        return -1;
    }

    return 0;
}

int tests(const char *testFile)
{
    test_state_t state = {0};

    // Clear store
    state.store.SetData(Pinetime::Controllers::ControlPointStore::VERSION_NONE, state.controlPoints, state.scratch, sizeof(state.controlPoints) / sizeof(state.controlPoints[0]));

    FILE *fp = fopen(testFile, "rt");
    if (fp == NULL)
    {
        fprintf(stderr, "ERROR: Unable to open input file: %s\n", testFile);
        return -1;
    }

    char line[4 * 1024 + 1];
    int lineRet = 0;
    while (fgets(line, sizeof(line) / sizeof(char), fp) != NULL)
    {
        state.lineNumber++;
        if (strlen(line) > 0 && line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
        if (strlen(line) > 0 && line[strlen(line) - 1] == '\r') line[strlen(line) - 1] = '\0';
        lineRet = processLine(&state, line);
        if (lineRet != 0)
        {
            fprintf(stderr, "ERROR: Stopping on error, line %d\n", state.lineNumber);
            break;
        }
    }
    fclose(fp);

    if (lineRet == 0)
    {
        if (state.fails > 0)
        {
            fprintf(stderr, "\n\033[31mERROR: %d/%d tests failed\033[0m\n", state.fails, state.success + state.fails);
            return -1;
        }
        else
        {
            printf("\n\033[32mSUCCESS: All %d tests passed!\033[0m\n", state.success);
        }
    }
    
    return lineRet;
}

int main(int argc, char *argv[])
{
    bool help = false;
    const char *testFile = "tests.txt";
    for (int i = 1, positional = 0; i < argc; i++)
    {
        if (!strcmp(argv[i], "-input"))
        {
            testFile = argv[++i];
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "ERROR: Unrecognized parameter: %s\n", argv[i]);
            help = true;
        }
        else
        {
            if (positional == 0)
            {
                testFile = argv[i];
            }
            else
            {
                fprintf(stderr, "ERROR: Unexpected positional parameter %d: %s\n", i + 1, argv[i]);
                help = true;
            }
            positional++;
        }
    }

    if (help)
    {
        fprintf(stderr, "Usage:  test [[-input] <inputfile.csv>]");
        return -1;
    }

    return tests(testFile);
}
