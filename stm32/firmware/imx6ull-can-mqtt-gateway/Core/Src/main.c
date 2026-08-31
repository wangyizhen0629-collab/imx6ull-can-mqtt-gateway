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
#include "can.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
  uint16_t std_id;
  uint32_t period_ms;
  uint32_t last_release_ms;
  volatile uint32_t tx_success_count;
  volatile uint32_t tx_failure_count;
  uint8_t counter;
} ECU_MessageStateTypeDef;

typedef struct
{
  uint16_t vehicle_speed_centi_kph;
  uint16_t engine_speed_quarter_rpm;
  uint16_t throttle_tenth_percent;
  uint16_t battery_millivolt;
  uint32_t odometer_tenth_km;
  int16_t coolant_celsius;
  uint16_t soc_tenth_percent;
  uint16_t fault_flags;
  uint8_t gear;
  uint8_t door_flags;
  uint8_t ignition_state;
} ECU_VehicleStateTypeDef;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ECU_MESSAGE_COUNT              3U
#define ECU_VEHICLE_DYNAMICS_ID        0x100U
#define ECU_POWER_STATUS_ID            0x101U
#define ECU_BODY_STATUS_ID             0x102U

/*
 * 一个 tick 对应一帧成功发送的 0x100（10 ms）。60 秒场景依次为：
 * 熄火驻车 3 s、怠速 2 s、加速 15 s、巡航 20 s、减速 15 s、
 * 怠速 3 s、熄火驻车 2 s。发送失败时不推进场景或 counter，保证
 * 接收端看到的成功帧序列可以按相同初值逐帧复现。
 */
#define ECU_DRIVE_CYCLE_TICKS          6000U
#define ECU_ENGINE_START_TICK          300U
#define ECU_ACCEL_START_TICK           500U
#define ECU_CRUISE_START_TICK          2000U
#define ECU_DECEL_START_TICK           4000U
#define ECU_IDLE_STOP_TICK             5500U
#define ECU_ENGINE_STOP_TICK           5800U

#define ECU_CRUISE_SPEED_CENTI_KPH     6000U
#define ECU_IDLE_SPEED_RPM             800U
#define ECU_THROTTLE_ACCEL_TENTH       320U
#define ECU_THROTTLE_CRUISE_TENTH      160U
#define ECU_BATTERY_OFF_MILLIVOLT      12600U
#define ECU_BATTERY_RUNNING_MILLIVOLT  13800U
#define ECU_INITIAL_COOLANT_CELSIUS    20
#define ECU_MAX_COOLANT_CELSIUS        90
#define ECU_INITIAL_SOC_TENTH          800U
#define ECU_INITIAL_ODOMETER_TENTH_KM  1234567UL
#define ECU_ODOMETER_MAX_TENTH_KM      0xFFFFFFUL
#define ECU_DISTANCE_DIVISOR           3600000UL

/* 这些取值是模拟场景约定；DBC 只规定字段位宽，没有定义枚举文本。 */
#define ECU_GEAR_NEUTRAL               0U
#define ECU_IGNITION_OFF               0U
#define ECU_IGNITION_RUN               2U
#define ECU_DOOR_DRIVER_OPEN           0x01U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static ECU_MessageStateTypeDef ecu_messages[ECU_MESSAGE_COUNT] =
{
  {ECU_VEHICLE_DYNAMICS_ID,   10U, 0U, 0U, 0U, 0U},
  {ECU_POWER_STATUS_ID,      100U, 0U, 0U, 0U, 0U},
  {ECU_BODY_STATUS_ID,      1000U, 0U, 0U, 0U, 0U}
};

static ECU_VehicleStateTypeDef ecu_vehicle_state =
{
  0U,
  0U,
  0U,
  ECU_BATTERY_OFF_MILLIVOLT,
  ECU_INITIAL_ODOMETER_TENTH_KM,
  ECU_INITIAL_COOLANT_CELSIUS,
  ECU_INITIAL_SOC_TENTH,
  0U,
  ECU_GEAR_NEUTRAL,
  ECU_DOOR_DRIVER_OPEN,
  ECU_IGNITION_OFF
};

static uint32_t ecu_drive_cycle_tick;
static uint32_t ecu_distance_accumulator;
static uint16_t ecu_warmup_tick_count;
static uint16_t ecu_cooldown_tick_count;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void ECU_InitSchedule(uint32_t now);
static uint8_t ECU_ProcessMessage(ECU_MessageStateTypeDef *message,
                                  uint32_t now);
