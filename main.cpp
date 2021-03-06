// pin 19 (Yellow): CLK
// pin 20 (Orange): SI of x axis
// pin 18 (Blue)  : SI of y axis
// pin 3 (Brown)  : AO of x axis
// pin 4 (Green)  : AO of y axis
// Red            : 5V
// White          : Gnd

#include <wirish/wirish.h>
#include <libmaple/adc.h>
#include <string.h>

// We'll use timer 2
HardwareTimer timer(2);

#define COM SerialUSB // use this for Maple

unsigned long start = 0;
unsigned long stop = 0;
unsigned long pixelIndex = 0;
uint32 pixelVal_x[129];
uint32 pixelVal_y[129];
unsigned long sum_x = 0;
unsigned long sum_y = 0;
double threshold_x = 0;
double threshold_y = 0;
uint8 xy_buffer[32];

boolean dispReg = false;
boolean dispWelcome = false;
boolean dispValue = false;
boolean dispNormal = false;
boolean dispTiming = false;
boolean startSample = false;

int SI_x = 20;
int SI_y = 18;
int CLK = 19;
int AO_x = 3;   // channel 8
int AO_y = 4;   // channel 7

// GPIO
gpio_dev *CLK_dev;
uint8 CLK_pin;
gpio_dev *SI_x_dev;
uint8 SI_x_pin;
gpio_dev *SI_y_dev;
uint8 SI_y_pin;

uint32 adc_sequence = 0;
uint8 adc_length = 1; //The number of channels to be converted per ADC channel
uint8 ADC1_Sequence[] = { /*The ADC1 channels sequence, left to right*/
  8 }; /* channel 8 - pin 4. Set the sequence 1-6 for SQR3 (left will be first). Must top up to all 6 channels with zeros */
uint8 ADC2_Sequence[] = { /*The ADC2 channels sequence, left to right*/
  7 };  /* channel 7 - pin 3 */

/*
* calc_adc_sequence(ADCx_Sequence) converts the SQR3 6 channels' (each ADC1 and ADC2) list into
 * a valid 6 X 5=30 bits sequence format and returns that 30 bits number.
 * For more channels, repeat the same for SQR2, SQR1. (For SQR1 4 channels only!)
 */
uint32 calc_adc_sequence(uint8 adc_sequence_array[6])
{
  adc_sequence = 0;
  for (int i = 0; i<6; i++) // There are 6 available sequences in each SQR3 SQR2, and 4 in SQR1.
  {
    /*This function converts the array into one number by multiplying each 5-bits channel numbers
     by multiplications of 2^5
     */
    adc_sequence = adc_sequence + adc_sequence_array[i] * pow(2, (i * 5));
  }
  return adc_sequence;
} //end calc_adc_sequence

void timerHandler(void);

void setup()
{
  // Pause the timer while we're configuring it
  timer.pause();

  // Set up period
  timer.setPeriod(2000); // in microseconds

  // Set up an interrupt on channel 1
  timer.setMode(TIMER_CH1, TIMER_OUTPUT_COMPARE);
  timer.setCompare(TIMER_CH1, 1); // Interrupt 1 count after each update
  timer.attachInterrupt(TIMER_CH1, timerHandler);

  // Refresh the timer's count, prescale, and overflow
  timer.refresh();

  // Start the timer counting
  timer.resume();

  pinMode(BOARD_LED_PIN, OUTPUT);
  togglePin(BOARD_LED_PIN);

  // the following line is needed for Maple
  pinMode(AO_x, INPUT_ANALOG);
  pinMode(AO_y, INPUT_ANALOG);
  pinMode(CLK, OUTPUT);
  pinMode(SI_x, OUTPUT);
  pinMode(SI_y, OUTPUT);

  adc_init(ADC1); //rcc_clk_enable(ADC1->clk_id), Must be the first adc command!
  adc_init(ADC2);

  adc_enable(ADC1); //ADC_CR2_ADON_BIT = 1
  adc_enable(ADC2);

  adc_calibrate(ADC1); //Optional
  adc_calibrate(ADC2);

  // 采样率优化的关键寄存器参数
  adc_set_prescaler(ADC_PRE_PCLK2_DIV_2);
  adc_set_sample_rate(ADC1, ADC_SMPR_1_5);
  adc_set_sample_rate(ADC2, ADC_SMPR_1_5);

  adc_set_exttrig(ADC1, 1); //External trigger must be Enabled for both ADC.
  adc_set_exttrig(ADC2, 1); //External trigger must be Enabled for both ADC.
  adc_set_extsel(ADC1, ADC_ADC12_SWSTART); //External trigger Event
  adc_set_extsel(ADC2, ADC_ADC12_SWSTART); //External trigger Event

  adc_set_reg_seqlen(ADC1, adc_length); //The number of channels to be converted.
  adc_set_reg_seqlen(ADC2, adc_length);
  ADC1->regs->SQR3 |= calc_adc_sequence(ADC1_Sequence);
  ADC2->regs->SQR3 |= calc_adc_sequence(ADC2_Sequence);  

  // right alligned
  ADC1->regs->CR2 &= (0xfffff7ff);
  ADC2->regs->CR2 &= (0xfffff7ff);

  // DMA bit set, the key!!! I was stucked here for several days.
  // RM0008 page 219: Note: In dual ADC mode, to read the slave converted data on the master data register, the DMA 
  // bit must be enabled even if it is not used to transfer converted regular channel data.
  ADC1->regs->CR2 |= (1<<8);
  ADC2->regs->CR2 |= (1<<8);

  ADC1->regs->CR1 |= (6 << 16);       // 0b0110: Regular simultaneous mode only
  //  ADC2->regs->CR1 |= (6 << 16);     // For CR1 DUALMOD, These bits are reserved in ADC2 and ADC3. RM0008 page 229

  // digitalWrite() preparation work
  // CLK
  CLK_dev = PIN_MAP[CLK].gpio_device;
  CLK_pin = PIN_MAP[CLK].gpio_bit;
  // SI x
  SI_x_dev = PIN_MAP[SI_x].gpio_device;
  SI_x_pin = PIN_MAP[SI_x].gpio_bit;
  // SI y
  SI_y_dev = PIN_MAP[SI_y].gpio_device;
  SI_y_pin = PIN_MAP[SI_y].gpio_bit;
}

