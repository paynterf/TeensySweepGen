//Frequency and amplitude sweep tool for testing IR demodulator
//07/11/17 modified to trigger the demodulator so its output can be related to sweep freq
#include <ADC.h> //Analog-to-Digital library

ADC *adc = new ADC(); // adc object;
IntervalTimer myTimer; //added 07/23/17


const int SQWAVE_PIN = 33;
const int DAC_PIN = A21; //DAC0
const int LED_PIN = 13; //added 03/10/15
const int DEMOD_SYNCH_OUT_PIN = 1;
const int DEMOD_SYNCH_IN_PIN = 2;
const int DEMOD_VALUE_READ_PIN = A12;

const int DEFAULT_START_FREQ = 510;//8Hz below
const int DEFAULT_END_FREQ = 530;//8Hz above
const int DEFAULT_FREQ_STEPS = 30;
const float DEFAULT_SEC_PER_FREQ_STEP = 0.5;
const int DEFAULT_OUTPUT_PCT = 50;

const int DEFAULT_START_AMP_PCT = 0;
const int DEFAULT_END_AMP_PCT = 100;
const int DEFAULT_AMP_STEPS = 40;
//const float DEFAULT_AMP_CTR_FREQ_HZ = 51.87; //copied from SqWaveGen.ino
const float DEFAULT_AMP_CTR_FREQ_HZ = 521.1; //number reqd to get 520.8Hz output
const float DEFAULT_SEC_PER_AMP_STEP = 0.5;
//const int DEFAULT_START_AMP_PCT = 50;
//const int DEFAULT_END_AMP_PCT = 50;
//const int DEFAULT_AMP_STEPS = 10;
//const float DEFAULT_AMP_CTR_FREQ_HZ = 520.8; //copied from SqWaveGen.ino
//const float DEFAULT_AMP_CTR_FREQ_HZ = 521.1; //number reqd to get 520.8Hz output
//const float DEFAULT_AMP_CTR_FREQ_HZ = 522.0; //very slow rl
//const float DEFAULT_AMP_CTR_FREQ_HZ = 521.9; //med lr
//const float DEFAULT_AMP_CTR_FREQ_HZ = 521.98; //slow rl
//const float DEFAULT_SEC_PER_AMP_STEP = 10;

//swept freq variables
int FreqStart = 0;
int FreqEnd = 0;
int FreqSteps = 0;
float SecPerFreqStep = 0;
int OutLevelPct = 0;

//swept amplitude variables
int AmpStartPct = 0;
int AmpEndPct = 0;
int AmpSteps = 0;
float AmpCtrFreqHz = 0;
float SecPerAmpStep = 0;
float HalfCycleMicroSec;

//common variables
int DACoutHigh;
int DACoutLow;
int SqWvOutState = LOW;


char Instr[20];



