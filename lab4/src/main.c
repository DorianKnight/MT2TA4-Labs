/****** Robert Li, 2023  


1.The Fist External button (named extBtn1)  is connected to PC1 (cannot use PB1, for 429i-DISCO ,pb1 is used by LCD), ExtBtn1_Config()  //
					cannot use pin PB1, for 429i-DISCO ,pb1 is used by LCD. if use this pin, always interrupt by itself
					can not use pin PA1, used by gyro. if use this pin, never interrupt
					pd1----WILL ACT AS PC13, To trigger the RTC timestamp event
					....ONLY PC1 CAN BE USED TO FIRE EXTI1 !!!!
2. the Second external button (extBtn2) is conected to  PD2.  ExtBtn2_Config() --The pin PB2 on the board have trouble.
    when connect external button to the pin PB2, the voltage at this pin is not 3V as it is supposed to be, it is 0.3V, why?
		so changed to use pin PD2.
		PA2: NOT OK. (USED BY LCD??)
		PB2: OK.
		PC2: ok, BUT sometimes (every 5 times around), press pc2 will trigger exti1, which is configured to use PC1. (is it because of using internal pull up pin config?)
		      however, press PC1 does not affect exti 2. sometimes press PC2 will also affect time stamp (PC13)
		PD2: OK,     
		PE2:  OK  (PE3, PE4 PE5 , seems has no other AF function, according to the table in the manual for discovery board)
		PF2: NOT OK. (although PF2 is used by SDRAM, it affects LCD. press it, LCD will flick and displayed chars change to garbage)
		PG2: OK
		

**********************************************/


#include "main.h"


#define COLUMN(x) ((x) * (((sFONT *)BSP_LCD_GetFont())->Width))    //see font.h, for defining LINE(X)


//Timer data types
TIM_HandleTypeDef Tim3_Handle, Tim4_Handle;
TIM_OC_InitTypeDef Tim4_OCInitStructure;
uint16_t Tim3_PrescalerValue, Tim4_PrescalerValue;
__IO uint16_t Tim4_CCR;

//ADC data types
/* ADC handler declaration */
ADC_HandleTypeDef Adc3_Handle;
ADC_ChannelConfTypeDef sConfig;

//DMA handler declaration
DMA_HandleTypeDef DmaHandle;
DMA_HandleTypeDef hdma_adc;

const uint32_t BUFFER_SIZE = 32;
//static const uint32_t aSRC_Const_Buffer = ;//The spot where the raw value is stored

//static uint32_t aDST_Buffer[BUFFER_SIZE];

/* Variable used to get converted value */
uint32_t uhADCxConvertedValue = 0;






double ADC3ConvertedValue=0;


 volatile double  setPoint=23.5;  //NOTE: if declare as float, when +0.5, the compiler will give warning:
															//"single_precision perand implicitly converted to double-precision"
															//ALTHOUGH IT IS A WARNING, THIS WILL MAKE THE PROGRAM stop WORKing!!!!!!
															//if declare as float, when setPoint+=0.5, need to cast : as setPioint+=(float)0.5, otherwise,
															//the whole program will not work! even this line has  not been used/excuted yet
															//BUT, if declare as double, there is no such a problem.
															
	    														
	//You must then have code to enable the FPU hardware prior to using any FPU instructions. This is typically in the ResetHandler or SystemInit()

	//            system_stm32f4xx.c
  ////            void SystemInit(void)
	//							{
	//								/* FPU settings ------------------------------------------------------------*/
	//								#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
	//									SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
	//								#endif
	//							...											
	//		-----MODIFY the system_stm32f4xx.c in the above way, will also fix the "float" type problem mentioned above. 												
												
double measuredTemp;
	/*
	according to the reference manual of STM32f4Discovery, the Vref+ should =VDD=VDDA=3.0V, while Vref-=VSSA=0
	so the voltage of 3V is mapped to 12 bits ADC result, which ranges from 0 to 4095.  
	(althoug ADC_DR register is 16 bits, ADC converted result is just of 12bits)   
	so the voltage resolution is 3/4095  (v/per_reading_number)   
	since the temperature is amplified 3 times before it is fed in MCU, the actual voltage from the temperature sensor is: 
	ADC_convertedvalue* (3/4095) /3. 
	
	since the temperature sensor sensitity is 10mV/C ,that is:  (0.01V/C)
	so the temperature is: ADC_convertedvalue* (3/4095) /3 /0.01 
	
	NOTE: you'd better not, or cannot, let the MCU to the calculation becuase its power is limited. Otherwise your program may not work as expected. 
	*/




