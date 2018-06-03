/*
 * Communication.c
 *
 *  Created on: May 28, 2018
 *      Author: Admin
 */

/********************************** Includes **********************************/
#include "Communication.h"

/******************************* Public Variables *******************************/
volatile RobotGoal robotGoal;
volatile RobotState robotState;

/******************************* Private Variables ******************************/
static volatile RobotGoal *robotGoalPtr;
static volatile uint8_t robotGoalData[sizeof(RobotGoal)];
static volatile uint8_t *robotGoalDataPtr;
volatile uint8_t buf[21];
static volatile uint8_t startSeqCount;
static volatile uint8_t totalBytesRead;
static volatile RobotState *robotStatePtr;

/*********************************** Externs **********************************/
extern UART_HandleTypeDef huart5;
extern osMutexId PCUARTHandle;
extern osThreadId rxTaskHandle;
extern osThreadId defaultTaskHandle;

/******************************** Functions ************************************/
void Comm_Init(volatile RobotGoal* robotGoal, volatile RobotState* robotState) {
	// Receiving
	robotGoal->id = 0;
	robotGoalPtr = robotGoal;
	robotGoalDataPtr = robotGoalData;
	startSeqCount = 0;
	totalBytesRead = 0;

	// Sending
	robotState->id = 0;
	robotStatePtr = robotState;
	robotState->start_seq = UINT32_MAX;
	robotState->end_seq = 0;
}

/* StartRxTask function */
void StartRxTask(void const * argument) {
	/* USER CODE BEGIN StartRxTask */
	xTaskNotifyWait(UINT32_MAX, UINT32_MAX, NULL, portMAX_DELAY);

	HAL_StatusTypeDef status;

	/* Infinite loop */
	for (;;) {
		do{
			xSemaphoreTake(PCUARTHandle, 1);
			status = HAL_UART_Receive_IT(&huart5, (uint8_t *) buf, sizeof(buf));
			xSemaphoreGive(PCUARTHandle);
		}while(status != HAL_OK);

		xTaskNotifyWait(UINT32_MAX, UINT32_MAX, NULL, portMAX_DELAY);

		for (uint8_t i = 0; i < sizeof(buf); ++i) {
			if (startSeqCount == 4) {
				// This control block is entered when the header sequence of
				// 0xFFFFFFFF has been received; thus we know the data we
				// receive will be in tact

				*robotGoalDataPtr = buf[i];
				robotGoalDataPtr++;
				totalBytesRead++;

				if (totalBytesRead == sizeof(RobotGoal)) {
					// If, after the last couple of receive interrupts, we have
					// received sizeof(RobotGoal) bytes, then we copy the data
					// buffer into the robotGoal structure and wake the control
					// thread to distribute states to each actuator
					memcpy(&robotGoal, robotGoalData, sizeof(RobotGoal));

					// Reset the variables to help with reception of a RobotGoal
					robotGoalDataPtr = robotGoalData;
					startSeqCount = 0;
					totalBytesRead = 0;

					BaseType_t xHigherPriorityTaskWoken = pdTRUE;
					xTaskNotifyFromISR(defaultTaskHandle, 1UL, eNoAction,
							&xHigherPriorityTaskWoken);
					continue;
				}
			}else{
				// This control block is used to verify that the data header is in tact
				if (buf[i] == 0xFF) {
					startSeqCount++;
				} else {
					startSeqCount = 0;
				}
			}
		}
	}
	/* USER CODE END StartRxTask */
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef * huart) {
	if (huart == &huart5) {
		BaseType_t xHigherPriorityTaskWoken = pdTRUE;
		xTaskNotifyFromISR(rxTaskHandle, 1UL, eNoAction,
				&xHigherPriorityTaskWoken);
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
     static volatile error = HAL_UART_GetError(huart);
}
