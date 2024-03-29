/****** 

1. For GPIO pins, Both OD mode and PP mode can drive the motor! However, some pins cannot output  high in OD mode!!! 
   
2. The signals do not need to be inverted before being fed in H-bridge.   
*/


#include "main.h"


#define COLUMN(x) ((x) * (((sFONT *)BSP_LCD_GetFont())->Width))    //see font.h, for defining LINE(X)


void LCD_DisplayString(uint16_t LineNumber, uint16_t ColumnNumber, uint8_t *ptr);
void LCD_DisplayInt(uint16_t LineNumber, uint16_t ColumnNumber, int Number);
void LCD_DisplayFloat(uint16_t LineNumber, uint16_t ColumnNumber, float Number, int DigitAfterDecimalPoint);

void LEDs_Config(void);

void TIM3_Config(void);
void TIM3_OC_Config(void);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim);

void ExtBtn1_Config(void);
void ExtBtn2_Config(void);
void ExtBtn3_Config(void);

void A1Solenoid_Config(void);
void A2Solenoid_Config(void);
void B1Solenoid_Config(void);
void B2Solenoid_Config(void);

static void SystemClock_Config(void);
static void Error_Handler(void);

TIM_HandleTypeDef Tim3_Handle;
TIM_OC_InitTypeDef Tim3_OCInitStructure;
uint16_t Tim3_PrescalerValue;
uint16_t Tim3_CCR = 65535;
float stepTime = (5/48.0); //0.77083;

//Flag declaration
uint8_t directionFlag = 0; //0 corresponds to clockwise and 1 corresponds to counter clockwise
uint8_t stepFlag = 0; //0 corresponds to full stepping and 1 corresponds to half stepping
int8_t stepCounter = 0; //Counts the number of steps completed, there are 96 in total (0-95), if full stepping the step count increments by 2
uint8_t timerOverflow = 0; //Allows me to keep track when the timer overflows
uint8_t currentSolenoidStep = 0; //Allos me to keep track of which solenoid should be turned on
uint8_t repititionFlag = 0;
uint8_t periodMultiplier = 0;

//Stepper motor array
//{A1,B1,A2,B2}
uint8_t coilArray[8][4] = {
												{1,0,0,0},
												{1,1,0,0},
												{0,1,0,0},
												{0,1,1,0},
												{0,0,1,0},
												{0,0,1,1},
												{0,0,0,1},
												{1,0,0,1}
												};
uint8_t activeCoil[4];
												
