#define CALIBRATE_SPECIFIC_LAMPS false

// https://www.apogeeinstruments.com/conversion-ppfd-to-lux/
#define PPFD_CALIBRATION_SUNLIGHT       0.0185 //Sunlight
#define PPFD_CALIBRATION_FLOURESCENT    0.0135 //Cool White Fluorescent Lamps
#define PPFD_CALIBRATION_METAL_HALIDE   0.0141 //Metal Halide
#define PPFD_CALIBRATION_HPS            0.0122 //Mogul Base High Pressure Sodium Lamps
// specific lamps
#define PPFD_CALIBRATION_DEHPS          0.0130 //Dual-Ended High Pressure Sodium (DEHPS): ePapillion 1000 W
#define PPFD_CALIBRATION_CMH942         0.0154  //Ceramic Metal Halide (CMH942): standard 4200 K color temperature
#define PPFD_CALIBRATION_CMH930         0.0170 //Ceramic Metal Halide (CMH930-Agro): 3100 K color temperature, spectrum shifted to red wavelengths

#include <BH1750.h>

void readBH1750(byte address){
    BH1750 lightMeter = BH1750(address);
    lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE_2);
    float lux = lightMeter.readLightLevel();
    if (lux < 0) {
        Serial.printf("Got invalud lux reading from light meter: %f", lux);
        return;
    }
    DataPoint datapoints[5];
    unsigned long t = timeClient.getEpochTime();
    datapoints[0] = createDataPoint(FLOAT, "lux", "bh1750", lux, t);
    datapoints[1] = createDataPoint(FLOAT, "PPFDSunlight","bh1750", lux*PPFD_CALIBRATION_SUNLIGHT, t);
    datapoints[2] = createDataPoint(FLOAT, "PPFDFlourescent","bh1750", lux*PPFD_CALIBRATION_FLOURESCENT, t);
    datapoints[3] = createDataPoint(FLOAT, "PPFDMetalHalide","bh1750", lux*PPFD_CALIBRATION_METAL_HALIDE, t);
    datapoints[4] = createDataPoint(FLOAT, "PPFDHighPressureSodium","bh1750", lux*PPFD_CALIBRATION_HPS, t);
    bulkOutputDataPoints(datapoints, 5, "bh1750", t);
#if CALIBRATE_SPECIFIC_LAMPS
    outputPoint(FLOAT, "PPFDDualEndedHighPressureSodium","bh1750", lux*PPFD_CALIBRATION_DEHPS);
    outputPoint(FLOAT, "PPFDCeramicMetalHalide4200k","bh1750", lux*PPFD_CALIBRATION_CMH942);
    outputPoint(FLOAT, "PPFDCeramicMetalHalide3100k","bh1750", lux*PPFD_CALIBRATION_CMH930);
#endif   
}
