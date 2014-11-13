//
// Copyright (C) 2010 Pandachip Limited.  All rights reserved.
//
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "VoiceFilterBank_msc3.h"

static short h0[FILTER_LENGTH+1];
static short temp_voice[FILTER_LENGTH+1];

double fh0[FILTER_LENGTH+1] = { 
0.0001894468,
0.0016070033,
-0.0012761811,
0.0018984791,
0.0017185615,
-0.0070609605,
0.0051794466,
-0.0115097446,
-0.0222122117,
0.0026533117,
-0.0581311311,
-0.0331012582,
-0.0088599435,
-0.1616992662,
0.0954368721,
0.7558181763,
0.0954368721,
-0.1616992662,
-0.0088599435,
-0.0331012582,
-0.0581311311,
0.0026533117,
-0.0222122117,
-0.0115097446,
0.0051794466,
-0.0070609605,
0.0017185615,
0.0018984791,
-0.0012761811,
0.0016070033,
0.0001894468,
0,
};



void VoiceFilterBank_init() 
{
	int i;
	memset(temp_voice,0,(FILTER_LENGTH+1)*sizeof(short));
	for (i=0; i< FILTER_LENGTH; i++) {
		h0[i] = (int) (fh0[i]*0x7FFF);
	}
}

clock_t start,stop;

void VoiceFilterBank_Filtering(short *pSignal_Input,int InputSize, short *pSignal_Output)
{
	int Tmp_Sum=0;
	int i,j;

	int Tmp_Output0;
	//start = clock();
	for (i=0; i<FILTER_LENGTH; i++) {
		Tmp_Output0=0;
		for (j=0; j<(FILTER_LENGTH-i-1); j++) {
			Tmp_Output0+=temp_voice[i+j]*h0[j];
		}
		int offset = (FILTER_LENGTH-i-1);
		for (j=(FILTER_LENGTH-i-1); j<FILTER_LENGTH; j++) {
			Tmp_Output0+=*(pSignal_Input+j-offset)*h0[j];
		}
		Tmp_Sum = (Tmp_Output0>>16);

		if (Tmp_Sum > 32767) {
			*pSignal_Output = 32767;
		} 
		else if (Tmp_Sum < -32768) {
			*pSignal_Output = -32768;
		}
		else {
			*pSignal_Output = (short) Tmp_Sum;
		}

		pSignal_Output++;
	}

	for (i=FILTER_LENGTH; i<InputSize; i++) {
		Tmp_Output0=0;

		int offset = (FILTER_LENGTH-i-1);
		for (j=0; j<FILTER_LENGTH; j++) {
			Tmp_Output0+=*(pSignal_Input+j-offset)*h0[j];
		}
		Tmp_Sum = (Tmp_Output0>>16);

		if (Tmp_Sum > 32767) {
			*pSignal_Output = 32767;
		} 
		else if (Tmp_Sum < -32768) {
			*pSignal_Output = -32768;
		}
		else {
			*pSignal_Output = (short) Tmp_Sum;
		}

		pSignal_Output++;
	}

	memcpy(temp_voice,(pSignal_Input+InputSize-(FILTER_LENGTH-1)),(FILTER_LENGTH-1)*sizeof(short));
	//stop = clock();
	//printf("T=%d c\n",(stop-start));
}