void setup()
{
	Serial.begin(115200);
	Serial.println("In Setup() ");
	pinMode(SQWAVE_PIN, OUTPUT);
	pinMode(LED_PIN, OUTPUT);
	pinMode(DAC_PIN, OUTPUT); //analog output pin
	pinMode(DEMOD_SYNCH_OUT_PIN, OUTPUT); //07/11/17 added for triggering IR demod modle
	pinMode(DEMOD_SYNCH_IN_PIN, INPUT_PULLDOWN); //07/11/17 added for triggering IR demod modle
	pinMode(DEMOD_VALUE_READ_PIN, INPUT); //analog input from demod module

	digitalWrite(SQWAVE_PIN, LOW);
	digitalWrite(LED_PIN, LOW);
	digitalWrite(DEMOD_SYNCH_OUT_PIN, LOW); //initial state
	//initialize test mode and test parameters
	Serial.println("Swept Frequency/Amplitude Generator"); Serial.println();

	//decreases conversion time from ~15 to ~5uSec
	adc->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED);
	adc->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED);
	adc->setResolution(12);
	adc->setAveraging(1);

	//added 07/12/17 - pin 31/A12 is connected to ADC_1 *not* ADC_0!
	adc->setAveraging(16, ADC_1); // set number of averages
	adc->setResolution(12, ADC_1); // set bits of resolution
	adc->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED, ADC_1); // change the conversion speed
	adc->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED, ADC_1); // change the sampling speed
	//adc->setAveraging(16, ADC_1); // set number of averages
	//adc->setResolution(12, ADC_1); // set bits of resolution
	//adc->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED, ADC_1); // change the conversion speed
	//adc->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED, ADC_1); // change the sampling speed




	Serial.print("Choose Freq or Amp Sweep.  F/A (F)? ");
	while (Serial.available() == 0); //waits for input
	String res = Serial.readString().trim();
	Serial.println(res);
	if (!res.equalsIgnoreCase('A'))
	{
		////Get Sweep parameters
		//FreqStart = GetIntegerParameter("Start Freq?", DEFAULT_START_FREQ);
		//FreqEnd = GetIntegerParameter("End Freq?", DEFAULT_END_FREQ);
		//FreqSteps = GetIntegerParameter("Freq Steps?", DEFAULT_FREQ_STEPS);
		//SecPerFreqStep = GetFloatParameter("Seconds Per Step?", DEFAULT_SEC_PER_FREQ_STEP);
		//OutLevelPct = GetIntegerParameter("Output Level Pct?", DEFAULT_OUTPUT_PCT);
		FreqStart = DEFAULT_START_FREQ;
		FreqEnd = DEFAULT_END_FREQ;
		FreqSteps = DEFAULT_FREQ_STEPS;
		SecPerFreqStep = DEFAULT_SEC_PER_FREQ_STEP;
		OutLevelPct = DEFAULT_OUTPUT_PCT;

		Serial.print("Sweep Parameters are: "); 
		Serial.print("Start = "); Serial.print(FreqStart);
		Serial.print(", End = "); Serial.print(FreqEnd);
		Serial.print(", Steps = "); Serial.print(FreqSteps);
		Serial.println();
		Serial.print("SecPerFreqStep = "); Serial.print(SecPerFreqStep);
		Serial.print(", OutputLevel = "); Serial.print(OutLevelPct);
		Serial.println();

		Serial.print("Start Y/N (Y)? ");
		while (Serial.available() == 0); //waits for input
		String res = Serial.readString().trim();
		Serial.println(res);
		if (!res.equalsIgnoreCase('N'))
		{
			Serial.println("Triggering IR Demod ....");
			digitalWrite(DEMOD_SYNCH_OUT_PIN, HIGH); //trigger IR demod start
			//while (digitalRead(DEMOD_SYNCH_IN_PIN) == LOW)
			//{
			//	//infinite loop to wait for trigger from IR demod module
			//	Serial.println("Waiting for IR Demod Response....");
			//	delay(1000);
			//}

			float freqstepHz = (float)(FreqEnd - FreqStart) / (float)(FreqSteps);
			DACoutHigh = 4096;
			DACoutLow = DACoutHigh*OutLevelPct / 100;
			//07/11/17 now doing only one sweep, but it is synched with demod
			//for (int i = 0; i < 10; i++)
			{
				//Serial.print("Iteration # "); Serial.println(i + 1);
				Serial.println("Starting....");
				Serial.println("Step\tFreq\tValue");

				for (int i = 0; i <= FreqSteps; i++)
				{
					float freqHz = FreqStart + i*freqstepHz;
					//Serial.print("Step "); Serial.print(i + 1); Serial.print(" Freq = "); Serial.println(freqHz);

					//compute required elapsed time for square wave transitions
					HalfCycleMicroSec = 0.5e6 / freqHz;
					myTimer.begin(SqwvGen, HalfCycleMicroSec);
					delay(SecPerFreqStep*1000);
					//myTimer.end(); //c/o 07/29/17

					//read & print the analog voltage
					int FinalVal = adc->analogRead(DEMOD_VALUE_READ_PIN); //0-4096
					Serial.print(i+1); Serial.print("\t");
					Serial.print(freqHz); Serial.print("\t");
					Serial.print(FinalVal);
					Serial.println();
				}
					myTimer.end(); //07/29/17 moved outside of freq step loop
			}
				//delay(3000);
		}
	}
	else//must be amplitude sweep
	{
		//AmpStartPct = GetIntegerParameter("Start Amp Pct?", DEFAULT_START_AMP_PCT);
		//AmpEndPct = GetIntegerParameter("End Amp Pct?", DEFAULT_END_AMP_PCT);
		//AmpSteps = GetIntegerParameter("Amp Steps?", DEFAULT_AMP_STEPS);
		//SecPerAmpStep = GetFloatParameter("Sec Per Step?", DEFAULT_SEC_PER_AMP_STEP);
		//AmpCtrFreqHz = GetFloatParameter("Output Ctr Freq HZ?", DEFAULT_AMP_CTR_FREQ_HZ);
		AmpStartPct = DEFAULT_START_AMP_PCT;
		AmpEndPct = DEFAULT_END_AMP_PCT;
		AmpSteps = DEFAULT_AMP_STEPS;
		SecPerAmpStep = DEFAULT_SEC_PER_AMP_STEP;
		AmpCtrFreqHz = DEFAULT_AMP_CTR_FREQ_HZ;

		Serial.print("Sweep Parameters are: ");
		Serial.print("Start = "); Serial.print(AmpStartPct);
		Serial.print(", End = "); Serial.print(AmpEndPct);
		Serial.print(", Steps = "); Serial.print(AmpSteps);
		Serial.println();
		Serial.print("SecPerAmpStep = "); Serial.print(SecPerAmpStep);
		Serial.print(", Output Ctr Freq = "); Serial.print(AmpCtrFreqHz);
		Serial.println();

		//Serial.print("Start Y/N (Y)? ");
		//while (Serial.available() == 0); //waits for input
		//String res = Serial.readString().trim();
		//Serial.println(res);
		//if (!res.equalsIgnoreCase('N'))
		{
			Serial.println("Triggering IR Demod ....");
			digitalWrite(DEMOD_SYNCH_OUT_PIN, HIGH); //trigger IR demod start
													 //while (digitalRead(DEMOD_SYNCH_IN_PIN) == LOW)
													 //{
													 //	//infinite loop to wait for trigger from IR demod module
													 //	Serial.println("Waiting for IR Demod Response....");
													 //	delay(1000);
													 //}

			//float AmpStepPct = (float)(AmpEndPct - AmpStartPct) / (float)(AmpSteps - 1);
			float AmpStepPct = (AmpSteps > 1)? (float)(AmpEndPct - AmpStartPct) / (float)(AmpSteps - 1): 0;
			DACoutHigh = 4096;
			//int DACoutLow = 0;

			//07/11/17 now doing only one sweep, but it is synched with demod
			//for (int i = 0; i < 10; i++)
			{
				//compute required elapsed time for square wave transitions
				HalfCycleMicroSec = 0.5e6 / AmpCtrFreqHz;
				Serial.print("HalfCycleMicroSec = "); Serial.println(HalfCycleMicroSec);
				Serial.print("SecPerAmpStep = "); Serial.println(SecPerAmpStep);

				//Serial.print("Iteration # "); Serial.println(i + 1);
				Serial.println("Starting....");
				Serial.println("Step\tAmpPct\tValue");

				for (int i = 0; i < AmpSteps; i++)
				{
					float AmpPct = AmpStartPct + i*AmpStepPct;
					//Serial.print("Step "); Serial.print(i + 1); Serial.print(" Amp = "); Serial.println(AmpPct);

					//output a square wave for the specified number of seconds
					long startMsec = millis();
					//while (stopMsec - startMsec < 1000 * SecPerAmpStep)
					while (millis() - startMsec < 1000 * SecPerAmpStep)
					{
						long startUsec = micros();
						digitalWrite(SQWAVE_PIN, HIGH);
						analogWriteDAC0(DACoutHigh); 
						digitalWrite(LED_PIN, HIGH);
						while (micros() - startUsec < HalfCycleMicroSec);//wait for rest of half-period to elapse

						digitalWrite(SQWAVE_PIN, LOW);
						analogWriteDAC0(DACoutHigh * (1- AmpPct/100) ); //level for this amp step
						digitalWrite(LED_PIN, LOW);
						while (micros() - startUsec < 2 * HalfCycleMicroSec);//wait for rest of half-period to elapse
					}

					//read & print the analog voltage
					int FinalVal = adc->analogRead(DEMOD_VALUE_READ_PIN); //0-4096
					Serial.print(i + 1); Serial.print("\t");
					Serial.print(AmpPct); Serial.print("\t");
					Serial.print(FinalVal);
					Serial.println();


					//delay(1000);
				}
			}
			delay(3000);
		}

	}

	Serial.println("Exiting - Bye!");
	digitalWrite(DEMOD_SYNCH_OUT_PIN, LOW);
	while (1);
}//setup()

