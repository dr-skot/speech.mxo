/* aka.speech.c */

#include "ext.h"		/* contains basic function macros and type definitions */
#include "ext_obex.h"
#include "ext_strings.h"

#include <Gestalt.h>
#include <SpeechSynthesis.h>
#include <FixMath.h>
#include <Components.h>
#include "string.h"


//#define	RES_ID			7704
//#define	ASSIST_INLET	1
//#define	ASSIST_OUTLET	6

#define	VOICE_MAX	64
#define	TEXT_MAX	1024

/* Speech object data structure */

typedef struct Speech
{
	struct object		myObject;		/* object */
	SpeechChannel		speechChannel;
	long				voice;
	OSType				inputMode, charactorMode, numberMode;
	SpeechDoneUPP		endOfSpeechUPP;
	SpeechPhonemeUPP	phonemeUPP;
	SpeechWordUPP	wordUPP;
  char        text[TEXT_MAX];
  long        cursor;
	void				*nameOutlet;
	void				*phonemeOutlet;
	void				*endOfSpeechOutlet;
  void        *wordOutlet;
  void        *notSpokenOutlet;
  void        *textToPhonemesOutlet;
  t_symbol*   messageForText;
} Speech;

void *Speech_new(Symbol *s,int ac,Atom *av);
void Speech_free(Speech *x);

Boolean SpeechAvailable (void);
OSErr CreateSpeechChannel(Speech *x);
pascal void MyEndOfSpeechUPP(SpeechChannel chan, long refCon);
pascal void MyPhonemeUPP(SpeechChannel chan, long refCon, short phonemeOpcode);
pascal void MyWordUPP(SpeechChannel chan, long refCon, long wordPos, short wordLen);
void Speech_outputUnspokenCharacters(Speech *x, long wordPos, short wordLen); 

void Speech_speak(Speech *x,Symbol *message,int argc,Atom *argv);
void Speech_stop(Speech *x, int n);
void Speech_pause(Speech *x, int n);
void Speech_continue(Speech *x);
void Speech_voice(Speech *x,Symbol *message,int argc,Atom *argv);
void OutputVoiceList(Speech *x);

void Speech_pitch(Speech *x,Symbol *message,int argc,Atom *argv);
void Speech_pitch_in(Speech *x,double f);
void Speech_volume(Speech *x,Symbol *message,int argc,Atom *argv);
void Speech_volume_in(Speech *x,double f);
void Speech_rate(Speech *x,Symbol *message,int argc,Atom *argv);
void Speech_rate_in(Speech *x,double f);
void Speech_modulation(Speech *x,Symbol *message,int argc,Atom *argv);
void Speech_modulation_in(Speech *x,double f);
void Speech_reset(Speech *x);

void Speech_inputMode(Speech *x,Symbol *s);
void ChangeInputMode(Speech *x);
void Speech_characterMode(Speech *x,Symbol *s);
void ChangeCharactorMode(Speech *x);
void Speech_numberMode(Speech *x,Symbol *s);
void ChangeNumberMode(Speech *x);

void Speech_phonemes(Speech *x, Symbol *message, short atomCount, Atom *atoms);

void Speech_assist(Speech *x,void *b,long m,long a,char *s);
void PostErr(char *message, OSErr theErr);


// utility functions
t_atom *newAtom();
t_atom *atomFromText(char *text);
char* atomsToString(short atomCount, Atom *atoms);
char* join_strings(int count, char* strings[], char *glue);
char *MYCFStringCopyUTF8String(CFStringRef aString);


//fptr	*FNS;				/* global copy of Max function macro table */
void	*class;				/* global variable that contains the Speech class */
//long	gSaveA5;