int main(void){
	
		/* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
		HAL_Init();
		
	
		 /* Configure the system clock to 72 MHz */
		SystemClock_Config();
		
		HAL_InitTick(0x0000); // set systick's priority to the highest.
	
	
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
	
		LCD_DisplayString(2, 3, (uint8_t *)"Lab");
	
		LCD_DisplayInt(2, 8, 5);
			
		//Initializing the LEDS	
		LEDs_Config();
		
		//Configure GPIOs
		A1Solenoid_Config();
		A2Solenoid_Config();
		B1Solenoid_Config();
		B2Solenoid_Config();
		
		//Configure the buttons
		BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI); //User button
		ExtBtn1_Config(); //PC1
		ExtBtn2_Config(); //PD2
		ExtBtn3_Config(); //PC3
		
		//Configuring the timer
		TIM3_Config();
		TIM3_OC_Config();
		//LCD_DisplayString(10, 3, (uint8_t *)"Testing line");
		
		while(1) {
			HAL_Delay(10);
			if (timerOverflow == 1 && repititionFlag == 0)
			{
				//Execute motor step
				
				currentSolenoidStep = stepCounter %8; //Since there are only 8 options in the array all 96 steps have to be mapped to an element in the array. 
																							//Steps 0, 8 and 16 are all the same from the perspective of the solenoids
				
				switch (currentSolenoidStep) //Select which which solenoid needs to be turned on or off
				{
					case 0:
						activeCoil[0] = coilArray[0][0];
						activeCoil[1] = coilArray[0][1];
						activeCoil[2] = coilArray[0][2];
						activeCoil[3] = coilArray[0][3];
						break;
					case 1:
						activeCoil[0] = coilArray[1][0];
						activeCoil[1] = coilArray[1][1];
						activeCoil[2] = coilArray[1][2];
						activeCoil[3] = coilArray[1][3];
						break;
					case 2:
						activeCoil[0] = coilArray[2][0];
						activeCoil[1] = coilArray[2][1];
						activeCoil[2] = coilArray[2][2];
						activeCoil[3] = coilArray[2][3];
						break;
					case 3:
						activeCoil[0] = coilArray[3][0];
						activeCoil[1] = coilArray[3][1];
						activeCoil[2] = coilArray[3][2];
						activeCoil[3] = coilArray[3][3];
						break;
					case 4:
						activeCoil[0] = coilArray[4][0];
						activeCoil[1] = coilArray[4][1];
						activeCoil[2] = coilArray[4][2];
						activeCoil[3] = coilArray[4][3];
						break;
					case 5:
						activeCoil[0] = coilArray[5][0];
						activeCoil[1] = coilArray[5][1];
						activeCoil[2] = coilArray[5][2];
						activeCoil[3] = coilArray[5][3];
						break;
					case 6:
						activeCoil[0] = coilArray[6][0];
						activeCoil[1] = coilArray[6][1];
						activeCoil[2] = coilArray[6][2];
						activeCoil[3] = coilArray[6][3];
						break;
					case 7:
						activeCoil[0] = coilArray[7][0];
						activeCoil[1] = coilArray[7][1];
						activeCoil[2] = coilArray[7][2];
						activeCoil[3] = coilArray[7][3];
						break;
					default:
						BSP_LED_Toggle(LED4); //Something has gone wrong - signal to operator
				}
				
				//Clear screen
				BSP_LCD_ClearStringLine(4);
				BSP_LCD_ClearStringLine(5);
				BSP_LCD_ClearStringLine(6);
				BSP_LCD_ClearStringLine(7);
				BSP_LCD_ClearStringLine(8);
				BSP_LCD_ClearStringLine(9);
				
				
				//Update screen display
				LCD_DisplayInt(4, 1, activeCoil[0]);
				LCD_DisplayInt(4, 3, activeCoil[1]);
				LCD_DisplayInt(4, 5, activeCoil[2]);
				LCD_DisplayInt(4, 7, activeCoil[3]);
				LCD_DisplayString(7, 1, (uint8_t*) "Period:");
				LCD_DisplayInt(7, 9, stepTime*48*(periodMultiplier+1));
				LCD_DisplayString(8, 1, (uint8_t*) "Step:");
				LCD_DisplayInt(8, 7, stepCounter);
				LCD_DisplayString(9,1, (uint8_t*) "Reps left");
				LCD_DisplayInt(9, 11, repititionFlag);
				
				if (stepFlag == 0)
				{
					LCD_DisplayString(5, 1,(uint8_t*) "Full Stepping");
				}
				else
				{
					LCD_DisplayString(5, 1,(uint8_t*) "Half Stepping");
				}
				
				if (directionFlag == 0)
				{
					LCD_DisplayString(6, 1,(uint8_t*) "CW");
				}
				else
				{
					LCD_DisplayString(6, 1,(uint8_t*) "CCW");
				}
				
				//Now that we know shich solenoids need to be turned on or off, set the correct pins on the gpio
				(activeCoil[0]) ? HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET) : HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); //Write A1 pin
				(activeCoil[1]) ? HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET) : HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); //Write A2 pin
				(activeCoil[2]) ? HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET) : HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET); //Write B1 pin
				(activeCoil[3]) ? HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET) : HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET); //Write B2 pin
				
				
				//Increment step counter
				if (stepFlag == 0 && directionFlag == 1) //Full stepping and counter clockwise
				{
					//If you're currently at a "half step" go to the nearest full step
					if (stepCounter %2 == 1)
					{
						stepCounter ++;
					}
					
					else
					{
						stepCounter +=2;
					}
				}
				
				else if(stepFlag == 1 && directionFlag == 1) //Half stepping and counter clockwise direction
				{
					stepCounter ++;
				}
				
				else if (stepFlag == 0 && directionFlag == 0) //Full stepping and clockwise
				{
					//If you're currently at a "half step" go to the nearest full step
					if (stepCounter %2 == 1)
					{
						stepCounter --;
					}
					
					else
					{
						stepCounter -=2;
					}
				}
				
				else if(stepFlag == 1 && directionFlag == 0) //Half stepping and clockwise direction
				{
					stepCounter --;
				}

				
				//Handling overflow
				//If you've completed a full revolution counter clockwise, reset the step counter
				if (stepCounter >95 && directionFlag == 1)
				{
					stepCounter = 0;
				}
				
				//If you've completed a full revolution clockwise, reset your step counter
				else if (stepCounter < 0 && directionFlag == 0)
				{
					stepCounter += 96; //If you're at -1 you'll end up at 95, if you're at -2 you'll end up at 94
				}
				
				//Reset the repitition counter flag
				repititionFlag = periodMultiplier;
			}
			
			else if (timerOverflow == 1)
			{
				BSP_LCD_ClearStringLine(9);
				LCD_DisplayString(9,1, (uint8_t*) "Reps left");
				LCD_DisplayInt(9, 11, repititionFlag);
				repititionFlag --;
			}
				
				
			//BSP_LED_Toggle(LED3);
			//Reset flag
			timerOverflow = 0;			
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
  RCC_OscInitStruct.PLL.PLLQ = 2;
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

