#define A1 70  //测量范围下限
#define A2 1000 //测量范围上限
__IO uint16_t ADC_LOG[200]= {0};
uint16_t LOG_AVG_Data = 0;

typedef struct 
{
    /* data */
    uint8_t mode;
    uint8_t control;
    float dis_set;
    float dis_cur;
}Hydr_TypeDef;

Hydr_TypeDef hydr_dev ={
    0,
    0,
    0.0,
    0.0
};
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
    tick++;
    /*计算当前距离，并显示*/
    hydr_dev.dis_cur = distance_calc(A1,A2,LOG_AVG_Data);
    //添加距离显示（打印函数）
    printf("wset dis_cur.valf %f",hydr_dev.dis_cur);
    /*如果是自动模式模式，根据当前距离和设定距离，计算出控制方法  0：stop 1：- 2：+*/
    if(mode == 1){

        //检测距离小于预设距离，电池阀B得电，A断电，伸缩杆缩回
        if( hydr_dev.dis_set -  hydr_dev.dis_cur > 0.1)
            hydr_dev.control = 2;
        //检测距离大于预设距离，电池阀A得电，B断电，伸缩杆伸出
        else if(dis_set - dis_cur < -0.1)
            hydr_dev.control = 1;
        else
            hydr_dev.control = 0;
    }
    /*根据结构体中control属性，控制继电器通断，从而控制电机和相应电磁阀工作*/
    solenoid_valve_control(hydr_dev.control);
   
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
            //解析串口接收数据
            if(cmd_size > 0)
            {
                if(strstr(cmd_data,"mode:auto\r\n") != NULL)
                {
                    hydr_dev.mode  = 1;
                }
                else if(strstr(cmd_data,"mode:manual\r\n") != NULL)
                {
                    hydr_dev.mode  = 0;
                }
                else if(strstr(cmd_data,"dis_set:")  != NULL){
                    float setdis = atof(cmd_data);
                    //判断输入数据是否在检测范围内
                    if(( setdis >= 70) &&( setdis <= 1000))
                    hydr_dev.dis_set  = setdis;
                }
                else if(strstr(cmd_data,"dis:+\r\n") != NULL){
                    hydr_dev.control = 2;
                }
                else if(strstr(cmd_data,"dis:-\r\n") != NULL){
                    hydr_dev.control = 1;
                }
                else if (strstr(cmd_data,"stop\r\n") != NULL)
                {
                    hydr_dev.control = 0;
                }
                else
                ;
                memset(cmd_data,0x00,cmd_size);
                cmd_size = 0;
            }
			memset(Uart_ReadCache,0x00,300);
			HAL_UARTEx_ReceiveToIdle_IT(&huart1, Uart_ReadCache, 300);
		}

}