/* ======================================================= */
int main()						/* you're passed a pointer to the function macro table */
{
    //long oldA4;
        
    //oldA4 = SetCurrentA4();
    //RememberA4();

	//EnterCodeResource();	// sets correct value of A4
	//PrepareCallback();		// remembers A4 inside the code resource

	//FNS = f;				/* must do this before any function macros work */	

	//gSaveA5 = SetCurrentA5();

	if (SpeechAvailable())
	{
		/* set up the class */
		setup((t_messlist **)&class, (method)Speech_new,(method)Speech_free, (short)sizeof(Speech), 0L, A_GIMME, 0);
		
		addmess((method)Speech_speak,"speak",A_GIMME,0);
		addmess((method)Speech_speak,"s",A_GIMME,0);
		addmess((method)Speech_stop,"stop",A_DEFLONG,0);
		addmess((method)Speech_pause,"pause",A_DEFLONG,0);
		addmess((method)Speech_continue,"continue",A_NOTHING,0);
		
		addmess((method)Speech_rate,"rate",A_GIMME,0);
		addmess((method)Speech_rate,"r",A_GIMME,0);
		addmess((method)Speech_pitch,"pitch",A_GIMME,0);
		addmess((method)Speech_pitch,"p",A_GIMME,0);
		addmess((method)Speech_volume,"volume",A_GIMME,0);
		addmess((method)Speech_volume,"v",A_GIMME,0);
		addmess((method)Speech_modulation,"modulation",A_GIMME,0);
		addmess((method)Speech_modulation,"m",A_GIMME,0);
		addmess((method)Speech_voice,"voice",A_GIMME,0);
		addmess((method)Speech_reset,"reset",A_NOTHING,0);
		addmess((method)Speech_inputMode,"inputMode",A_SYM,0);
		addmess((method)Speech_inputMode,"i",A_SYM,0);
		addmess((method)Speech_characterMode,"characterMode",A_SYM,0);
		addmess((method)Speech_characterMode,"c",A_SYM,0);
		addmess((method)Speech_numberMode,"numberMode",A_SYM,0);
		addmess((method)Speech_numberMode,"n",A_SYM,0);
    
    addmess((method)Speech_phonemes, "phonemes", A_GIMME, 0);
    addmess((method)Speech_phonemes, "tune", A_GIMME, 0);
    
		addmess((method)Speech_assist,"assist",A_CANT,0);
		
		addftx((method)Speech_pitch_in,1);
		addftx((method)Speech_volume_in,2);
		addftx((method)Speech_rate_in,3);
		addftx((method)Speech_modulation_in,4);

		finder_addclass("System","speech");				/* add to New Object List */
		//rescopy('STR#',RES_ID);								/* copy assistance string resource to Max Temp file */
	}
	else
	{
		error("Speech Manager is not available.");
	}
	
	post("aka.speech 1.2-UB by Masayuki Akamatsu");
	post("modified by skot: added word callbacks and text-to-phonemes and tune!");

	//RestoreA4(oldA4);
	//ExitCodeResource();
  
  return 0;
}

/* ======================================================= */
Boolean SpeechAvailable (void)
{
	OSErr			theErr;
	long			result;
	
	theErr = Gestalt(gestaltSpeechAttr, &result);
	if ((theErr != noErr) || !(result &  (1 << gestaltSpeechMgrPresent)))
		return false;
	else
		return true;
}

/* ======================================================= */
/* ======================================================= */
void *Speech_new(Symbol *s,int ac,Atom *av)
{
	Speech *x;							/* object we'll be creating */
	OSErr	theErr;
	
	x = (Speech *)newobject(class);		         /* allocates memory and sticks in an inlet */
	x->textToPhonemesOutlet = outlet_new(x, 0L);  /* create a general outlet */
	x->endOfSpeechOutlet = bangout(x);	       /* create a bang outlet */
	x->phonemeOutlet = intout(x);		           /* create an int outlet */
	x->notSpokenOutlet = outlet_new(x, 0L); /* create an anything outlet */
	x->wordOutlet = outlet_new(x, 0L);	     /* create an anything outlet */
	x->nameOutlet = outlet_new(x, 0L);	       /* create a general outlet */
	
	floatin(x, 4);
	floatin(x, 3);
	floatin(x, 2);
	floatin(x, 1);

	x->speechChannel = nil;
	x->voice = 0;
	x->inputMode = modeText;
	x->charactorMode = modeNormal;
	x->numberMode = modeNormal;
	x->endOfSpeechUPP = NewSpeechDoneUPP(MyEndOfSpeechUPP);
	x->phonemeUPP = NewSpeechPhonemeUPP(MyPhonemeUPP);
	x->wordUPP = NewSpeechWordUPP(MyWordUPP);
  x->messageForText = gensym("list");

	theErr = CreateSpeechChannel(x);
	if (theErr == noErr)
		OutputVoiceList(x);

	return (x);					/* always return a copy of the created object */
}

