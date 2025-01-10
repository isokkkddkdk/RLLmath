//Create by 李益 2025/1/10
#include <iostream>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define SAMPLE_PATE 10000  //采样率
#define ADC_PE 4096        //ADC分辨率（2^12)
#define V_REFER 3.3           //ADC参考电压
#define MAX_Hz 1000              //最大频率

//缓冲区域 volatile避免被程序优化
volatile uint16_t channe1Date[256];//输入
volatile uint16_t channe2Date[256];//参考
volatile bool dateReady = false;

// 锁相环参数
volatile double phaseAcc = 0.0;        // 相位累积器
volatile double phaseStep = 0.0;       // 相位步进（与频率对应）
volatile double currentPhase = 0.0;    // 当前相位
volatile double amplitude = 1.0;       // 输出信号幅值

//初始化ADC
void initADC() {
    // 配置 ADC 基本参数
    // 配置通道1（待测信号）
    // 配置通道2（参考信号）
    // 开始采集
}

//读取 ADC（模数转换器）通道的值的函数
uint16_t readADC(uint8_t channel) {
    return 2048;//虚假的数据
};

//定时采样

void timeIntH() {
    static uint16_t sampleIndex = 0;
    //读取ADC
    channe1Date[sampleIndex] = readADC(1);
    channe2Date[sampleIndex] = readADC(2);
    sampleIndex++;
    if (sampleIndex >= 256) {
        sampleIndex = 0;
        dateReady = true;
        //数据采集完毕，通知主任务开始
    }
}

//低通滤波
void lowPassFilter(volatile uint16_t *data, volatile uint16_t *filteredData, int length) {
    for (int i = 1; i < length; i++) {
        filteredData[i] = 0.9 * filteredData[i - 1] + 0.1 * data[i]; // 简单的 IIR 滤波器
    }
}
// 提取输入信号频率
double extractFrequency(volatile uint16_t* data) {
    int zeroCrossings = 0;
    double tFirst = -1, tLast = -1;

    for (int i = 1; i < 256; i++) {
        if (data[i - 1] < ADC_PE / 2 && data[i] >= ADC_PE / 2) {
            double t = (double)i / SAMPLE_PATE;
            if (tFirst < 0) tFirst = t;
            tLast = t;
            zeroCrossings++;
        }
    }

    double period = (tLast - tFirst) / (zeroCrossings - 1);
    return 1.0 / period;
}
// 计算相位误差
double calculatePhaseError(volatile uint16_t* inputData,volatile uint16_t* refData) {
    double dotProduct = 0.0;
    double inputMagnitude = 0.0, refMagnitude = 0.0;

    for (int i = 0; i < 256; i++) {
        dotProduct += inputData[i] * refData[i];
        inputMagnitude += inputData[i] * inputData[i];
        refMagnitude += refData[i] * refData[i];
    }

    return acos(dotProduct / sqrt(inputMagnitude * refMagnitude));
}

// 动态估算信号幅值
double estimateAmplitude(volatile uint16_t* data) {
    double maxVal = 0, minVal = ADC_PE - 1;
    for (int i = 0; i < 256; i++) {
        if (data[i] > maxVal) maxVal = data[i];
        if (data[i] < minVal) minVal = data[i];
    }
    return (maxVal - minVal) * V_REFER / ADC_PE / 2.0;
}
// 输出信号到 DAC
void outputToDAC(double sinOutput, double cosOutput) {
    uint16_t dacValueSin = (sinOutput + 1.0) * 0.5 * (ADC_PE - 1);
    uint16_t dacValueCos = (cosOutput + 1.0) * 0.5 * (ADC_PE - 1);
   //将信号输出到数字模拟转换器（DAC） writeDAC(1, dacValueSin); // 输出到 DAC 通道1
   //将信号输出到数字模拟转换器（DAC） writeDAC(2, dacValueCos); // 输出到 DAC 通道2
}
//主要函数Create by 李益 2025/1/10
void pllAl() {
    if (dateReady == false) return;  // 如果 dataReady 为 false，直接退出函数
    dateReady = false;               // 如果 dataReady 为 true，继续执行并将其设为 false
//0)滤波
    volatile uint16_t filtereInput[256];
    lowPassFilter(channe1Date, filtereInput, 256);
    //1
    double inputFreq = extractFrequency(filtereInput);
    double phaseError = calculatePhaseError(filtereInput, channe2Date);
   //2
    phaseStep = (inputFreq / SAMPLE_PATE) * 2 * M_PI;
    phaseAcc += phaseError * 0.01; // 简单的 P 控制器
    //3
    amplitude = estimateAmplitude(filtereInput);
   //4
    for (int i = 0; i < 256; i++) {
        currentPhase += phaseStep;
        if (currentPhase >= 2 * M_PI) currentPhase -= 2 * M_PI;

        // 生成正弦波和余弦波
        double sinOutput = amplitude * sin(currentPhase);
        double cosOutput = amplitude * cos(currentPhase);

        // 输出信号（DAC 或 PWM）
        outputToDAC(sinOutput, cosOutput);
    }
}
int main() {
    initADC();
    //定时器配置函数configureTimerInterrupt(SAMPLE_PATE);

    while (true) {
        pllAl(); // 锁相环主任务
    }
}