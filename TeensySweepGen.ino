const int SQWAVE_PIN = 33;
const int DAC_PIN = A21; //DAC0
const int LED_PIN = 13; //added 03/10/15

const int DEFAULT_START_FREQ = 25;//one octave below
const int DEFAULT_END_FREQ = 104;//one octave above
const int DEFAULT_FREQ_STEPS = 40;
const int DEFAULT_SEC_PER_STEP = 2;
const int DEFAULT_OUTPUT_PCT = 50;

//swept freq variables
int FreqStart = 0;
int FreqEnd = 0;
int FreqSteps = 0;
int SecPerStep = 0;
int OutLevelPct = 0;

//swept amplitude variables
int AmpStart = 0;
int AmpEnd = 0;
int AmpSteps = 0;

float sinceLastTransition;

char Instr[20];



void setup()
{
	Serial.begin(115200);
	Serial.println("In Setup() ");
	pinMode(SQWAVE_PIN, OUTPUT);
	pinMode(LED_PIN, OUTPUT);
	pinMode(DAC_PIN, OUTPUT); //analog output pin

	digitalWrite(SQWAVE_PIN, LOW);
	digitalWrite(LED_PIN, LOW);

	//initialize test mode and test parameters
	Serial.println("Swept Frequency/Amplitude Generator"); Serial.println();

	//Serial.print("Choose Freq or Amp Sweep.  F/A (F)? ");
	//while (Serial.available() == 0); //waits for input
	//String res = Serial.readString().trim();
	//Serial.println(res);
	//if (!res.equalsIgnoreCase('A'))
	{
		////Get Sweep parameters
		//FreqStart = GetIntegerParameter("Start Freq?", DEFAULT_START_FREQ);
		//FreqEnd = GetIntegerParameter("End Freq?", DEFAULT_END_FREQ);
		//FreqSteps = GetIntegerParameter("Freq Steps?", DEFAULT_FREQ_STEPS);
		//CyclesPerStep = GetIntegerParameter("Cycles Per Step?", DEFAULT_CYCLES_PER_STEP);
		//OutLevelPct = GetIntegerParameter("Output Level Pct?", DEFAULT_OUTPUT_PCT);
		FreqStart = DEFAULT_START_FREQ;
		FreqEnd = DEFAULT_END_FREQ;
		FreqSteps = DEFAULT_FREQ_STEPS;
		SecPerStep = DEFAULT_SEC_PER_STEP;
		OutLevelPct = DEFAULT_OUTPUT_PCT;

		Serial.print("Sweep Parameters are: "); 
		Serial.print("Start = "); Serial.print(FreqStart);
		Serial.print(", End = "); Serial.print(FreqEnd);
		Serial.print(", Steps = "); Serial.print(FreqSteps);
		//Serial.print(", CycPerStep = "); Serial.print(CyclesPerStep);
		Serial.print(", SecPerStep = "); Serial.print(SecPerStep);
		Serial.print(", OutputLevel = "); Serial.print(OutLevelPct);
		Serial.println();

		Serial.print("Start Y/N (Y)? ");
		while (Serial.available() == 0); //waits for input
		String res = Serial.readString().trim();
		Serial.println(res);
		if (!res.equalsIgnoreCase('N'))
		{
			float freqstepHz = (float)(FreqEnd - FreqStart) / (float)(FreqSteps - 1);
			int DACoutHigh = 4096;
			int DACoutLow = DACoutHigh*OutLevelPct / 100;
			for (int i = 0; i < 10; i++)
			{
				Serial.print("Iteration # "); Serial.println(i + 1);

				for (int i = 0; i < FreqSteps; i++)
				{
					float freqHz = FreqStart + i*freqstepHz;
					Serial.print("Step "); Serial.print(i + 1); Serial.print(" Freq = "); Serial.println(freqHz);

					//compute required elapsed time for square wave transitions
					sinceLastTransition = 0.5e6 / freqHz;
					Serial.print("StartFreq Squarewave Transition Time (uS) = "); Serial.println(sinceLastTransition);

					//output a square wave for the specified number of seconds
					long startMsec = millis();
					long stopMsec = millis();
					while (stopMsec - startMsec < 1000*SecPerStep)
					{
						digitalWrite(SQWAVE_PIN, HIGH);
						analogWriteDAC0(DACoutHigh);
						digitalWrite(LED_PIN, HIGH);
						delayMicroseconds((int)sinceLastTransition);

						digitalWrite(SQWAVE_PIN, LOW);
						analogWriteDAC0(DACoutLow);
						digitalWrite(LED_PIN, LOW);
						delayMicroseconds((int)sinceLastTransition);

						stopMsec = millis(); //update the stop timer
					}

					//delay(1000);
				}
			}
				delay(3000);
		}



	}

	Serial.println("Exiting - Bye!");
	while (1);
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