/* ======================================================= */
/* ======================================================= */
OSErr CreateSpeechChannel(Speech *x)
{
	OSErr		theErr;
	VoiceSpec	voiceSpec;
	
	if (x->speechChannel)
	{
		theErr = DisposeSpeechChannel(x->speechChannel);
		PostErr("DisposeSpeechChannel",theErr);
		x->speechChannel = nil;
	}
	
	GetIndVoice(x->voice, &voiceSpec);
	if (x->voice == 0)
		theErr = NewSpeechChannel(0, &(x->speechChannel));
	else
		theErr = NewSpeechChannel(&voiceSpec, &(x->speechChannel));			
	PostErr("NewSpeechChannel",theErr);
	

	if (theErr == noErr)
	{
		ChangeInputMode(x);
		ChangeCharactorMode(x);
		ChangeNumberMode(x);
	}

	if (theErr == noErr)
	{
		theErr =  SetSpeechInfo (x->speechChannel, soRefCon, x);
		PostErr("SetSpeechInfo/soRefCon",theErr);
		theErr = SetSpeechInfo (x->speechChannel, soSpeechDoneCallBack, x->endOfSpeechUPP);
		PostErr("SetSpeechInfo/soSpeechDoneCallBack",theErr);
		theErr = SetSpeechInfo (x->speechChannel, soPhonemeCallBack, x->phonemeUPP);
		PostErr("SetSpeechInfo/soPhonemeCallBack",theErr);
		theErr = SetSpeechInfo (x->speechChannel, soWordCallBack, x->wordUPP);
		PostErr("SetSpeechInfo/kSpeechWordCallBack",theErr);
	}	
	
	return(theErr);
}

/* ======================================================= */
pascal void MyEndOfSpeechUPP(SpeechChannel chan, long refCon)
{
	Speech *x;
	
	x = (Speech *)refCon;
  Speech_outputUnspokenCharacters(x, strlen(x->text), 0);
	outlet_bang(x->endOfSpeechOutlet);
}

/* ======================================================= */
pascal void MyPhonemeUPP (SpeechChannel chan, long refCon, short phonemeOpcode)
{
	Speech *x;
	
	x = (Speech *)refCon;
	outlet_int(x->phonemeOutlet,phonemeOpcode);	
}

/* ======================================================= */
pascal void MyWordUPP (SpeechChannel chan, long refCon, long wordPos, short wordLen)
{
	Speech *x;
	
	x = (Speech *)refCon;
  
  Speech_outputUnspokenCharacters(x, wordPos, wordLen);

  // collect word
  char word[wordLen+2];
  strncpy_zero(word, x->text+wordPos, wordLen+1);
    
  t_atom theAtoms[3];
  atom_setlong(theAtoms, wordPos);
  atom_setlong(theAtoms + 1, wordLen);
  atom_setsym(theAtoms + 2, gensym(word));
	outlet_anything(x->wordOutlet, gensym("list"), 3, theAtoms);
  
  //post("word |%s|", word);

  //free(word);
}

void Speech_outputUnspokenCharacters(Speech *x, long wordPos, short wordLen) 
{
  long unspokenLen = wordPos - x->cursor;
  
  // collect word
  char unspokenChars[unspokenLen+2];
  strncpy_zero(unspokenChars, x->text+x->cursor, unspokenLen+1);
  
  t_atom theAtoms[3];
  atom_setlong(theAtoms, x->cursor);
  atom_setlong(theAtoms + 1, unspokenLen);
  atom_setsym(theAtoms + 2, gensym(unspokenChars));
  outlet_anything(x->notSpokenOutlet, gensym("list"), 3, theAtoms);

  x->cursor = wordPos + wordLen;

  //post("unspoken |%s|", unspokenChars);
  
  //free(unspokenChars);
}


/* ======================================================= */
void Speech_free(Speech *x)	/* argument is a pointer to an instance */
{
	OSErr	theErr;
	
	if (x->speechChannel)
	{
		theErr = StopSpeech(x->speechChannel);
		PostErr("StopSpeech",theErr);
		theErr = DisposeSpeechChannel(x->speechChannel);
		PostErr("DisposeSpeechChannel",theErr);
		x->speechChannel = nil;
	}

	DisposeRoutineDescriptor(x->endOfSpeechUPP);
	DisposeRoutineDescriptor(x->phonemeUPP);
  DisposeRoutineDescriptor(x->wordUPP);
}

