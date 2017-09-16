//G. Frank Paynter, PhD created 01 August 2017
//Frequency and amplitude sweep tool for testing frequency and/or amplitude sensitive devices
//Output is square wave frequency sweep at specified output level (peak-peak % of full-scale DAC output),
//or p-p amplitude sweep at specified frequency.  The output appears on Teensy pin A21 (DAC0).
//the output square wave is echoed to Teensy digital output pin 33 for frequency monitoring
//Teensy digital output pin 1 is toggled HIGH at the start, and LOW at the end of each sweep
//Measurement input is expected on analog input pin A12.  This pin is read once at the end of each sweep step
//Latest source can be found on GitHub at 

//Notes:
//	09/15/17 revised for Teensy 3.2
//		DAC_PIN changed from A21 to A14
//		TX_OUTPUT_PIN (waveform monitor pin) changed from 33 to 12 
//		IR_LED_GND_PIN eliminated

#include <ADC.h> //Analog-to-Digital library

ADC *adc = new ADC(); // adc object;
IntervalTimer myTimer; //added 07/23/17

//const int TX_OUTPUT_PIN = 33; //monitor output
//const int IR_LED_GND_PIN = 32; //bugfix - added 08/31/17
//const int DAC_PIN = A21; //DAC0: freq/amp sweep output pin

//09/15/17 rev for Teensy 3.2
const int TX_OUTPUT_PIN = 12; //monitor output
const int DAC_PIN = A14; //DAC0: freq/amp sweep output pin
//const int IR_LED_GND_PIN = 12;
const int DAC_OUT_HIGH_VAL = 4095;
const int LED_PIN = 13; //added 03/10/15
const int DEMOD_SYNCH_OUT_PIN = 1; //goes HIGH at start and LOW at end of each sweep
const int DEMOD_VALUE_READ_PIN = A0; 

const int DEFAULT_START_FREQ = 510;//8Hz below
const int DEFAULT_END_FREQ = 530;//8Hz above
const int DEFAULT_FREQ_STEPS = 10;
const float DEFAULT_SEC_PER_FREQ_STEP = 0.5;
const int DEFAULT_OUTPUT_PCT = 50;