void LEDs_Config(void)
{
 /* Initialize Leds mounted on STM32F429-Discovery board */
	BSP_LED_Init(LED3);   //BSP_LED_....() are in stm32f4291_discovery.c
  BSP_LED_Init(LED4);
}

//Used to control time between steps
void TIM3_Config(void)
{
	Tim3_Handle.Init.Period = 65535;
	//Calculates the prescaler value for timer 3. We want the timer to overflow 16 times a second
	Tim3_PrescalerValue = (uint32_t) (stepTime*SystemCoreClock / ((Tim3_Handle.Init.Period + 1)))-1;
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

void TIM3_OC_Config(void)
{
	Tim3_OCInitStructure.OCMode = TIM_OCMODE_TOGGLE;
	Tim3_OCInitStructure.Pulse = Tim3_CCR;
	Tim3_OCInitStructure.OCPolarity = TIM_OCPOLARITY_HIGH;
	
	HAL_TIM_OC_Init(&Tim3_Handle);
	HAL_TIM_OC_ConfigChannel(&Tim3_Handle, &Tim3_OCInitStructure, TIM_CHANNEL_2);
	HAL_TIM_OC_Start_IT(&Tim3_Handle, TIM_CHANNEL_2);
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


void ExtBtn3_Config(void){  //**********PC3.***********

	GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOC clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  
  /* Configure PD2 pin as input floating */
  GPIO_InitStructure.Mode =  GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull =GPIO_PULLUP;
  GPIO_InitStructure.Pin = GPIO_PIN_3;
	//GPIO_InitStructure.Speed=GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

	//__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);   //is defined the same as the __HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_1); ---check the hal_gpio.h
	__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_3);// after moving the chunk of code in the GPIO_EXTI callback from _it.c (before these chunks are in _it.c)
																					//the program "freezed" when start, suspect there is a interupt pending bit there. Clearing it solve the problem.
  // Enable and set EXTI Line0 Interrupt to the lowest priority 
  HAL_NVIC_SetPriority(EXTI3_IRQn, 3, 2);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}


void A1Solenoid_Config(void) //PB0
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.Pin = GPIO_PIN_0;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); //Write pins low once initialized
}

void A2Solenoid_Config(void) //PB1
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.Pin = GPIO_PIN_1;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); //Write pins low once initialized
}

void B1Solenoid_Config(void) //PB2
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.Pin = GPIO_PIN_2;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET); //Write pins low once initialized
}

void B2Solenoid_Config(void) //PC5
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.Pin = GPIO_PIN_5;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET); //Write pins low once initialized
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
		//Clock wise or counter clock wise
		if(GPIO_Pin == KEY_BUTTON_PIN)  //GPIO_PIN_0
		{
			BSP_LED_Toggle(LED3);
			if (directionFlag == 0)
			{
				directionFlag = 1;
			}
			else
			{
				directionFlag = 0;
			}
		}
		
		//Full step or half step
		if(GPIO_Pin == GPIO_PIN_1)
		{
				BSP_LED_Toggle(LED3);
			if (stepFlag == 0)
			{
				stepFlag = 1;
				
				//Reset the output compare
				Tim3_CCR = 65536/2;
				Tim3_OCInitStructure.Pulse = Tim3_CCR;
				__HAL_TIM_SET_COMPARE(&Tim3_Handle, TIM_CHANNEL_2, Tim3_OCInitStructure.Pulse);
			}
			else
			{
				stepFlag = 0;
				Tim3_CCR = 65535;
				Tim3_OCInitStructure.Pulse = Tim3_CCR;
				__HAL_TIM_SET_COMPARE(&Tim3_Handle, TIM_CHANNEL_2, Tim3_OCInitStructure.Pulse);
			}
			
		}  //end of PIN_1

		//Speed up stepper motor
		if(GPIO_Pin == GPIO_PIN_2) {
			periodMultiplier ++;
		} //end of if PIN_2	
		

		//Slow down stepper motor
		if(GPIO_Pin == GPIO_PIN_3)
		{
			if (periodMultiplier >0)
			{
				periodMultiplier --;
			}          		
		} //end of if PIN_3
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if((*htim).Instance==TIM3)
	{
		BSP_LED_Toggle(LED3);
	}
}


void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef * htim) //see  stm32fxx_hal_tim.c for different callback function names. 
{																																//for timer4 
				
	BSP_LED_Toggle(LED4);
	
	//Set flag
	timerOverflow = 1;
	
	//clear the timer counter!  in stm32f4xx_hal_tim.c, the counter is not cleared after  OC interrupt
	__HAL_TIM_SET_COUNTER(htim, 0x0000);   //this maro is defined in stm32f4xx_hal_tim.h

			
	
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



