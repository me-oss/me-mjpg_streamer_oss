//
// Copyright (C) 2011 Pandachip Limited.  All rights reserved.
//
#ifndef __VOICEFILTERBANK_H__
#define __VOICEFILTERBANK_H__
	
#define	FILTER_LENGTH 31
#define	DEFAULT_A0 1
#define DEFAULT_B0 1

void VoiceFilterBank_Filtering(short *pSignal_Input,int InputSize, short *pSignal_Output)	;
void VoiceFilterBank_init() ;
#endif // __VOICEFILTERBANK_H__