void  LEDs_Config(void);




void LCD_DisplayString(uint16_t LineNumber, uint16_t ColumnNumber, uint8_t *ptr);
void LCD_DisplayInt(uint16_t LineNumber, uint16_t ColumnNumber, int Number);
void LCD_DisplayFloat(uint16_t LineNumber, uint16_t ColumnNumber, float Number, int DigitAfterDecimalPoint);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);


void TIM3_Config(void);
void TIM4_Config(void);
void TIM4_OC_Config(void);

void ADC_Config(void);

void ExtBtn1_Config(void);
void ExtBtn2_Config(void);

static void SystemClock_Config(void);
static void Error_Handler(void);

//State machine flags

uint8_t PC1Pressed = 0;
uint8_t PD2Pressed = 0;
uint8_t Tim3Overflow = 0;
double tempSetPoint = 24;
uint8_t displayed = 0;
uint8_t timerOverflowCountPC1 = 0;
uint8_t timerOverflowCountPD2 = 0;
uint8_t timerOverflowCountTemp = 0;


int main(void){
		
		/* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
		HAL_Init();
		
	
	
		SystemClock_Config();
		
		HAL_InitTick(0x0000); // set systick's priority to the highest.
	
		//Configure LED3 and LED4 ======================================
		LEDs_Config();
	
		//configure the USER button as exti mode. 
		BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);   // BSP_functions in stm32f429i_discovery.c
																			
		
	
	
		BSP_LCD_Init();
		//BSP_LCD_LayerDefaultInit(uint16_t LayerIndex, uint32_t FB_Address);
		BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);   //LCD_FRAME_BUFFER, defined as 0xD0000000 in _discovery_lcd.h
															// the LayerIndex may be 0 and 1. if is 2, then the LCD is dark.
		//BSP_LCD_SelectLayer(uint32_t LayerIndex);
		BSP_LCD_SelectLayer(0);
		BSP_LCD_SetLayerVisible(0, ENABLE); 
		
		BSP_LCD_Clear(LCD_COLOR_WHITE);  //need this line, otherwise, the screen is dark	
		BSP_LCD_DisplayOn();
	 
		BSP_LCD_SetFont(&Font20);  //the default font,  LCD_DEFAULT_FONT, which is defined in _lcd.h, is Font24
	
		LCD_DisplayString(3, 2, (uint8_t *) "Lab4 Starter ");
		
		LCD_DisplayString(9, 0, (uint8_t *) "Current ");
		LCD_DisplayString(10, 0, (uint8_t *)"setPoint");
	
	
		LCD_DisplayFloat(9, 10, 24, 2);
		LCD_DisplayFloat(10, 10, 24, 2);
	
		//Configure external buttons
		ExtBtn1_Config();  //for the first External button
		ExtBtn2_Config();
		
		//Configure timers
		TIM3_Config();
		TIM4_Config();
		Tim4_CCR=32767;       //  Currently a duty cycle of 50%
		TIM4_OC_Config();
	
		
		
		//Configure ADC
		ADC_Config();
		HAL_ADC_PollForConversion(&Adc3_Handle,1000);
	
		
		
	while(1) {		
		HAL_Delay(10); //Needed or else the if statement won't evaluate - not sure why
		
		//Button logic to increment or decrement the set point temperature
		if (Tim3Overflow)
		{
			if (timerOverflowCountPC1 <8 && HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == 0)
			{
				timerOverflowCountPC1 ++;
				//LCD_DisplayString(13, 0, (uint8_t *)"GOTHERE");
			}
			
			else if (timerOverflowCountPD2 <8 && HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == 0)
			{
				timerOverflowCountPD2 ++;
				//LCD_DisplayString(13, 0, (uint8_t *)"GOTHERE");
			}
			
			//Check if the button has been released - if so, reset the timer overflow counter
			if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == 1)
			{
				timerOverflowCountPC1 = 0;
				PC1Pressed = 0;
			}
			
			//Check if the button has been released - if so, reset the timer overflow counter
			if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == 1)
			{
				timerOverflowCountPD2 = 0;
				PD2Pressed = 0;
			}
			
			if (timerOverflowCountPC1 >= 8 || timerOverflowCountPD2 >=8)
			{
				
				if (PD2Pressed)
				{
					//LCD_DisplayString(13, 0, (uint8_t *)"GOTHERE");
					//Decrement the temperature by half a degree
					tempSetPoint -= 1;
					
					//Display information needs to change - set flag
					displayed = 0;
					
					//Reset the flag if not pressed anymore
					if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == 1)
					{	
						PD2Pressed = 0;
					}
				}
				
				else if (PC1Pressed)
				{
					//Increment the temperature by half a degree
					tempSetPoint += 1;
					
					//Display information needs to change - set flag
					displayed = 0;
					
					//Reset the flag if not pressed anymore
					if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == 1)
					{	
						PC1Pressed = 0;
					}
				}
				
				//Update LCD display if not already up to date
				if (!displayed)
				{
					//Clear the line
					BSP_LCD_ClearStringLine(10);
					HAL_Delay(10); //Needed so that the text properly prints to screen
					
					//Write to the line
					LCD_DisplayString(10, 0, (uint8_t *)"setPoint");
					LCD_DisplayFloat(10, 10, tempSetPoint, 2);
					
					//LCD_DisplayFloat(11,0,ADC3ConvertedValue,2);
				}
				
				//Reset the overflow counter
				timerOverflowCountPC1 = 0;
				timerOverflowCountPD2 = 0;
			}
			//Reset the flag
			Tim3Overflow = 0;
		}





		
	} // end of while loop
	
}  //end of main


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 

	* 					Oscillator											=HSE
	*    				HSE frequencey 										=8,000,000   (8MHz)
	*      ----However, if the project is created by uVision, the default HSE_VALUE is 25MHz. thereore, need to define HSE_VALUE
	*						PLL Source											=HSE
  *            PLL_M                          = 4
  *            PLL_N                          = 72
  *            PLL_P                          = 2
  *            PLL_Q                          = 3
  *        --->therefore, PLLCLK =8MHz X N/M/P=72MHz   
	*            System Clock source            = PLL (HSE)
  *        --> SYSCLK(Hz)                     = 72,000,000
  *            AHB Prescaler                  = 1
	*        --> HCLK(Hz)                       = 72 MHz
  *            APB1 Prescaler                 = 2
	*        --> PCLK1=36MHz,  -->since TIM2, TIM3, TIM4 TIM5...are on APB1, thiese TIMs CLK is 36X2=72MHz
							 	
  *            APB2 Prescaler                 = 1
	*        --> PCLK1=72MHz 

  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 5
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();
  
  /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Activate the Over-Drive mode */
  HAL_PWREx_EnableOverDrive();
 
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

void LEDs_Config(void)
{
 /* Initialize Leds mounted on STM32F429-Discovery board */
	BSP_LED_Init(LED3);   //BSP_LED_....() are in stm32f4291_discovery.c
  BSP_LED_Init(LED4);
}




void LCD_DisplayString(uint16_t LineNumber, uint16_t ColumnNumber, uint8_t *ptr)
{  
  //here the LineNumber and the ColumnNumber are NOT  pixel numbers!!!
		while (*ptr!=NULL)
    {
				BSP_LCD_DisplayChar(COLUMN(ColumnNumber),LINE(LineNumber), *ptr); //new version of this function need Xpos first. so COLUMN() is the first para.
				ColumnNumber++;
			 //to avoid wrapping on the same line and replacing chars 
				if ((ColumnNumber+1)*(((sFONT *)BSP_LCD_GetFont())->Width)>=BSP_LCD_GetXSize() ){
					ColumnNumber=0;
					LineNumber++;
				}
					
				ptr++;
		}
}

void LCD_DisplayInt(uint16_t LineNumber, uint16_t ColumnNumber, int Number)
{  
  //here the LineNumber and the ColumnNumber are NOT  pixel numbers!!!
		char lcd_buffer[15];
		sprintf(lcd_buffer,"%d",Number);
	
		LCD_DisplayString(LineNumber, ColumnNumber, (uint8_t *) lcd_buffer);
}

void LCD_DisplayFloat(uint16_t LineNumber, uint16_t ColumnNumber, float Number, int DigitAfterDecimalPoint)
{  
  //here the LineNumber and the ColumnNumber are NOT  pixel numbers!!!
		char lcd_buffer[15];
		
		sprintf(lcd_buffer,"%.*f",DigitAfterDecimalPoint, Number);  //6 digits after decimal point, this is also the default setting for Keil uVision 4.74 environment.
	
		LCD_DisplayString(LineNumber, ColumnNumber, (uint8_t *) lcd_buffer);
}

//Configure Timer 3
//This timer will be used to determine if the buttons have been held down for 1/2 a second
void TIM3_Config(void)
{
	
	//Calculates the prescaler value for timer 3. We want the timer to overflow ever eigth a second which means that it needs to complete two cycles per second
	Tim3_Handle.Init.Period = 65535;
	Tim3_PrescalerValue = (uint32_t) (SystemCoreClock / (16*(Tim3_Handle.Init.Period + 1)))-1;
	//Tim3_PrescalerValue = (uint32_t) (SystemCoreClock / 10000) - 1;
	Tim3_Handle.Instance = TIM3;
	
	Tim3_Handle.Init.Prescaler = Tim3_PrescalerValue;
	Tim3_Handle.Init.ClockDivision = 0;
  Tim3_Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
	
	
	if(HAL_TIM_Base_Init(&Tim3_Handle) != HAL_OK) // this line need to call the callback function _MspInit() in stm32f4xx_hal_msp.c to set up peripheral clock and NVIC..
  {
    Error_Handler();
  }
	
	if(HAL_TIM_Base_Start_IT(&Tim3_Handle) != HAL_OK)   //the TIM_XXX_Start_IT function enable IT, and also enable Timer
																											//so do not need HAL_TIM_BASE_Start() any more.
  {
		//BSP_LED_Toggle(LED3);
    Error_Handler();
  }
	
	//BSP_LED_Toggle(LED4);
}

//Configure Timer4
//This timer will be used for the fan PWM
void TIM4_Config(void)
{
	//Calculate the prescaler value to have the TIM4 counter overflow 50 KHz
	
	Tim4_Handle.Init.Period = 65535; //This won't matter since we'll be using the output compare in Timer 4
	Tim4_PrescalerValue = (uint32_t) (SystemCoreClock / (50000 * (Tim4_Handle.Init.Period + 1)))-1;
	Tim4_Handle.Instance = TIM4;
	Tim4_Handle.Init.Prescaler = Tim4_PrescalerValue;
	Tim4_Handle.Init.ClockDivision = 0;
	Tim4_Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
}

//Configure the output compare for timer 4

void TIM4_OC_Config(void)
{
	Tim4_OCInitStructure.OCMode = TIM_OCMODE_TIMING;
	Tim4_OCInitStructure.Pulse = Tim4_CCR; //CCR is variable between 0 and 65535
	Tim4_OCInitStructure.OCPolarity = TIM_OCPOLARITY_HIGH;
	
	HAL_TIM_OC_Init(&Tim4_Handle);
	HAL_TIM_OC_ConfigChannel(&Tim4_Handle, &Tim4_OCInitStructure, TIM_CHANNEL_1);
	HAL_TIM_OC_Start_IT(&Tim4_Handle, TIM_CHANNEL_1);
}


void ExtBtn1_Config(void)     // for GPIO C pin 1
// can only use PA0, PB0... to PA4, PB4 .... because only  only  EXTI0, ...EXTI4,on which the 
	//mentioned pins are mapped to, are connected INDIVIDUALLY to NVIC. the others are grouped! 
		//see stm32f4xx.h, there is EXTI0_IRQn...EXTI4_IRQn, EXTI15_10_IRQn defined
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOB clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  
  /* Configure PA0 pin as input floating */
  GPIO_InitStructure.Mode =  GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull =GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_1;
	//GPIO_InitStructure.Speed=GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

	//__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);   //is defined the same as the __HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1); ---check the hal_gpio.h
	__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1);// after moving the chunk of code in the GPIO_EXTI callback from _it.c (before these chunks are in _it.c)
																					//the program "freezed" when start, suspect there is a interupt pending bit there. Clearing it solve the problem.
  /* Enable and set EXTI Line0 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI1_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);
}

void ExtBtn2_Config(void){  //**********PD2.***********

	GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOD clock */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  
  /* Configure PD2 pin as input floating */
  GPIO_InitStructure.Mode =  GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull =GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_2;
	//GPIO_InitStructure.Speed=GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);

	//__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);   //is defined the same as the __HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1); ---check the hal_gpio.h
	__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_2);// after moving the chunk of code in the GPIO_EXTI callback from _it.c (before these chunks are in _it.c)
																					//the program "freezed" when start, suspect there is a interupt pending bit there. Clearing it solve the problem.
  // Enable and set EXTI Line0 Interrupt to the lowest priority 
  HAL_NVIC_SetPriority(EXTI2_IRQn, 3, 1);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
}