static void ECU_BuildPayload(const ECU_MessageStateTypeDef *message,
                             uint8_t payload[8]);
static void ECU_UpdateVehicleState(void);
static void ECU_AdvanceVehicleState(void);
static uint8_t ECU_SelectGear(uint16_t speed_centi_kph);
static uint16_t ECU_CalculateEngineSpeed(uint16_t speed_centi_kph,
                                         uint8_t gear);
static uint8_t ECU_EncodeTenthPercent(uint16_t tenth_percent);
static void ECU_PackU16Le(uint8_t payload[8], uint8_t offset,
                          uint16_t value);
static void ECU_PackU24Le(uint8_t payload[8], uint8_t offset,
                          uint32_t value);
static void ECU_FinalizePayload(uint8_t counter, uint8_t payload[8]);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void ECU_InitSchedule(uint32_t now)
{
  uint32_t index;

  for (index = 0U; index < ECU_MESSAGE_COUNT; index++)
  {
    ecu_messages[index].last_release_ms = now;
  }
}

static uint8_t ECU_SelectGear(uint16_t speed_centi_kph)
{
  if (speed_centi_kph <= 1200U)
  {
    return 1U;
  }
  if (speed_centi_kph <= 2500U)
  {
    return 2U;
  }
  if (speed_centi_kph <= 4000U)
  {
    return 3U;
  }
  return 4U;
}

static uint16_t ECU_CalculateEngineSpeed(uint16_t speed_centi_kph,
                                         uint8_t gear)
{
  uint32_t engine_rpm;

  if (speed_centi_kph == 0U)
  {
    engine_rpm = ECU_IDLE_SPEED_RPM;
  }
  else if (gear == 1U)
  {
    engine_rpm = 900U + (((uint32_t)speed_centi_kph * 7U) / 5U);
  }
  else if (gear == 2U)
  {
    engine_rpm = 900U + (((uint32_t)speed_centi_kph * 7U) / 10U);
  }
  else if (gear == 3U)
  {
    engine_rpm = 900U + (((uint32_t)speed_centi_kph * 9U) / 20U);
  }
  else
  {
    engine_rpm = 900U + ((uint32_t)speed_centi_kph / 4U);
  }

  /* EngineSpeed 的 DBC factor 为 0.25 rpm，因此 raw = rpm * 4。 */
  return (uint16_t)(engine_rpm * 4U);
}

