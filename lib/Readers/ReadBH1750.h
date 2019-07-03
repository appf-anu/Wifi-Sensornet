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
    // sensor setup. BH1750 use hgh res mode rather than fast mode.
    BH1750 lightMeter = BH1750(address);
    lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE_2);

    time_t t = time(nullptr);
    float lux = lightMeter.readLightLevel();
    if (lux < 0) {
        Serial.printf("bad lux read: %f", lux);
        return;
    }
    DataSender<DataPoint> sender(3);
    DataPoint lux_ = createDataPoint(FLOAT, "lux", "bh1750", lux, t);
    sender.push_back(lux_);
    DataPoint PPFDSunlight = createDataPoint(FLOAT, "PPFDSunlight","bh1750", lux*PPFD_CALIBRATION_SUNLIGHT, t);
    sender.push_back(PPFDSunlight);
    DataPoint PPFDFlourescent = createDataPoint(FLOAT, "PPFDFlourescent","bh1750", lux*PPFD_CALIBRATION_FLOURESCENT, t);
    sender.push_back(PPFDFlourescent);
    DataPoint PPFDMetalHalide = createDataPoint(FLOAT, "PPFDMetalHalide","bh1750", lux*PPFD_CALIBRATION_METAL_HALIDE, t);
    sender.push_back(PPFDMetalHalide);
    DataPoint PPFDHighPressureSodium = createDataPoint(FLOAT, "PPFDHighPressureSodium","bh1750", lux*PPFD_CALIBRATION_HPS, t);
    sender.push_back(PPFDHighPressureSodium);
    
#if CALIBRATE_SPECIFIC_LAMPS
    DataPoint PPFDDualEndedHighPressureSodium = createDataPoint(FLOAT, "PPFDDualEndedHighPressureSodium","bh1750", lux*PPFD_CALIBRATION_DEHPS);
    sender.push_back(PPFDDualEndedHighPressureSodium);
    DataPoint PPFDCeramicMetalHalide4200k = createDataPoint(FLOAT, "PPFDCeramicMetalHalide4200k","bh1750", lux*PPFD_CALIBRATION_CMH942);
    sender.push_back(PPFDCeramicMetalHalide4200k);
    DataPoint PPFDCeramicMetalHalide3100k = createDataPoint(FLOAT, "PPFDCeramicMetalHalide3100k","bh1750", lux*PPFD_CALIBRATION_CMH930);
    sender.push_back(PPFDCeramicMetalHalide3100k);
#endif   
}
