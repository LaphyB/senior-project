/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

#include "icm20948.h"
#include "stm32_seq.h"

#include "config.h"
#include "pins.h"

#include "gyro_filters.h"

#include "buttons.h"
#include "led.h"
#include "motor.h"
#include "gesture_detector.h"
#include "gyro_to_mouse.h"
#include "adaptive_analyzer.h"
#include "sensor_data.h"

#include "hids_app.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

IPCC_HandleTypeDef hipcc;

RNG_HandleTypeDef hrng;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

ICM20948_Data_t imu_data;
MagData_t mag_data;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_RF_Init(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_IPCC_Init(void);
static void MX_RNG_Init(void);
static void MX_RTC_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

void TakeData(SensorData_t *datastruct);
void PrintDisplay(SensorData_t *data, bool blockHID, MousePos_t mouse_pos, GestureType_t gesture);
void PrintAnalyzerView();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

volatile uint16_t adc_results[4] __attribute__((aligned(32)));

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Config code for STM32_WPAN (HSE Tuning must be done before system clock configuration) */
  MX_APPE_Config();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* IPCC initialisation */
  MX_IPCC_Init();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_RF_Init();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  if (MX_FATFS_Init() != APP_OK) {
    Error_Handler();
  }
  MX_RNG_Init();
  MX_RTC_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init code for STM32_WPAN */
  MX_APPE_Init();

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

	printf("Setting up device...\r\n");

	extern FATFS USERFatFs;
	extern FIL USERFile;
	extern char USERPath[4];

	printf("Loading Initial Configs...\r\n");
	Config_Init();
	printf("Initial Configs Loaded!\r\n");

	printf("Calibrating ADC...\r\n");
	if(HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
	{
	  printf("ADC Calibration failed!\r\n");
	  while(1);
	}
	printf("ADC calibrated successfully!\r\n");

  printf("Starting DMA...\r\n");
  if(HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_results, 4) != HAL_OK)
  {
	  printf("DMA start failed!\r\n");
	  while(1);
  }
  printf("DMA started successfully!\r\n");

  printf("Initializing ICM-20948...\r\n");
  if (ICM20948_Init(&hi2c1) != HAL_OK)
  {
	  printf("ICM-20948 initialization failed!\r\n");

  }
  printf("ICM-20948 initialized successfully!\r\n");

  printf("Verifying ICM-20948 connection...\r\n");
  uint8_t who_am_i;
  if (ICM20948_WhoAmI(&hi2c1, &who_am_i) != HAL_OK)
  {
	  printf("WHO_AM_I = 0x%02X (expected 0xEA)\r\n", who_am_i);
  }
  printf("ICM-20948 connection verified!\r\n");

  // Initialize magnetometer
  printf("Initializing magnetometer...\r\n");
  if (ICM20948_InitMagnetometer(&hi2c1) != HAL_OK)
  {
	  printf("Magnetometer initialization failed!\r\n");
  }
  printf("Magnetometer ready!\r\n");

  printf("Initializing SDCard Reader and Data Logger...\r\n");
  DataLogger_Init(&USERFatFs, &USERFile, USERPath);
  DataLogger_LoadConfig();  // overwrites defaults with saved values if file exists
  printf("SDCard Reader and Data Logger ready!\r\n");

  printf("Initializing modules...\r\n");

  GestureDetector_Init();

  printf("Initializing Variables...\r\n");

  static uint32_t last_led_toggle = 0;
  static bool motor_triggered = false;
  static uint8_t display_page = 0;

  // For data struct
  SensorData_t sensor_data;

  // For LPF filtering
  static float 	smooth_gyro_x = 0
		  	  , smooth_gyro_y = 0
		  	  , smooth_gyro_z = 0
  	  	  	  ,	smooth_mag_x = 0
  		  	  , smooth_mag_y = 0
  		  	  , smooth_mag_z = 0;
  static bool first = true;

  // Temp int for For cursor handling
  //int8_t mouse_x, mouse_y;
  bool blockHID = false;

  printf("Entering Main Loop!\r\n");

  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE END WHILE */
	MX_APPE_Process();
	UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);

    /* USER CODE BEGIN 3 */

	  // Update Timers
	  DataLogger_Update();
	  setMotorTimed_Update();

	  // Heartbeat
	  if (HAL_GetTick() - last_led_toggle >= 500)
	  {
	      toggleLED();
	      last_led_toggle = HAL_GetTick();
	  }

	  // Take data
	  TakeData(&sensor_data);

	  // Bias Correction
	  sensor_data.gyro_x -= (int16_t)gyro_bias_x;
	  sensor_data.gyro_y -= (int16_t)gyro_bias_y;

	  // Use buttons

	  if(!sensor_data.buttons.BUTTON1)
	  {
		  blockHID = true;
	  }
	  else
	  {
		  blockHID = false;
	  }

	  if (!sensor_data.buttons.BUTTON6)
	  {
	      if (!motor_triggered)
	      {
	          setMotorTimed(BOTH, 3000);
	          motor_triggered = true;
	      }
	  }
	  else
	  {
	      motor_triggered = false;
	  }

	  // JS1 - run analyzer
	  static bool js1_last = true;
	  bool js1_now = sensor_data.buttons.JOYSTICKBUTTON1;
	  if (js1_last == false && js1_now == true)
	      AdaptiveAnalyzer_Run();
	  js1_last = js1_now;

	  // JS2 - break config for demo
	  static bool js2_last = true;
	  bool js2_now = sensor_data.buttons.JOYSTICKBUTTON2;
	  if (js2_last == false && js2_now == true)
	      AdaptiveAnalyzer_BreakConfig();
	  js2_last = js2_now;

	  // Toggle view on BUTTON2 release
	  static bool btn2_last = true;
	  bool btn2_now = sensor_data.buttons.BUTTON4;
	  if (btn2_last == false && btn2_now == true)
	  {
	      display_page = (display_page + 1) % 3;
	      printf("\033[2J\033[H");
	  }
	  btn2_last = btn2_now;

	  // Apply LPF Filter / Smooth Data
	  if (first)
	  {
		  smooth_gyro_x = sensor_data.gyro_x;
		  smooth_gyro_y = sensor_data.gyro_y;
		  smooth_gyro_z = sensor_data.gyro_z;

		  smooth_mag_x = sensor_data.mag_x;
		  smooth_mag_y = sensor_data.mag_y;
		  smooth_mag_z = sensor_data.mag_z;

		  first = false;
	  }
	  else
	  {
		  smooth_gyro_x = LPF(sensor_data.gyro_x, smooth_gyro_x);
		  smooth_gyro_y = LPF(sensor_data.gyro_y, smooth_gyro_y);
		  smooth_gyro_z = LPF(sensor_data.gyro_z, smooth_gyro_z);

		  smooth_mag_x = LPF(sensor_data.mag_x, smooth_mag_x);
		  smooth_mag_y = LPF(sensor_data.mag_y, smooth_mag_y);
		  smooth_mag_z = LPF(sensor_data.mag_z, smooth_mag_z);
	  }

	  // Detect Gestures
	  GestureType_t gesture = GestureDetector_Update(&sensor_data);

	  if (gesture != GESTURE_NONE)
	  {
		  switch(gesture)
		  {
		  	  case GESTURE_NONE:
		  		  break;
		  	  case GESTURE_FLICK_LEFT:
			          setMotorTimed(BOTH, 100);
				   break;
			   case GESTURE_FLICK_RIGHT:
			          setMotorTimed(BOTH, 100);
				   break;
			   case GESTURE_FLICK_UP:
			          setMotorTimed(BOTH, 100);
				   break;
			   case GESTURE_FLICK_DOWN:
			          setMotorTimed(BOTH, 100);
				   break;
			   case GESTURE_ROTATE_CW:
			          setMotorTimed(BOTH, 100);
				   break;
			   case GESTURE_ROTATE_CCW:
			          setMotorTimed(BOTH, 100);
				   break;
		  }
	  }

	  bool gestureBlock = (GestureDetector_GetState() != STATE_IDLE);

	  // Handle cursor movement
	  	  MousePos_t mouse_pos = GyroMouse_Update( smooth_mag_x, smooth_mag_y, smooth_mag_z, blockHID);

	  	  // Send BLE HID mouse report
	  	  if (!blockHID)
	  	  {
	  		  static bool last_block = false;
	  		  static float accum_x = 0;
	  		  static float accum_y = 0;

	  		  if (last_block && !blockHID)
	  		      HIDSAPP_SendMouseReport(0, 0, 0);
	  		  last_block = blockHID;

	  		  if (!blockHID)
	  		  {
	  		      accum_x += -smooth_gyro_y / 20.0f;  // was 50, now faster
	  		      accum_y +=  smooth_gyro_x / 20.0f;

	  		      // Clamp to prevent huge jumps
	  		      if (accum_x > 20)  accum_x = 20;
	  		      if (accum_x < -20) accum_x = -20;
	  		      if (accum_y > 20)  accum_y = 20;
	  		      if (accum_y < -20) accum_y = -20;

	  		      int8_t report_x = (int8_t)accum_x;
	  		      int8_t report_y = (int8_t)accum_y;

	  		      accum_x -= report_x;
	  		      accum_y -= report_y;

	  		      if (report_x != 0 || report_y != 0)
	  		          HIDSAPP_SendMouseReport(report_x, report_y, 0);
	  		  }
	  	  }

	  // Display
	  switch(display_page)
	  {
	      case 0:
	          break;
	      case 1:
	          PrintDisplay(&sensor_data, blockHID, mouse_pos, gesture);
	          break;
	      case 2:
	          PrintAnalyzerView();
	          break;
	  }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI1
                              |RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4|RCC_CLOCKTYPE_HCLK2
                              |RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS|RCC_PERIPHCLK_RFWAKEUP;
  PeriphClkInitStruct.RFWakeUpClockSelection = RCC_RFWKPCLKSOURCE_LSI;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSI;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN Smps */

  /* USER CODE END Smps */
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_19CYCLES_5;
  hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_19CYCLES_5;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.NbrOfConversion = 4;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00C12166;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief IPCC Initialization Function
  * @param None
  * @retval None
  */