static void ECU_UpdateVehicleState(void)
{
  uint32_t phase_tick;
  uint32_t ramp_tick;

  phase_tick = ecu_drive_cycle_tick;

  if ((phase_tick < ECU_ENGINE_START_TICK) ||
      (phase_tick >= ECU_ENGINE_STOP_TICK))
  {
    ecu_vehicle_state.vehicle_speed_centi_kph = 0U;
    ecu_vehicle_state.engine_speed_quarter_rpm = 0U;
    ecu_vehicle_state.throttle_tenth_percent = 0U;
    ecu_vehicle_state.gear = ECU_GEAR_NEUTRAL;
    ecu_vehicle_state.ignition_state = ECU_IGNITION_OFF;
    ecu_vehicle_state.door_flags = ECU_DOOR_DRIVER_OPEN;
  }
  else if ((phase_tick < ECU_ACCEL_START_TICK) ||
           (phase_tick >= ECU_IDLE_STOP_TICK))
  {
    ecu_vehicle_state.vehicle_speed_centi_kph = 0U;
    ecu_vehicle_state.engine_speed_quarter_rpm =
      (uint16_t)(ECU_IDLE_SPEED_RPM * 4U);
    ecu_vehicle_state.throttle_tenth_percent = 0U;
    ecu_vehicle_state.gear = ECU_GEAR_NEUTRAL;
    ecu_vehicle_state.ignition_state = ECU_IGNITION_RUN;
    ecu_vehicle_state.door_flags = 0U;
  }
  else if (phase_tick < ECU_CRUISE_START_TICK)
  {
    ramp_tick = phase_tick - ECU_ACCEL_START_TICK;
    ecu_vehicle_state.vehicle_speed_centi_kph =
      (uint16_t)((ramp_tick * ECU_CRUISE_SPEED_CENTI_KPH) /
                 (ECU_CRUISE_START_TICK - ECU_ACCEL_START_TICK));
    ecu_vehicle_state.throttle_tenth_percent =
      ECU_THROTTLE_ACCEL_TENTH;
    ecu_vehicle_state.gear =
      ECU_SelectGear(ecu_vehicle_state.vehicle_speed_centi_kph);
    ecu_vehicle_state.engine_speed_quarter_rpm =
      ECU_CalculateEngineSpeed(ecu_vehicle_state.vehicle_speed_centi_kph,
                               ecu_vehicle_state.gear);
    ecu_vehicle_state.ignition_state = ECU_IGNITION_RUN;
    ecu_vehicle_state.door_flags = 0U;
  }
  else if (phase_tick < ECU_DECEL_START_TICK)
  {
    ecu_vehicle_state.vehicle_speed_centi_kph =
      ECU_CRUISE_SPEED_CENTI_KPH;
    ecu_vehicle_state.throttle_tenth_percent =
      ECU_THROTTLE_CRUISE_TENTH;
    ecu_vehicle_state.gear = 4U;
    ecu_vehicle_state.engine_speed_quarter_rpm =
      ECU_CalculateEngineSpeed(ecu_vehicle_state.vehicle_speed_centi_kph,
                               ecu_vehicle_state.gear);
    ecu_vehicle_state.ignition_state = ECU_IGNITION_RUN;
    ecu_vehicle_state.door_flags = 0U;
  }
  else
  {
    ramp_tick = phase_tick - ECU_DECEL_START_TICK;
    ecu_vehicle_state.vehicle_speed_centi_kph =
      (uint16_t)(ECU_CRUISE_SPEED_CENTI_KPH -
                 ((ramp_tick * ECU_CRUISE_SPEED_CENTI_KPH) /
                  (ECU_IDLE_STOP_TICK - ECU_DECEL_START_TICK)));
    ecu_vehicle_state.throttle_tenth_percent = 0U;
    ecu_vehicle_state.gear =
      ECU_SelectGear(ecu_vehicle_state.vehicle_speed_centi_kph);
    ecu_vehicle_state.engine_speed_quarter_rpm =
      ECU_CalculateEngineSpeed(ecu_vehicle_state.vehicle_speed_centi_kph,
                               ecu_vehicle_state.gear);
    ecu_vehicle_state.ignition_state = ECU_IGNITION_RUN;
    ecu_vehicle_state.door_flags = 0U;
  }

  ecu_vehicle_state.battery_millivolt =
    (ecu_vehicle_state.engine_speed_quarter_rpm == 0U) ?
    ECU_BATTERY_OFF_MILLIVOLT : ECU_BATTERY_RUNNING_MILLIVOLT;
}

static uint8_t ECU_EncodeTenthPercent(uint16_t tenth_percent)
{
  if (tenth_percent > 1000U)
  {
    tenth_percent = 1000U;
  }

  /* DBC factor 0.4 % 等于 4 个 0.1 %；场景值均可被 4 整除。 */
  return (uint8_t)(tenth_percent / 4U);
}

static void ECU_PackU16Le(uint8_t payload[8], uint8_t offset,
                          uint16_t value)
{
  payload[offset] = (uint8_t)(value & 0xFFU);
  payload[offset + 1U] = (uint8_t)(value >> 8U);
}