/* ======================================================= */
/* ======================================================= */
void Speech_speak(Speech *x,Symbol *message,int argc,Atom *argv)
{
	OSErr	theErr;
	short	i;
	char	*str;
	char	text[TEXT_MAX];
	Str255	numStr;
	long	len;
	
	if (x->speechChannel != nil)
	{
		len = 0;
		text[0] = 0;

		for(i=0; i<argc; i++)
		{
			if (i>0)
			{
				len++;
				if (len >= TEXT_MAX)	break;
				strcat(text, " ");
			}
			
			if (argv->a_type==A_SYM)
			{
				str = argv->a_w.w_sym->s_name;
				len += strlen(str);
				if (len >= TEXT_MAX)	break;
				strcat(text, str);
			}
			else
			if (argv->a_type==A_LONG)
			{
				NumToString(argv->a_w.w_long, numStr);
				PtoCstr(numStr);
				len += strlen((char *)numStr);
				if (len >= TEXT_MAX)	break;
				strcat(text, (char *)numStr);
			}
			argv++;
		}
		theErr = SpeakText(x->speechChannel, text, len);
		PostErr("SpeakText",theErr);

    // reset buffer
    strncpy_zero(x->text, text, len+1);
    x->cursor = 0;
	}
}

/* ======================================================= */
void Speech_stop(Speech *x, int n)
{
	OSErr theErr;
	
	if (x->speechChannel != nil)
	{
		if (n<kImmediate || n>kEndOfSentence)
			n = kImmediate;
		theErr = StopSpeechAt(x->speechChannel, n);
		PostErr("StopSpeechAt",theErr);
	}
}

/* ======================================================= */
void Speech_pause(Speech *x, int n)
{
	OSErr theErr;
	
	if (x->speechChannel != nil)
	{
		if (n<kImmediate || n>kEndOfSentence)
			n = kImmediate;
		theErr = PauseSpeechAt(x->speechChannel, n);
		PostErr("PauseSpeechAt",theErr);
	}
}

/* ======================================================= */
void Speech_continue(Speech *x)
{
	OSErr theErr;
	
	if (x->speechChannel != nil)
	{
		theErr = ContinueSpeech(x->speechChannel);
		PostErr("ContinueSpeech",theErr);
	}
}

/* ======================================================= */
/* ======================================================= */
void Speech_voice(Speech *x,Symbol *message,int argc,Atom *argv)
{
	OSErr				theErr;
	short				i,voiceMax;
	
	if (x->speechChannel != nil)
	{
	
		theErr = CountVoices(&voiceMax);
		PostErr("CountVoices",theErr);

		if ( argc>0 && argv->a_type==A_LONG)
		{
			i = argv->a_w.w_long;
			if (i>=0 && i<=voiceMax)
			{
				x->voice = i;
				CreateSpeechChannel(x);
			}
		}
		else
		if (argc==0)
		{
			OutputVoiceList(x);
		}			
	}
}

/* ======================================================= */
void OutputVoiceList(Speech *x)
{
	OSErr				theErr;
	short				i,voiceMax;
	VoiceSpec			voiceSpec;
	VoiceDescription	vd;
	Atom				atm;

	theErr = CountVoices(&voiceMax);
	PostErr("CountVoices",theErr);

	outlet_anything(x->nameOutlet,gensym("clear"),0,NIL);
	for (i = 0; i <= voiceMax; i++)
	{
		if (i==0)
		{
			SETSYM( &atm, gensym("(default)"));
		}
		else
		{
			theErr = GetIndVoice(i, &voiceSpec);
			PostErr("GetIndVoice",theErr);
			theErr = GetVoiceDescription(&voiceSpec, &vd, sizeof(VoiceDescription));
			PostErr("GetVoiceDescription",theErr);
			PtoCstr(vd.name);
			SETSYM( &atm, gensym((char *)(vd.name)));
		}
		outlet_anything(x->nameOutlet,gensym("append"),1,&atm);
	}
}

/* ======================================================= */
/* ======================================================= */
void Speech_pitch(Speech *x,Symbol *message,int argc,Atom *argv)
{
	double	f;
	
	if (x->speechChannel != nil)
	{
		if (argc>0)
		{
			if (argv->a_type==A_LONG)
				f = (double)(argv->a_w.w_long);
			if (argv->a_type==A_FLOAT)
				f =  argv->a_w.w_float;

			Speech_pitch_in(x, f);
		}
	}
}

/* ======================================================= */
void Speech_pitch_in(Speech *x,double f)
{
	OSErr	theErr;
	Fixed temp;

	temp =  X2Fix(f);
	theErr = SetSpeechPitch(x->speechChannel, temp);
	PostErr("SetSpeechPitch",theErr);
}