// functions called by IntervalTimer should be short, run as quickly as
// possible, and should avoid calling other functions if possible.
void SqwvGen(void)
{
	if (SqWvOutState == LOW)
	{
		SqWvOutState = HIGH;
		digitalWrite(SQWAVE_PIN, HIGH);
		analogWriteDAC0(DACoutHigh);
	}
	else 
	{
		SqWvOutState = LOW;
		digitalWrite(SQWAVE_PIN, LOW);
		analogWriteDAC0(DACoutLow);
	}
}
void loop()
{

  /* add main program code here */

}

// check a string to see if it is numeric and accept Decimal point
//copied from defragster's post at https://forum.pjrc.com/threads/27842-testing-if-a-string-is-numeric
bool isNumeric(char * str) 
{
	byte ii = 0;
	bool RetVal = false;
	if ('-' == str[ii])
		ii++;
	while (str[ii])
	{
		if ('.' == str[ii]) {
			ii++;
			break;
		}
		if (!isdigit(str[ii])) return false;
		ii++;
		RetVal = true;
	}
	while (str[ii])
	{
		if (!isdigit(str[ii])) return false;
		ii++;
		RetVal = true;
	}
	return RetVal;
}

int GetIntegerParameter(String prompt, int defaultval)
{
	int param = 0;
	bool bDone = false;

	while (!bDone)
	{
		Serial.print(prompt); Serial.print(" ("); Serial.print(defaultval);Serial.print("): ");
		while (Serial.available() == 0); //waits for input
		String res = Serial.readString().trim();
		int reslen = res.length();
		if (reslen == 0) //user entered CR only
		{
			bDone = true;
			param = defaultval;
		}
		else
		{
			res.toCharArray(Instr, reslen + 1);
			if (isNumeric(Instr) && atoi(Instr) >= 0)
			{
				param = atoi(Instr);
				bDone = true;
			}
			else
			{
				Serial.print(Instr); Serial.println(" Is invalid input - please try again");
			}
		}
	}
	Serial.println(param);
	return param;
}

float GetFloatParameter(String prompt, float defaultval)
{
	float param = 0;
	bool bDone = false;

	while (!bDone)
	{
		Serial.print(prompt); Serial.print(" ("); Serial.print(defaultval); Serial.print("): ");
		while (Serial.available() == 0); //waits for input
		String res = Serial.readString().trim();
		int reslen = res.length();
		if (reslen == 0) //user entered CR only
		{
			bDone = true;
			param = defaultval;
		}
		else
		{
			res.toCharArray(Instr, reslen + 1);
			if (isNumeric(Instr) && atof(Instr) >= 0)
			{
				param = atof(Instr);
				bDone = true;
			}
			else
			{
				Serial.print(Instr); Serial.println(" Is invalid input - please try again");
			}
		}
	}
	Serial.println(param);
	return param;
}