static void MX_IPCC_Init(void)
{

  /* USER CODE BEGIN IPCC_Init 0 */

  /* USER CODE END IPCC_Init 0 */

  /* USER CODE BEGIN IPCC_Init 1 */

  /* USER CODE END IPCC_Init 1 */
  hipcc.Instance = IPCC;
  if (HAL_IPCC_Init(&hipcc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IPCC_Init 2 */

  /* USER CODE END IPCC_Init 2 */

}

/**
  * @brief RF Initialization Function
  * @param None
  * @retval None
  */
static void MX_RF_Init(void)
{

  /* USER CODE BEGIN RF_Init 0 */

  /* USER CODE END RF_Init 0 */

  /* USER CODE BEGIN RF_Init 1 */

  /* USER CODE END RF_Init 1 */
  /* USER CODE BEGIN RF_Init 2 */

  /* USER CODE END RF_Init 2 */

}

/**
  * @brief RNG Initialization Function
  * @param None
  * @retval None
  */
static void MX_RNG_Init(void)
{

  /* USER CODE BEGIN RNG_Init 0 */

  /* USER CODE END RNG_Init 0 */

  /* USER CODE BEGIN RNG_Init 1 */

  /* USER CODE END RNG_Init 1 */
  hrng.Instance = RNG;
  hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
  if (HAL_RNG_Init(&hrng) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RNG_Init 2 */

  /* USER CODE END RNG_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = CFG_RTC_ASYNCH_PRESCALER;
  hrtc.Init.SynchPrediv = CFG_RTC_SYNCH_PRESCALER;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.SubSeconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the WakeUp
  */
  //if (HAL_RTCEx_SetWakeUpTimer(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
  //{
  //  Error_Handler();
  //}

  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);

  /*Configure GPIO pins : PB8 PB9 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA6 PA8 PA11
                           PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_11
                          |GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PE4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

void TakeData(SensorData_t *datastruct)
{
	datastruct->timestamp_ms = HAL_GetTick();

	datastruct->buttons = ReadButtons();

	datastruct->js1_x = adc_results[0];
	datastruct->js1_y = adc_results[1];
	datastruct->js2_x = adc_results[2];
	datastruct->js2_y = adc_results[3];

	if(ICM20948_ReadAll(&hi2c1, &imu_data) == HAL_OK)
	{
		datastruct->gyro_x = imu_data.gyro_x;
		datastruct->gyro_y = imu_data.gyro_y;
		datastruct->gyro_z = imu_data.gyro_z;

		datastruct->accel_x = imu_data.accel_x;
		datastruct->accel_y = imu_data.accel_y;
		datastruct->accel_z = imu_data.accel_z;

		if(ICM20948_ReadMagnetometer(&hi2c1, &mag_data) == HAL_OK)
		{
			datastruct->mag_x = mag_data.x_ut;
			datastruct->mag_y = mag_data.y_ut;
			datastruct->mag_z = mag_data.z_ut;
		}
	}
}

void PrintDisplay(SensorData_t *data, bool blockHID, MousePos_t mouse_pos, GestureType_t gesture)
{
    static GestureType_t last_gesture = GESTURE_NONE;
    static uint32_t last_gesture_time = 0;

    if (gesture != GESTURE_NONE)
    {
        last_gesture      = gesture;
        last_gesture_time = HAL_GetTick();
    }

    uint32_t time_since = (last_gesture_time == 0) ? 0 : (HAL_GetTick() - last_gesture_time);

    // Row positions
    #define R_BUTTONS   1
    #define R_JOY1      11
    #define R_JOY2      15
    #define R_SENSORS   19
    #define R_MOUSE     30
    #define R_GESTURE   34

    // Buttons
    printf("\033[%d;1H[ BUTTONS ]\033[K",          R_BUTTONS);
    printf("\033[%d;1HBTN1: %s\033[K",  R_BUTTONS+1, data->buttons.BUTTON1         ? "UP  " : "DOWN");
    printf("\033[%d;1HBTN2: %s\033[K",  R_BUTTONS+2, data->buttons.BUTTON2         ? "UP  " : "DOWN");
    printf("\033[%d;1HBTN3: %s\033[K",  R_BUTTONS+3, data->buttons.BUTTON3         ? "UP  " : "DOWN");
    printf("\033[%d;1HBTN4: %s\033[K",  R_BUTTONS+4, data->buttons.BUTTON4         ? "UP  " : "DOWN");
    printf("\033[%d;1HBTN5: %s\033[K",  R_BUTTONS+5, data->buttons.BUTTON5         ? "UP  " : "DOWN");
    printf("\033[%d;1HBTN6: %s\033[K",  R_BUTTONS+6, data->buttons.BUTTON6         ? "UP  " : "DOWN");  // wait this is R_JOY1, let me fix
    printf("\033[%d;1HJS1:  %s\033[K",  R_BUTTONS+7, data->buttons.JOYSTICKBUTTON1 ? "UP  " : "DOWN");
    printf("\033[%d;1HJS2:  %s\033[K",  R_BUTTONS+8, data->buttons.JOYSTICKBUTTON2 ? "UP  " : "DOWN");

    // Joystick 1
    printf("\033[%d;1H[ JOYSTICK 1 ]\033[K", R_JOY1);
    printf("\033[%d;1HX: %4d\033[K",         R_JOY1+1, data->js1_x);
    printf("\033[%d;1HY: %4d\033[K",         R_JOY1+2, data->js1_y);

    // Joystick 2
    printf("\033[%d;1H[ JOYSTICK 2 ]\033[K", R_JOY2);
    printf("\033[%d;1HX: %4d\033[K",         R_JOY2+1, data->js2_x);
    printf("\033[%d;1HY: %4d\033[K",         R_JOY2+2, data->js2_y);

    // Sensors
    printf("\033[%d;1H[ SENSORS ]\033[K",       R_SENSORS);
    printf("\033[%d;1HGyro  X: %7.1f\033[K",    R_SENSORS+1,  (float)data->gyro_x);
    printf("\033[%d;1HGyro  Y: %7.1f\033[K",    R_SENSORS+2,  (float)data->gyro_y);
    printf("\033[%d;1HGyro  Z: %7.1f\033[K",    R_SENSORS+3,  (float)data->gyro_z);
    printf("\033[%d;1HAccel X: %7d\033[K",      R_SENSORS+4,  data->accel_x);
    printf("\033[%d;1HAccel Y: %7d\033[K",      R_SENSORS+5,  data->accel_y);
    printf("\033[%d;1HAccel Z: %7d\033[K",      R_SENSORS+6,  data->accel_z);
    printf("\033[%d;1HMag   X: %7.2f uT\033[K", R_SENSORS+7,  data->mag_x);
    printf("\033[%d;1HMag   Y: %7.2f uT\033[K", R_SENSORS+8,  data->mag_y);
    printf("\033[%d;1HMag   Z: %7.2f uT\033[K", R_SENSORS+9,  data->mag_z);

    // Mouse
    printf("\033[%d;1H[ MOUSE ]\033[K",           R_MOUSE);
    printf("\033[%d;1HX: %4d\033[K",              R_MOUSE+1, mouse_pos.x);
    printf("\033[%d;1HY: %4d\033[K",              R_MOUSE+2, mouse_pos.y);

    // Gesture
	printf("\033[%d;1H[ GESTURE ]\033[K",  R_GESTURE);
	if (last_gesture == GESTURE_NONE)
	{
		printf("\033[%d;1HNone yet\033[K", R_GESTURE+1);
		printf("\033[%d;1H        \033[K", R_GESTURE+2);
	}
	else
	{
		printf("\033[%d;1H%-15s\033[K",      R_GESTURE+1, GestureDetector_Gesturetostring(last_gesture));
		printf("\033[%d;1H%lu ms ago\033[K", R_GESTURE+2, (unsigned long)time_since);
	}
}

void PrintAnalyzerView(void)
{
    printf("\033[1;1H[ ADAPTIVE ANALYZER VIEW ]\033[K");
    printf("\033[2;1H--------------------------------------------------\033[K");
    printf("\033[3;1H\033[2K");
    // Live config
    printf("\033[4;1H[ LIVE CONFIG ]\033[K");
    printf("\033[5;1HThreshold:  %8.1f\033[K",  gesture_magnitude_threshold);
    printf("\033[6;1HWindow:     %8lu ms\033[K", (unsigned long)gesture_detection_window_ms);
    printf("\033[7;1HPeak Frac:  %8.2f\033[K",   gesture_peak_fraction);
    printf("\033[8;1HBias X:     %8.1f\033[K",   gyro_bias_x);
    printf("\033[9;1HBias Y:     %8.1f\033[K",   gyro_bias_y);
    printf("\033[10;1H\033[2K");
    printf("\033[11;1H[ GESTURE STATE ]\033[K");
    printf("\033[12;1HState:  %-12s\033[K",       GestureDetector_StateToString(GestureDetector_GetState()));
    printf("\033[13;1HPeak:   %8.1f\033[K",       GestureDetector_GetPeakMagnitude());
    printf("\033[14;1H\033[2K");
    printf("\033[15;1H[ ANALYZER OUTPUT ]\033[K");
    printf("\033[16;1H--------------------------------------------------\033[K");
    printf("\033[17;1H\033[K");  // blank line before output
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