void sampleSensor()
{
  for(int i = 0; i<2; i++)
  {
    pixelIndex = 0;

    // digitalWrite(CLK, LOW);
    CLK_dev->regs->BSRR = (1U << CLK_pin)<<16;    // CLK set low
    SI_x_dev->regs->BSRR = (1U << SI_x_pin)<<16;  // SI x set low
    SI_y_dev->regs->BSRR = (1U << SI_y_pin)<<16;  // SI y set low
    SI_x_dev->regs->BSRR = (1U << SI_x_pin);      // SI x set high
    SI_y_dev->regs->BSRR = (1U << SI_y_pin);      // SI y set high
    CLK_dev->regs->BSRR = (1U << CLK_pin);        // CLK set high

    // start the 1st ADC sample and conversion
    ADC1->regs->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->regs->SR & ADC_SR_EOC))
      ;
    pixelVal_x[pixelIndex] = (uint16)(ADC1->regs->DR);                            // low 16 bits
    pixelVal_y[pixelIndex] = ((uint32)(ADC1->regs->DR & ADC_DR_ADC2DATA))>>16;    // high 16 bits

    SI_x_dev->regs->BSRR = (1U << SI_x_pin)<<16;  // SI x set low
    SI_y_dev->regs->BSRR = (1U << SI_y_pin)<<16;  // SI y set low
    CLK_dev->regs->BSRR = (1U << CLK_pin)<<16;    // CLK set low

    while (pixelIndex < 128)
    {
      pixelIndex++;

      // digitalWrite(CLK, HIGH);
      CLK_dev->regs->BSRR = (1U << CLK_pin);

      // pixelVal_x[pixelIndex] = analogRead(AO_x);
      ADC1->regs->CR2 |= ADC_CR2_SWSTART;
      while (!(ADC1->regs->SR & ADC_SR_EOC))
        ;
      pixelVal_x[pixelIndex] = (uint16)(ADC1->regs->DR);                            // low 16 bits
      pixelVal_y[pixelIndex] = ((uint32)(ADC1->regs->DR & ADC_DR_ADC2DATA))>>16;    // high 16 bits
      // the last one: pixelIndex = 128

      // digitalWrite(CLK, LOW);
      CLK_dev->regs->BSRR = (1U << CLK_pin)<<16;
    }
  }
}

void calcThreshold()
{
  sum_x = 0;
  sum_y = 0;
  threshold_x = 0;
  threshold_y = 0;

  for(int i=0;i<128;i++)
  {
    sum_x += pixelVal_x[i];
    sum_y += pixelVal_y[i];
  }
  threshold_x = sum_x / 128.0;
  threshold_y = sum_y / 128.0;
}

void digitize()
{
  memset(xy_buffer,0,sizeof(uint8)*32);

  for(int j=0;j<16;j++)
  {
    for(int i=0;i<8;i++)
    {
      if(pixelVal_x[i+j*8] >= threshold_x)
        xy_buffer[j] |= 1 << (7 - i);
//      else
//        xy_buffer[j] &= ~(1 << i);
    }
  }
  for(int j=0;j<16;j++)
  {
    for(int i=0;i<8;i++)
    {
      if(pixelVal_y[i+j*8] >= threshold_y)
        xy_buffer[j+16] |= 1 << (7 - i);
//      else
//        xy_buffer[j+16] &= ~(1 << i);
    }
  }
}