static void ECU_PackU24Le(uint8_t payload[8], uint8_t offset,
                          uint32_t value)
{
  payload[offset] = (uint8_t)(value & 0xFFU);
  payload[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
  payload[offset + 2U] = (uint8_t)((value >> 16U) & 0xFFU);
}

static void ECU_FinalizePayload(uint8_t counter, uint8_t payload[8])
{
  uint8_t byte_index;
  uint8_t xor_value = 0U;

  payload[6] = counter;
  for (byte_index = 0U; byte_index < 7U; byte_index++)
  {
    xor_value ^= payload[byte_index];
  }
  payload[7] = xor_value;
}

static void ECU_BuildPayload(const ECU_MessageStateTypeDef *message,
                             uint8_t payload[8])
{
  if (message->std_id == ECU_VEHICLE_DYNAMICS_ID)
  {
    ECU_PackU16Le(payload, 0U,
                  ecu_vehicle_state.vehicle_speed_centi_kph);
    ECU_PackU16Le(payload, 2U,
                  ecu_vehicle_state.engine_speed_quarter_rpm);
    payload[4] =
      ECU_EncodeTenthPercent(ecu_vehicle_state.throttle_tenth_percent);
    payload[5] = ecu_vehicle_state.gear;
  }
  else if (message->std_id == ECU_POWER_STATUS_ID)
  {
    ECU_PackU16Le(payload, 0U, ecu_vehicle_state.battery_millivolt);
    payload[2] = (uint8_t)(ecu_vehicle_state.coolant_celsius + 40);
    payload[3] =
      ECU_EncodeTenthPercent(ecu_vehicle_state.soc_tenth_percent);
    ECU_PackU16Le(payload, 4U, ecu_vehicle_state.fault_flags);
  }
  else
  {
    ECU_PackU24Le(payload, 0U, ecu_vehicle_state.odometer_tenth_km);
    payload[3] = (uint8_t)(ecu_vehicle_state.door_flags & 0x0FU);
    payload[4] = (uint8_t)(ecu_vehicle_state.ignition_state & 0x03U);
    /* byte 3 高四位、byte 4 高六位和 byte 5 是 spare，保持为零。 */
  }

  ECU_FinalizePayload(message->counter, payload);
}

static uint8_t ECU_ProcessMessage(ECU_MessageStateTypeDef *message,
                                  uint32_t now)
{
  uint8_t tx_data[8] = {0};
  uint32_t elapsed;

  elapsed = now - message->last_release_ms;
  if (elapsed < message->period_ms)
  {
    return 0U;
  }

  /* Preserve the schedule phase while skipping any already missed slots. */
  message->last_release_ms += (elapsed / message->period_ms) *
                              message->period_ms;

  ECU_BuildPayload(message, tx_data);

  if (CAN_SendMessage(message->std_id, tx_data, 8U) == HAL_OK)
  {
    message->counter++;
    message->tx_success_count++;
    return 1U;
  }

  message->tx_failure_count++;
  return 0U;
}

static void ECU_AdvanceVehicleState(void)
{
  ecu_distance_accumulator +=
    (uint32_t)ecu_vehicle_state.vehicle_speed_centi_kph;
  if (ecu_distance_accumulator >= ECU_DISTANCE_DIVISOR)
  {
    ecu_distance_accumulator -= ECU_DISTANCE_DIVISOR;
    if (ecu_vehicle_state.odometer_tenth_km < ECU_ODOMETER_MAX_TENTH_KM)
    {
      ecu_vehicle_state.odometer_tenth_km++;
    }
  }

  if (ecu_vehicle_state.engine_speed_quarter_rpm != 0U)
  {
    ecu_cooldown_tick_count = 0U;
    ecu_warmup_tick_count++;
    if (ecu_warmup_tick_count >= 100U)
    {
      ecu_warmup_tick_count = 0U;
      if (ecu_vehicle_state.coolant_celsius < ECU_MAX_COOLANT_CELSIUS)
      {
        ecu_vehicle_state.coolant_celsius++;
      }
    }
  }
  else
  {
    ecu_warmup_tick_count = 0U;
    ecu_cooldown_tick_count++;
    if (ecu_cooldown_tick_count >= 500U)
    {
      ecu_cooldown_tick_count = 0U;
      if (ecu_vehicle_state.coolant_celsius > ECU_INITIAL_COOLANT_CELSIUS)
      {
        ecu_vehicle_state.coolant_celsius--;
      }
    }
  }

  ecu_drive_cycle_tick++;
  if (ecu_drive_cycle_tick >= ECU_DRIVE_CYCLE_TICKS)
  {
    ecu_drive_cycle_tick = 0U;
  }
}

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

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    Error_Handler();
  }

  ECU_InitSchedule(HAL_GetTick());

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now;
    uint8_t dynamics_sent;

    now = HAL_GetTick();
    ECU_UpdateVehicleState();
    dynamics_sent = ECU_ProcessMessage(&ecu_messages[0], now);
    (void)ECU_ProcessMessage(&ecu_messages[1], now);
    (void)ECU_ProcessMessage(&ecu_messages[2], now);

    if (dynamics_sent != 0U)
    {
      ECU_AdvanceVehicleState();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
#ifdef USE_FULL_ASSERT
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
