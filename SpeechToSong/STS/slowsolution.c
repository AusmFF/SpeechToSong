typedef int bool;
enum {
    false, true
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <straight/straight.h>

#define DEBUG 1

char inputAFileName[256] = "";
char noteFileName[256] = "";
char outputFileName[256] = "";


char outputPluginName[256] = "output_wav";

static stBool callbackFunc(Straight straight, stCallbackType callbackType,
                           void *callbackData, void *userData) {
    int percent;

    if (callbackType == STRAIGHT_F0_PERCENTAGE_CALLBACK) {
        percent = (int) callbackData;
        fprintf(stderr, "STRAIGHT F0: %d %%\r", percent);
        if (percent >= 100) {
            fprintf(stderr, "\n");
        }
    }
    else if (callbackType == STRAIGHT_AP_PERCENTAGE_CALLBACK) {
        percent = (int) callbackData;
        fprintf(stderr, "STRAIGHT AP: %d %%\r", percent);
        if (percent >= 100) {
            fprintf(stderr, "\n");
        }
    }
    else if (callbackType == STRAIGHT_SPECGRAM_PERCENTAGE_CALLBACK) {
        percent = (int) callbackData;
        fprintf(stderr, "STRAIGHT spectrogram: %d %%\r", percent);
        if (percent >= 100) {
            fprintf(stderr, "\n");
        }
    }
    else if (callbackType == STRAIGHT_SYNTH_PERCENTAGE_CALLBACK) {
        percent = (int) callbackData;
        fprintf(stderr, "STRAIGHT synthesis: %d %%\r", percent);
        if (percent >= 100) {
            fprintf(stderr, "\n");
        }
    }

    return ST_TRUE;
}

int main(int argc, char *argv[]) {

    // note = baseNote * 2^(n-37/12)*220.00;
    double baseNote = 220.0;

    int i, j, k, l, m;
    //Config
    int frameShift1 = 10;
    int frameLength1 = 40;
    int nframesA = 0;
    int nframes = 0;
    StraightConfig config;
    Straight straight;
    StraightSource sourceA;
    StraightSpecgram specgramA;
    StraightSynth synth;
    double *inputAWave;
    long inputAWaveLength;


    double startTimes[300];
    double endTimes[300];

    const char *noteSheet[75] = {
            "A,,", "A#,,", "B,,", "C,", "C#,",
            "D,", "D#,", "E,", "F,", "F#,",
            "G,", "G#,", "A,", "A#,", "B,",
            "C", "C#", "D", "D#", "E",
            "F", "F#", "G", "G#", "A",
            "A#", "B", "c", "c#", "d",
            "d#", "e", "f", "f#", "g",
            "g#", "a", "a#", "b", "c'",
            "c#'", "d'", "d#'", "e'", "f'",
            "f#'", "g'", "g#'", "a'", "a#'",
            "b'", "c''", "c#''", "d''", "d#''",
            "e''", "f''", "f#''", "g''", "g#''",
            "a''", "a#''", "b''", "c'''", "c#'''"
    };

    double note[300];


    double startTime = 0.0;
    double endTime = 0.0;
    char noteValue[10];
    bool found = false;
    FILE *noteFile;
    int nFramesA;
    int nFrames;


    int lastFrame;
    double lastHertz;
    double nextHertz;
    double hertz;
    int nextFrame;

    int startFrame = 0;
    int endFrame = 0;


    double f0a;
    double tempLastHertz;
    double step;
    double lastEnd = 0.0;

    double minimum;

    char line[1000];
    int mallocCounter = 0;

    double abc1, abc2, abc3, abc4;
    char c;

    if (argc < 4) {
        fprintf(stderr, "simple <input file speech> <input file notes> <output file> <frameshift[10ms]>\n");
        exit(1);
    }
    else if (argc == 5) {
        frameShift1 = atoi(argv[4]);

        if (frameShift1 != 0) {
            fprintf(1, "Frameshift %s ms added\n", argv[4]);
        }
        else {
            fprintf(1, "Frameshift %s ms couldn't be added. Stay below %ld\n", argv[4], frameLength1);
        }
    }


    strcpy_s(inputAFileName, 100, argv[1]);
    strcpy_s(outputFileName, 100, argv[3]);
    strcpy_s(noteFileName, 100, argv[2]);

    noteFile = fopen(noteFileName, "r");
    if (noteFile == NULL) {

        fprintf(stderr, "Could not open note file\n");
        exit(1);

    }


    while (!feof(noteFile)) {
        mallocCounter = mallocCounter + 1;
        startTime = -1;
        endTime = -1;
        found = false;

        if (mallocCounter > (sizeof(startTimes) / sizeof(double))) {
            fprintf(stderr, "Please check note file, too many lines\n");
            exit(1);
        }
        fscanf(noteFile, "%lf %lf %s\n", &startTime, &endTime, noteValue);
        printf("%lf %lf %s\n", startTime, endTime, noteValue);

        if (startTime < 0.0 || endTime < 0.0 || noteValue == NULL) {
            fprintf(stderr, "Please check note file, invalid format\n");
            exit(1);
        }


        if ((startTime > endTime) || (startTime < lastEnd)) {
            fprintf(stderr, "Please check note file, invalid times\n");
            exit(1);
        }
        for (i = 0; i < 65; i++) {
            if (strcmp(noteValue, noteSheet[i]) == 0 && found == false) {
                note[mallocCounter - 1] = (baseNote * powl(2.0, ((double) (i + 1 - 37) / 12)));
                found = true;

            }
        }
        if (found == false) {
            fprintf(stderr, "Please check note file, invalid notes\n");
            exit(1);
        }

        startTimes[mallocCounter - 1] = startTime;
        endTimes[mallocCounter - 1] = endTime;
        lastEnd = endTime;
    }

    fclose(noteFile);


#ifdef USE_TANDEM
    straightInitConfigTandem(&config);
#else
    straightInitConfig(&config);
#endif
    config.samplingFrequency = 16000.0;
    config.frameShift = frameShift1;
    config.frameLength = frameLength1;

    if ((straight = straightInitialize(&config)) != NULL) {
        if (straightReadAudioFile(straight, inputAFileName, NULL) == ST_FALSE) {
            fprintf(stderr, "Error: Cannot open file: %s\n", inputAFileName);
        }
        else {
            inputAWave = xstraightGetCurrentWave(straight, &inputAWaveLength);

            fprintf(stderr, "Input file A: %s\n", inputAFileName);

            straightSetCallbackFunc(straight, STRAIGHT_PERCENTAGE_CALLBACK, callbackFunc, NULL);

            sourceA = straightSourceInitialize(straight, NULL);
            straightSourceCompute(straight, sourceA, inputAWave, inputAWaveLength);

            specgramA = straightSpecgramInitialize(straight, NULL);
            straightSpecgramCompute(straight, sourceA, specgramA, inputAWave, inputAWaveLength);

            nFramesA = straightSpecgramGetNumFrames(specgramA);
            nFrames = nframesA;

            //Soft check if the musical score file contains too many lines
            if (nframesA < (sizeof(startTimes) / sizeof(double))) {
                fprintf(stderr, "Error, too many lines for the speech = %ld", (sizeof(startTimes) / sizeof(double)));
                exit(1);
            }

            //Hard check, if too many frames are used
            if (nframesA < endTimes[(sizeof(startTimes) / sizeof(double)) - 1] * 1000 / frameShift1) {
                fprintf(stderr, "Error, too long for the speech = %ld",
                        (endTimes[(sizeof(startTimes) / sizeof(double)) - 1] * 1000 / frameShift1));
                exit(1);
            }

#ifdef DEBUG
            fprintf(stderr, "nFrames = %ld",
                    nFramesA);
#endif

            /*
            An introduction in morphing, if the singing should be created via merging two sound tracks
            apBufferPtrA = straightSourceGetAperiodicityPointer(straight, sourceA, n);
            apBufferPtrMorph = straightSourceGetAperiodicityPointer(straight, sourceMorph, n);
            for (k = 0; k < frequencyLength; k++) {
            apBufferPtrMorph[k] = apBufferPtrA[k] * 0.5 + apBufferPtrB[k] * 0.5;
            }

            for (k = 0; k < frequencyLength; k++) {
            50% morphing of log spectrum
            specBufferPtrMorph[k] = pow(specBufferPtrA[k], 0.5) * pow(specBufferPtrB[k], 0.5);
            }
            **/


            //catch malloc for big files (maybe file caching?)
            lastFrame = 0;
            lastHertz = 0;
            for (i = 0; i < (sizeof(startTimes) / sizeof(double)); i = i + 1) {
                startFrame = (int) (startTimes[i] * 1000) / frameShift1;
                endFrame = (int) (endTimes[i] * 1000) / frameShift1;
                hertz = note[i];

                //Check if nextFrame is null
                nextFrame = (int) (startTimes[i + 1] * 1000) / frameShift1;
                if (i + 1 == (sizeof(startTimes) / sizeof(double))) {
                    nextHertz = 0.0;
                    nextFrame = endFrame + 1;
                    if (nextFrame < nFramesA) {
                        for (l = nextFrame; l < nFramesA; l = l + 1) {
                            straightSourceSetF0(straight, sourceA, l, 0.0);
                        }
                    }
                }
                else {
                    nextHertz = note[i + 1];
                    nextFrame = (int) (startTimes[i + 1] * 1000) / frameShift1;
                }

                if (startFrame < lastFrame || endFrame < startFrame || startFrame > nFramesA || endFrame > nFramesA) {
                    fprintf(stderr, "Please check note file, invalid times\n");
                    if (endFrame < startFrame) {
                        fprintf(stderr, "Please check note file, invalid times2\n");
                    }
                    if (startFrame > nFramesA) {
                        fprintf(stderr, "Please check note file, invalid times3 %ld, %ld, %ld, %ld\n", startFrame, i,
                                nFrames, nFramesA);
                    }
                    exit(1);
                }

                if (startFrame - lastFrame > 2) {
                    for (k = lastFrame; k < startFrame; k = k + 1) {
                        straightSourceSetF0(straight, sourceA, k, 0.0);
                    }
                    lastFrame = k;
                    lastHertz = 0.0;
                }


                for (j = startFrame; j < endFrame + 1; j = j + 1) {

                    if ((j - startFrame) * frameShift1 < 50) {
                        if (lastHertz < hertz) {


                            straightSourceGetF0(straight, sourceA, startFrame - 1, &f0a);
                            straightSourceGetF0(straight, sourceA, j - 1, &tempLastHertz);

                            step = (int) ((hertz - f0a + hertz * 0.05) / (50 / frameShift1));
                            straightSourceSetF0(straight, sourceA, j, tempLastHertz + step);
                            minimum = tempLastHertz + step;

                        }
                        else if (lastHertz > hertz) {


                            straightSourceGetF0(straight, sourceA, j - 1, &tempLastHertz);
                            straightSourceGetF0(straight, sourceA, startFrame - 1, &f0a);


                            //Overshot
                            step = (int) (hertz - f0a - hertz * 0.05) / (50 / frameShift1);
                            straightSourceSetF0(straight, sourceA, j, tempLastHertz + step);
                            minimum = tempLastHertz + step;

                        }

                    }
                    else if ((j - startFrame) * frameShift1 < 90) {
                        tempLastHertz;
                        straightSourceGetF0(straight, sourceA, j - 1, &tempLastHertz);
                        step = (int) (hertz - minimum) / (40 / frameShift1);
                        straightSourceSetF0(straight, sourceA, j, tempLastHertz + step);

                    }
                    else if ((endFrame - j) < (40 / frameShift1)) {
                        if (nextHertz < hertz) {

                            //step down
                            tempLastHertz;
                            straightSourceGetF0(straight, sourceA, j - 1, &tempLastHertz);

                            step = (int) ((hertz - minimum + (nextHertz - hertz) * 0.35) / (40 / frameShift1));
                            straightSourceSetF0(straight, sourceA, j, tempLastHertz + step);

                        }
                        else if (nextHertz > hertz) {

                            tempLastHertz;
                            straightSourceGetF0(straight, sourceA, j - 1, &tempLastHertz);


                            //Overshot
                            step = (int) ((hertz - minimum + (nextHertz - hertz) * 0.25) / (40 / frameShift1));
                            straightSourceSetF0(straight, sourceA, j, tempLastHertz + step);

                        }
                    }
                    else if ((endFrame - j) < (80 / frameShift1)) {

                        if (nextHertz < hertz) {

                            //step up
                            tempLastHertz;
                            straightSourceGetF0(straight, sourceA, j - 1, &tempLastHertz);

                            step = (int) (hertz * 0.05) / (40 / frameShift1);
                            straightSourceSetF0(straight, sourceA, j, tempLastHertz + step);
                            minimum = tempLastHertz + step;

                        }
                        else if (nextHertz > hertz) {

                            tempLastHertz;
                            straightSourceGetF0(straight, sourceA, j - 1, &tempLastHertz);


                            //Overshot
                            step = (int) (hertz * (-0.05)) / (40 / frameShift1);
                            straightSourceSetF0(straight, sourceA, j, tempLastHertz + step);
                            minimum = tempLastHertz + step;

                        }


                    }
                    else {
                        straightSourceSetF0(straight, sourceA, j, hertz);
                    }
                    lastFrame = j;
                    lastHertz = hertz;
                }
            }
            synth = straightSynthInitialize(straight, NULL);
            if (straightSynthCompute(straight, sourceA, specgramA, synth, 1.0, 1.0, 1.0) == ST_TRUE) {
                if (straightWriteSynthAudioFile(straight, synth,
                                                outputFileName, outputPluginName, 16) == ST_TRUE) {
                    fprintf(stderr, "Output file: %s\n", outputFileName);
                }
                else {
                    fprintf(stderr, "Error: Cannot open file: %s\n", outputFileName);
                }
            }

            straightSourceDestroy(sourceA);
            straightSpecgramDestroy(specgramA);
            straightSynthDestroy(synth);

            free(inputAWave);
            straightDestroy(straight);
        }
    }
    else {
        fprintf(stderr, "Error: Cannot initialize STRAIGHT engine\n");
    }

    return 0;
}