/* ======================================================= */
/* ======================================================= */
void Speech_volume(Speech *x,Symbol *message,int argc,Atom *argv)
{
	double	f;
	
	if (x->speechChannel != nil)
	{
		if (argc>0)
		{
			if (argv->a_type==A_LONG)
				f = (double)(argv->a_w.w_long);
			if (argv->a_type==A_FLOAT)
				f =  argv->a_w.w_float;

			Speech_volume_in(x, f);
		}
	}
}

/* ======================================================= */
void Speech_volume_in(Speech *x,double f)
{
	OSErr	theErr;
	Fixed	temp;

	temp = X2Fix(f / 100.0);	/* volume must be from 0.0 throgh 1.0 */
	
	theErr = SetSpeechInfo(x->speechChannel, soVolume, &temp);
	PostErr("SetSpeechInfo/Volume",theErr);
}

/* ======================================================= */
/* ======================================================= */
void Speech_rate(Speech *x,Symbol *message,int argc,Atom *argv)
{
	double	f;
	
	if (x->speechChannel != nil)
	{
		if (argc>0)
		{
			if (argv->a_type==A_LONG)
				f = (double)(argv->a_w.w_long);
			if (argv->a_type==A_FLOAT)
				f =  argv->a_w.w_float;

			Speech_rate_in(x, f);
		}
	}
}

/* ======================================================= */
void Speech_rate_in(Speech *x,double f)
{
	OSErr	theErr;
	Fixed	temp;
	
	temp =  X2Fix(f);
	theErr = SetSpeechRate(x->speechChannel, temp);
	PostErr("SetSpeechRate",theErr);
}

/* ======================================================= */
/* ======================================================= */
void Speech_modulation(Speech *x,Symbol *message,int argc,Atom *argv)
{
	double	f;
	
	if (x->speechChannel != nil)
	{
		if (argc>0)
		{
			if (argv->a_type==A_LONG)
				f = (double)(argv->a_w.w_long);
			if (argv->a_type==A_FLOAT)
				f =  argv->a_w.w_float;

			Speech_modulation_in(x, f);
		}
	}
}

/* ======================================================= */
void Speech_modulation_in(Speech *x,double f)
{
	OSErr	theErr;
	Fixed	temp;
	
	temp =  X2Fix(f);
	theErr = SetSpeechInfo(x->speechChannel, soPitchMod, &temp);
	PostErr("SetSpeechInfo/Modulation",theErr);
}

/* ======================================================= */
/* ======================================================= */
void Speech_reset(Speech *x)
{
	OSErr	theErr;
	long	temp = 0;

	if (x->speechChannel != nil)
	{
		theErr = SetSpeechInfo(x->speechChannel, soReset, &temp);
		PostErr("SetSpeechInfo/Reset",theErr);
	}
}

/* ======================================================= */
/* ======================================================= */
void Speech_inputMode(Speech *x,Symbol *s)
{
	if (x->speechChannel != nil)
	{
		if (s==gensym("phonemes"))
			x->inputMode = modePhonemes;
		else
			x->inputMode = modeText;	/* default */

		ChangeInputMode(x);
	}			
}
/* ======================================================= */
void ChangeInputMode(Speech *x)
{
	OSErr		theErr;

	theErr = SetSpeechInfo(x->speechChannel, soInputMode, &(x->inputMode));
	PostErr("SetSpeechInfo/soInputMode",theErr);
}

/* ======================================================= */
void Speech_characterMode(Speech *x,Symbol *s)
{
	if (x->speechChannel != nil)
	{
		if (s==gensym("literal"))
			x->charactorMode = modeLiteral;
		else
			x->charactorMode = modeNormal;	/* default */

		ChangeCharactorMode(x);
	}	
}

/* ======================================================= */
void ChangeCharactorMode(Speech *x)
{
	OSErr	theErr;

	theErr = SetSpeechInfo(x->speechChannel, soCharacterMode, &(x->charactorMode));
	PostErr("SetSpeechInfo/soCharacterMode",theErr);
}

/* ======================================================= */
void Speech_numberMode(Speech *x,Symbol *s)
{
	if (x->speechChannel != nil)
	{
		if (s==gensym("literal"))
			x->numberMode = modeLiteral;
		else
			x->numberMode = modeNormal;	/* default */

		ChangeNumberMode(x);
	}
}
/* ======================================================= */
void ChangeNumberMode(Speech *x)
{
	OSErr	theErr;

	theErr = SetSpeechInfo(x->speechChannel, soNumberMode, &(x->numberMode));
	PostErr("SetSpeechInfo/soNumberMode",theErr);
}