void print_registers()
{
  COM.print("\n PCLK2 = ");
  COM.println(PCLK2);
  COM.print("\n ADC1->regs->CR1 = ");
  COM.println(ADC1->regs->CR1, HEX);
  COM.print("\n ADC2->regs->CR1 = ");
  COM.println(ADC2->regs->CR1, HEX);
  COM.print("ADC1, CR2\t");
  COM.println(ADC1->regs->CR2, BIN);
  COM.print("ADC2, CR2\t");
  COM.println(ADC2->regs->CR2, BIN);
  COM.print("ADC1, SQR3\t");
  COM.println(ADC1->regs->SQR3, BIN);
  COM.print("ADC2, SQR3\t");
  COM.println(ADC2->regs->SQR3, BIN);
  COM.print("ADC1, SQR1\t");
  COM.println(ADC1->regs->SQR1, BIN);
  COM.print("ADC2, SQR1\t");
  COM.println(ADC2->regs->SQR1, BIN);
  COM.print("ADC1, SR\t");
  COM.println(ADC1->regs->SR, BIN);
  COM.println();
  COM.println();
}

void welcome_message()
{
  COM.println("-------------------------------------------------------------------");
  COM.println();
  COM.println("Welcome to the Maple structured light positioning demo / by Tom Xue");
  COM.println();
  COM.println("Real-time input parameters:");
  COM.println("'r' to display the relevant registers, stop by 'R'.");
  COM.println("'v' to display ADC read values, stop by 'V'.");
  COM.println("'n' to display the normal output used by C# program. Stop by 'N'.");
  COM.println("'t' to display timing info. Stop by 'T'.");
  COM.println();
  COM.println("Enter w to display this message");
  COM.println();
  dispWelcome = false;
}

void loop()
{
  //  COM.println("\nStarting loops:");

  if(startSample == true)
  {
    start = micros();
    sampleSensor();
    stop = micros();
    calcThreshold();
    digitize();
    startSample = false;
  }

  if(dispTiming)
  {
    COM.println("Stop loops:");
    COM.print("Elapsed Time: ");
    COM.print(stop - start);
    COM.print(" us (for ");
    COM.print(1*129*2);
    COM.println(" analog reads)");
    COM.print((stop-start)/(double)(1*129*2));
    COM.print(" us (for 1 sample) ");
    COM.print((stop-start)/(double)(1*2));
    COM.print(" us (for 1 sensor 1 loop: 129 pixels) ");
  }

  if(dispValue)
  {
    COM.println(" pixelVal_x = ");
    for(int k=0;k<128;k++)
    {
      COM.print(pixelVal_x[k]);
      COM.print("  ");
      if(k % 20 == 0)
        COM.println("");
    }

    COM.println("\n pixelVal_y = ");
    for(int k=0;k<128;k++)
    {
      COM.print(pixelVal_y[k]);
      COM.print("  ");
      if(k % 20 == 0)
        COM.println("");
    }

    COM.print("\n threshold x is ");
    COM.println(threshold_x);
    COM.print("threshold y is ");
    COM.println(threshold_y);
  }

  if(dispNormal)
  {
    for(int i=0;i<32;i++)
    {
      COM.print(xy_buffer[i]);
      if(i != 31)
        COM.print(",");
    }
    COM.println("");
  }

  if (dispReg) //Display the relevant registries
    print_registers();
  if(dispWelcome)
    welcome_message();

  while (COM.available())
  {
    uint8 input = COM.read();
    COM.println(input);
    switch (input)
    {
    case 'r':
      dispReg = true;
      break;
    case 'R':
      dispReg = false;
      break;
    case 'v':
      dispValue = true;
      break;
    case 'V':
      dispValue = false;
      break;
    case 'n':
      dispNormal = true;
      break;
    case 'N':
      dispNormal = false;
      break;
    case 't':
      dispTiming = true;
      break;
    case 'T':
      dispTiming = false;
      break;
    case 'l':
      togglePin(BOARD_LED_PIN);
      break; //Toggle test voltage on / off
    case 'w':
      dispWelcome = true;
    default:
      COM.print("Bad input");
      break;
    }
  }
}

void timerHandler(void)
{
  // within this handler, it must be light task
  // otherwise the chip goes to dead easily
//  togglePin(BOARD_LED_PIN);
  digitalWrite(BOARD_LED_PIN, HIGH);
  startSample = true;
}

// Force init to be called *first*, i.e. before static object allocation.
// Otherwise, statically allocated objects that need libmaple may fail.
// 下面的代码缺失的话，下载后maple mini串口设备消失，无法调试
__attribute__((constructor)) void premain() {
  init();
}

int main(void) {
  setup();
  while (true) {
    loop();
  }
  return 0;
}