//Configure the ADC
void ADC_Config(void)
{
	Adc3_Handle.Instance= ADC3;
  
  Adc3_Handle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
  Adc3_Handle.Init.Resolution = ADC_RESOLUTION_12B;
  Adc3_Handle.Init.ScanConvMode = DISABLE;
  Adc3_Handle.Init.ContinuousConvMode = ENABLE;
  Adc3_Handle.Init.DiscontinuousConvMode = DISABLE;
  Adc3_Handle.Init.NbrOfDiscConversion = 0;
  Adc3_Handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  Adc3_Handle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
  Adc3_Handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  Adc3_Handle.Init.NbrOfConversion = 1;
  Adc3_Handle.Init.DMAContinuousRequests = ENABLE;
  Adc3_Handle.Init.EOCSelection = DISABLE;
      
  if(HAL_ADC_Init(&Adc3_Handle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler(); 
  }
  
  /*##-2- Configure ADC regular channel ######################################*/  
  sConfig.Channel = ADC_CHANNEL_13;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  sConfig.Offset = 0;
  
  if(HAL_ADC_ConfigChannel(&Adc3_Handle, &sConfig) != HAL_OK)
  {
    /* Channel Configuration Error */
    Error_Handler(); 
  }

  /*##-3- Start the conversion process and enable interrupt ##################*/
  /* Note: Considering IT occurring after each number of ADC conversions      */
  /*       (IT by DMA end of transfer), select sampling time and ADC clock    */
  /*       with sufficient duration to not create an overhead situation in    */
  /*        IRQHandler. */ 
  //if(HAL_ADC_Start_DMA(&Adc3_Handle, (uint32_t*)&uhADCxConvertedValue, 1) != HAL_OK)
	if(HAL_ADC_Start(&Adc3_Handle) != HAL_OK)
  {
    /* Start Conversation Error */
    Error_Handler(); 
  }

}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	
  if(GPIO_Pin == KEY_BUTTON_PIN)  //GPIO_PIN_0
  {
			BSP_LED_Toggle(LED4);
  }
	
	
	if(GPIO_Pin == GPIO_PIN_1)
  {
		BSP_LED_Toggle(LED4);
		
		//Set flag
		PC1Pressed = 1;
	}  //end of PIN_1

	if(GPIO_Pin == GPIO_PIN_2)
  {
			BSP_LED_Toggle(LED4);
		
		//Set flag
		PD2Pressed = 1;
	} //end of if PIN_2	
	
	
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if((*htim).Instance==TIM3)
	{
		//Set flag
		Tim3Overflow = 1;
		BSP_LED_Toggle(LED3);
		
		timerOverflowCountTemp++;
		
		if (timerOverflowCountTemp >16) //ensures that temperature is only updated once per second to minimize screen glitching
		{
			//Reset the flag
			timerOverflowCountTemp = 0;
			ADC3ConvertedValue = HAL_ADC_GetValue(&Adc3_Handle)* 0.02442;
			
			//Display updated temperature to LCD
			BSP_LCD_ClearStringLine(9);
			HAL_Delay(10);
			LCD_DisplayString(9, 0, (uint8_t *) "Current ");
			LCD_DisplayFloat(9,10,ADC3ConvertedValue,2);
		}
	}
}


void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim) //see  stm32fxx_hal_tim.c for different callback function names. 
{																																//for timer4 
	//	if ((*htim).Instance==TIM4) {
			
	//	}	
		//clear the timer counter!  in stm32f4xx_hal_tim.c, the counter is not cleared after  OC interrupt
		__HAL_TIM_SET_COUNTER(htim, 0x0000);   //this maro is defined in stm32f4xx_hal_tim.h
	
}
 
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef * htim){  //this is for TIM3_pwm
	
	//__HAL_TIM_SET_COUNTER(htim, 0x0000);  not necessary
}


static void Error_Handler(void)
{
  /* Turn LED4 on */
  BSP_LED_On(LED4);
  while(1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif
/**
  * @}
  */

/**
  * @}
  */