/* ======================================================= */
/* ======================================================= */
void Speech_phonemes(Speech *x, Symbol *message, short atomCount, Atom *atoms)
{
	OSErr	theErr;

  // join the atoms into a string
  char *text;  
  text = atomsToString(atomCount, atoms);
  CFStringRef input = CFStringCreateWithCStringNoCopy(NULL, text, kCFStringEncodingUTF8, NULL);
  CFStringRef phonemes;
  
  if (message==gensym("tune")) {
    // crashes Max!!
    // SetSpeechProperty(x->speechChannel, kSpeechPhonemeOptionsProperty, "1248");
    theErr = SetSpeechInfo (x->speechChannel, soPhonemeOptions, (void *)"1248");
  }
  CopyPhonemesFromText(x->speechChannel, input, &phonemes);

  char *result = MYCFStringCopyUTF8String(phonemes);
  outlet_anything(x->textToPhonemesOutlet, gensym("list"), 1, atomFromText(result));
 
  free(result);  
}

t_atom *newAtom()
{
  long count = 0;
  t_atom *result = 0;
  char alloc = 0;
  atom_alloc(&count, &result, &alloc);
  return result;
}

t_atom *atomFromText(char *text) 
{
  t_atom *atom = newAtom();
  atom_setsym(atom, gensym(text));
  return atom;
}                

char *atomsToString(short atomCount, Atom *atoms)
{
  char *strings[atomCount];
  short i;
  
  for (i = 0; i < atomCount; i++) {
    t_symbol *sym = atom_getsym(atoms+i);
    strings[i] = sym->s_name;
  }
  
  return join_strings(atomCount, strings, " ");
}

char* join_strings(int count, char *strings[], char *glue)
{
  char* str = NULL;             /* Pointer to the joined strings  */
  size_t total_length = 0;      /* Total length of joined strings */
  int i = 0;                    /* Loop counter                   */
      
  /* Find total length of joined strings */
  for(i = 0 ; i<count ; i++) {
    if (i > 0) total_length += strlen(glue);
    total_length += strlen(strings[i]);
  }
  ++total_length;     /* For joined string terminator */
      
  str = (char*)malloc(total_length);  /* Allocate memory for joined strings */
  str[0] = '\0';                      /* Empty string we can append to      */
      
  // append all the strings
  for(i = 0 ; i<count ; i++) {
    if (i > 0) strcat(str, glue);    
    strcat(str, strings[i]);
  }
  return str;

}

// http://stackoverflow.com/questions/9166291/converting-a-cfstringref-to-char
char *MYCFStringCopyUTF8String(CFStringRef aString) {
  if (aString == NULL) {
    return NULL;
  }
  
  CFIndex length = CFStringGetLength(aString);
  CFIndex maxSize =
  CFStringGetMaximumSizeForEncoding(length,
                                    kCFStringEncodingUTF8);
  char *buffer = (char *)malloc(maxSize);
  if (CFStringGetCString(aString, buffer, maxSize,
                         kCFStringEncodingUTF8)) {
    return buffer;
  }
  return NULL;
}


            

/* ======================================================= */
/* ======================================================= */
void Speech_assist(Speech *x,void *b,long m,long a,char *s)
{
	if (m==ASSIST_INLET)
	{
		switch (a)
		{
			case 0: sprintf(s,"speak, voice, phonemes, tune, stop, pause, continue,..."); break;
			case 1: sprintf(s,"int(speaking pitch)"); break;
			case 2: sprintf(s,"int(speaking volume)"); break;
			case 3: sprintf(s,"int(speaking rate)"); break;
			case 4: sprintf(s,"int(speaking modulation)"); break;
		}
	}
	else	// ASSIST_OUTLET
	{
		switch(a)
		{
			case 0: sprintf(s,"voice names for umenu"); break;
      case 1: sprintf(s,"word about to be spoken"); break;
      case 2: sprintf(s,"unspoken characters"); break;
			case 3: sprintf(s,"phoneme about to be spoken"); break;
			case 4: sprintf(s,"bang when speech is done"); break;
			case 5: sprintf(s,"text to phonemes"); break;
		}
	}
}
/* ======================================================= */
void PostErr(char *message, OSErr theErr)
{
	if (theErr != noErr)
		error("speech: %s. ID=%ld", message, (long)theErr);
}
