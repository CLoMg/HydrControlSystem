#define A1 70  //测量范围下限
#define A2 1000 //测量范围上限
__IO uint16_t ADC_LOG[200]= {0};
uint16_t LOG_AVG_Data = 0;
/**
 * @brief ADC IRQ Callback
 * 
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == hadc1.Instance)
	{
		arm_mean_q15((q15_t *)ADC_LOG,200,(q15_t *)&LOG_AVG_Data);
	}
}
/**
 * @brief 根据adc值 计算实时距离
 * 
 */
float distance_calc(float a1,float a2,float ad_value){
    /*
    * a1- 0 ,a2 - 4095,dis = (a2 - a1) / 4095 * (ad_value - 0) + a
    */
   return((a2-a1)/4095 * ad_value + a1); 
}
/**
 * @brief  电池阀控制
 *  0 - 电池阀A断电 ，电池阀B断电
 *  1 - 电池阀A得电，电池阀B断电
 *  2 - 电池阀A断电，电池阀B得电
 */
static unsigned char pre_status = 0;
int solenoid_valve_control(unsigned char new_status){
    if(new_status == pre_status)
        return 0;
    else{
        switch (new_status)
        {
            case 0 :
            {
                HAL_GPIO_WritePin(SA_GPIO_PORT, SA_GPIO_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(SB_GPIO_PORT, SB_GPIO_PIN, GPIO_PIN_RESET);
            }
             break;
            case 1 :
            {
                HAL_GPIO_WritePin(SA_GPIO_PORT, SA_GPIO_PIN, GPIO_PIN_SET);
                HAL_GPIO_WritePin(SB_GPIO_PORT, SB_GPIO_PIN, GPIO_PIN_RESET);
            }
             break;
            case 2 :
            {
                HAL_GPIO_WritePin(SA_GPIO_PORT, SA_GPIO_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(SB_GPIO_PORT, SB_GPIO_PIN, GPIO_PIN_SET);
            }
             break;
            default:
                return -1;
                break;
        }
    }
    pre_status = new_status;
    return 1;
}
_Bool mode = 0;
float dis_set = 0;
uint16_t tick = 0;
void pupm_process(void){
    float dis_cur = 0;
    tick++;
    /*计算当前距离，并显示*/
    dis_cur = distance_calc(A1,A2,LOG_AVG_Data);
    //添加距离显示（打印函数）
    
    /*如果是自动模式模式，根据当前距离和设定距离，控制液压泵*/
    if(mode == 1){
        //显示设定距离

        //检测距离小于预设距离，电池阀B得电，A断电，伸缩杆缩回
        if(dis_set - dis_cur > 0.1)
            solenoid_valve_control(2);
        //检测距离大于预设距离，电池阀A得电，B断电，伸缩杆伸出
        else if(dis_set - dis_cur < -0.1)
            solenoid_valve_control(1);
        else
            solenoid_valve_control(0);
    }
    if(cmd_size > 0)
    {
        if(strstr(cmd_data,"Mode:Auto\r\n") != NULL)
        {
            mode  = 1;
        }
        else if(strstr(cmd_data,"Mode:Manual\r\n") != NULL)
        {
            mode = 0;
        }
        else if(strstr(cmd_data,"SetDis:")  != NULL){
            float setdis = atof(cmd_data);
            if(( setdis >= 70) &&( setdis <= 1000))
                dis_set  = setdis;
        }
        else
        ;
        memset(cmd_data,0x00,cmd_size);
        cmd_size = 0;
    }
    HAL_Delay(20);
}
uint8_t cmd_data[100];
uint8_t cmd_size = 0;
uint8_t Uart_ReadCache[100];
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{

		if(huart->Instance == USART1){
			memcpy(cmd_data,Uart_ReadCache,Size);
            cmd_size  = Size;
			memset(Uart_ReadCache,0x00,300);
			HAL_UARTEx_ReceiveToIdle_IT(&huart1, Uart_ReadCache, 300);
		}

}