const int DEFAULT_START_AMP_PCT = 0;
const int DEFAULT_END_AMP_PCT = 100;
const int DEFAULT_AMP_STEPS = 40;
const float DEFAULT_AMP_CTR_FREQ_HZ = 520.8; //copied from SqWaveGen.ino
const float DEFAULT_SEC_PER_AMP_STEP = 0.5;

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
	pinMode(TX_OUTPUT_PIN, OUTPUT);
	pinMode(LED_PIN, OUTPUT);
	pinMode(DAC_PIN, OUTPUT); //analog output pin
	pinMode(DEMOD_SYNCH_OUT_PIN, OUTPUT); //07/11/17 added for triggering IR demod modle
	pinMode(DEMOD_VALUE_READ_PIN, INPUT); //analog input from demod module

	digitalWrite(TX_OUTPUT_PIN, LOW);
	digitalWrite(LED_PIN, LOW);
	digitalWrite(DEMOD_SYNCH_OUT_PIN, LOW); //initial state

	//initialize test mode and test parameters
	Serial.println("Swept Frequency/Amplitude Generator"); Serial.println();

	//decreases conversion time from ~15 to ~5uSec
	adc->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED);
	adc->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED);
	adc->setResolution(12);
	adc->setAveraging(1);

	//09/15/17 intialize the DAC
	analogWriteResolution(12);

	while (1) //infinite loop.  User can exit at bottom
	{
		Serial.print("Choose Freq or Amp Sweep.  F/A (F)? ");
		while (Serial.available() == 0)
		{
			delay(100); //waits for input
		}
		String res = Serial.readString().trim();
		Serial.println(res);
		if (!res.equalsIgnoreCase('A'))
		{
			////Get Sweep parameters
			FreqStart = GetIntegerParameter("Start Freq?", DEFAULT_START_FREQ);
			FreqEnd = GetIntegerParameter("End Freq?", DEFAULT_END_FREQ);
			FreqSteps = GetIntegerParameter("Freq Steps?", DEFAULT_FREQ_STEPS);
			SecPerFreqStep = GetFloatParameter("Seconds Per Step?", DEFAULT_SEC_PER_FREQ_STEP);
			OutLevelPct = GetIntegerParameter("Output Level Pct?", DEFAULT_OUTPUT_PCT);

			//can bypass prompts by commenting out 5 lines above & uncommenting 5 lines below
			//FreqStart = DEFAULT_START_FREQ;
			//FreqEnd = DEFAULT_END_FREQ;
			//FreqSteps = DEFAULT_FREQ_STEPS;
			//SecPerFreqStep = DEFAULT_SEC_PER_FREQ_STEP;
			//OutLevelPct = DEFAULT_OUTPUT_PCT;

			Serial.print("Sweep Parameters are: ");
			Serial.print("Start = "); Serial.print(FreqStart);
			Serial.print(", End = "); Serial.print(FreqEnd);
			Serial.print(", Steps = "); Serial.print(FreqSteps);
			Serial.println();
			Serial.print("SecPerFreqStep = "); Serial.print(SecPerFreqStep);
			Serial.print(", OutputLevel = "); Serial.print(OutLevelPct);
			Serial.println();

			Serial.print("Start Y/N (Y)? ");
			while (Serial.available() == 0)
			{
				delay(100); //waits for input
			}

			String res = Serial.readString().trim();
			Serial.println(res);
			if (!res.equalsIgnoreCase('N'))
			{
				Serial.println("Send any key to stop sweep repeat");
				while (Serial.available() == 0) //do sweeps until user quits
				{
					Serial.print("Sent trigger to pin "); Serial.println(DEMOD_SYNCH_OUT_PIN);
					digitalWrite(DEMOD_SYNCH_OUT_PIN, HIGH); //trigger IR demod start

					float freqstepHz = (float)(FreqEnd - FreqStart) / (float)(FreqSteps - 1); //08/06/17 bugfix

					//09/16/17 change output sense
					DACoutLow = 0;
					DACoutHigh = OutLevelPct*DAC_OUT_HIGH_VAL / 100;

					Serial.println("Starting....");
					Serial.println("Step\tFreq\tValue");

					for (int i = 0; i < FreqSteps; i++)
					{
						float freqHz = FreqStart + i*freqstepHz;

						//compute required elapsed time for square wave transitions
						HalfCycleMicroSec = 0.5e6 / freqHz;

						//07/30/17 now using tni's technique for updating timer w/o reset 
						if (i == 0)
						{
							myTimer.begin(SqwvGen, HalfCycleMicroSec); //SqwvGen is name of ISR
						}
						else
						{
							updateInterval(myTimer, HalfCycleMicroSec);
						}

						delay(SecPerFreqStep * 1000);

						//read & print the analog voltage
						int RcvVal = adc->analogRead(DEMOD_VALUE_READ_PIN); //0-4096
						Serial.print(i + 1); Serial.print("\t");
						Serial.print(freqHz); Serial.print("\t");
						Serial.print(RcvVal);
						Serial.println();
					}
					myTimer.end(); //07/29/17 moved outside of freq step loop

					//toggle synch line
					digitalWrite(DEMOD_SYNCH_OUT_PIN, LOW);
					Serial.print("Disabled trigger on pin "); Serial.println(DEMOD_SYNCH_OUT_PIN);
					delay(500); //added 08/06/17 so can see transition on scope
				}
			}
		}
		else//must be amplitude sweep
		{
			AmpStartPct = GetIntegerParameter("Start Amp Pct?", DEFAULT_START_AMP_PCT);
			AmpEndPct = GetIntegerParameter("End Amp Pct?", DEFAULT_END_AMP_PCT);
			AmpSteps = GetIntegerParameter("Amp Steps?", DEFAULT_AMP_STEPS);
			SecPerAmpStep = GetFloatParameter("Sec Per Step?", DEFAULT_SEC_PER_AMP_STEP);
			AmpCtrFreqHz = GetFloatParameter("Output Ctr Freq HZ?", DEFAULT_AMP_CTR_FREQ_HZ);
			//AmpStartPct = DEFAULT_START_AMP_PCT;
			//AmpEndPct = DEFAULT_END_AMP_PCT;
			//AmpSteps = DEFAULT_AMP_STEPS;
			//SecPerAmpStep = DEFAULT_SEC_PER_AMP_STEP;
			//AmpCtrFreqHz = DEFAULT_AMP_CTR_FREQ_HZ;

			Serial.print("Sweep Parameters are: ");
			Serial.print("Start = "); Serial.print(AmpStartPct);
			Serial.print(", End = "); Serial.print(AmpEndPct);
			Serial.print(", Steps = "); Serial.print(AmpSteps);
			Serial.println();
			Serial.print("SecPerAmpStep = "); Serial.print(SecPerAmpStep);
			Serial.print(", Output Ctr Freq = "); Serial.print(AmpCtrFreqHz);
			Serial.println();

			Serial.print("Start Y/N (Y)? ");
			while (Serial.available() == 0)
			{
				delay(100); //waits for input
			}

			String res = Serial.readString().trim();
			Serial.println(res);
			if (!res.equalsIgnoreCase('N'))
			{
				Serial.println("Send any key to stop sweep repeat");
				while (Serial.available() == 0) //do sweeps until user quits
				{
					Serial.print("Sent trigger to pin "); Serial.println(DEMOD_SYNCH_OUT_PIN);
					digitalWrite(DEMOD_SYNCH_OUT_PIN, HIGH); //trigger IR demod start

					float AmpStepPct = (AmpSteps > 1) ? (float)(AmpEndPct - AmpStartPct) / (float)(AmpSteps - 1) : 0;

					//09/16/17 change output sense
					DACoutLow = 0;

					//compute required elapsed time for square wave transitions
					HalfCycleMicroSec = 0.5e6 / AmpCtrFreqHz;
					Serial.print("HalfCycleMicroSec = "); Serial.println(HalfCycleMicroSec);
					Serial.print("SecPerAmpStep = "); Serial.println(SecPerAmpStep);

					Serial.println("Starting....");
					Serial.println("Step\tAmpPct\tDACOut\tRcvValue");

					//09/15/17 revised for IntervalTimer use
					//07/30/17 now using tni's technique for updating timer w/o reset
					//09/15/17 for amp sweep, freq is constant, so just set it and let it run
					myTimer.begin(SqwvGen, HalfCycleMicroSec); //SqwvGen is name of ISR

					for (int i = 0; i < AmpSteps; i++)
					{
						float AmpPct = AmpStartPct + i*AmpStepPct;
						//DACoutLow = DACoutHigh * (1 - AmpPct / 100); //level for this amp step

						//09/16/17 change output sense
						DACoutHigh = AmpPct*DAC_OUT_HIGH_VAL / 100;
						Serial.printf("%d\t%2.2f\t%d\t", i + 1, AmpPct, DACoutHigh);
						delay(1000 * SecPerAmpStep);

						//get & print the analog voltage
						int RcvVal = adc->analogRead(DEMOD_VALUE_READ_PIN); //0-4096
						Serial.println(RcvVal);
					}

					//toggle synch line
					digitalWrite(DEMOD_SYNCH_OUT_PIN, LOW);
					Serial.print("Disabled trigger on pin "); Serial.println(DEMOD_SYNCH_OUT_PIN);
					delay(500); //added 08/06/17 so can see transition on scope
				}
			}
		}

		//clear the receive buffer
		while (Serial.available() != 0)
		{
			char getData = Serial.read();
			getData++; //just to avoid 'unused symbol' warning
		}

		Serial.print("Do Again Y/N (Y)? ");
		while (Serial.available() == 0)
		{
			delay(100); //waits for input
		}

		res = Serial.readString().trim();
		Serial.println(res);
		if (res.equalsIgnoreCase('N'))
		{
			Serial.println("Exiting - Bye!");
			while (1);
		}
	}
}//setup()

// functions called by IntervalTimer should be short, run as quickly as
// possible, and should avoid calling other functions if possible.
void SqwvGen(void)
{
	if (SqWvOutState == LOW)
	{
		SqWvOutState = HIGH;
		digitalWrite(TX_OUTPUT_PIN, HIGH);
		analogWriteDAC0(DACoutHigh);
	}
	else 
	{
		SqWvOutState = LOW;
		digitalWrite(TX_OUTPUT_PIN, LOW);
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

void updateInterval(IntervalTimer& timer, float period) 
{
	size_t channel_nr = IRQ_PIT_CH0 - timer.operator IRQ_NUMBER_t();
	if (channel_nr >= 4) return; // no PIT timer assigned

	KINETISK_PIT_CHANNEL_t& channel = (KINETISK_PIT_CHANNELS)[channel_nr];
	channel.LDVAL = (float)(F_BUS / 1000000) * period - 0.5f;
}