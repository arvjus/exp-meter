# Darkroom Exposure Calculator

## Preamble

Anyone who prints black-and-white photographs in a darkroom is familiar with making test strips to determine the correct exposure and paper grade.

Before starting this project, I asked myself a simple question: **if I always use the same paper, developer, and filters, why repeat the same process for every print?**


## What is it for

This device is designed for black-and-white darkroom printing. It measures the density range of a photographic negative and automatically determines the optimal exposure time and multigrade contrast filter required to produce a print that uses the full tonal range of the photographic paper.

The process is similar to an automatic levels adjustment in digital imaging, but performed entirely in the darkroom.

The operator measures the **thinnest** and **densest** areas of the negative. From these measurements, the calculator determines both the printing exposure and the multigrade filter that best match the negative's contrast.


The calculator uses the measured values in two different ways:

* Exposure time is calculated from the thinnest part of the negative, ensuring that the print highlights are reproduced correctly.
* Contrast filter is selected from the difference between the thinnest and densest measurements (the negative's density range), matching the paper contrast to the negative.

The calculator always determines the contrast filter first, followed by the exposure time. This order is essential because each multigrade filter has a different filter factor, changing the amount of light reaching the paper. Once the appropriate filter has been selected, the correct exposure time is calculated for that filter.

The calibration is based on two reference exposure points for the selected paper:

* the exposure at which the lightest area of the print becomes pure white with no remaining detail;
* the exposure at which the darkest area of the print becomes pure black with no remaining detail.

Using these reference points, the calculator maps the negative to the full printable tonal range of the paper, producing a well-balanced print with little or no need for test strips. After the automatic calculation, both exposure and contrast can be fine-tuned by approximately **±2 EV** to achieve the desired artistic result.


## Hardware

* Adafruit TSL2561 Sensor
* Hitachi HD44780 LCD chipset
* ATmega328(P) MCU - most of Arduino boards will do the job


## Usage

### General Notes
Perform all measurements with the darkroom safelight switched off. The only light reaching the sensor should come from the enlarger.
Before measuring, focus the enlarger and set the aperture you intend to use for printing. Any changes to the aperture after the measurements will invalidate the calculated exposure.

### Density Mode

Density mode can be used to measure the optical density or filter factor of photographic materials, such as neutral-density filters.

Illuminate the sensor with a constant light source.
Press the measurement button to reset the reference level (0 EV / 0 optical density).
Insert the material to be measured between the light source and the sensor.
The display shows both the optical density and the corresponding EV difference.

### Exposure Mode

Exposure mode is used to determine the correct multigrade filter and printing exposure.

1. Press and hold the measurement button for approximately **2 seconds** to start a new measurement. This clears the previously stored minimum and maximum values.
2. Focus the enlarger and set the desired printing aperture.
3. Move the sensor over different areas of the projected negative. The display continuously shows the current EV value.
4. Press the measurement button to record a reading. Repeat this for several highlights and shadows. The calculator automatically keeps the **lowest** and **highest** measured EV values.
5. The calculator determines the optimal multigrade filter from the measured EV range and then calculates the required exposure time.

#### Optional Brightness and Contrast Compensation

After the measurements have been completed, the calculated result can be adjusted without repeating the measurements.

Use the potentiometer to select the desired compensation value (**−1, −0.5, 0, +0.5 or +1 EV**) and press the corresponding button:

* **Brightness compensation** adjusts the exposure time.
* **Contrast compensation** adjusts the selected multigrade filter.

The selected compensation remains active until a new measurement is started by resetting the stored minimum and maximum values.


## Calibration

The calculator must be calibrated separately for each paper type, as every photographic paper has a different exposure response and contrast curve.

The current firmware includes calibration data for:

* **Ilford Multigrade RC V**
* **Ilford Multigrade FB Classic**

Calibration is the most time-consuming part of the project, but it only needs to be performed once for each paper.

This project was calibrated using the **Ilford Multigrade filter set**. A pair of calibrated test strips is produced for each full-grade filter (0, 1, 2, 3, 4 and 5). Intermediate half-grade filters are calculated automatically by the calibration model.

For each filter, two reference exposure points are determined:

* the exposure at which highlight detail **first becomes visible**;
* the exposure at which shadow detail **completely disappears into solid black**.

Both exposure values are measured with the same device in **EV** units.

Because highlight exposures are typically very short, using a neutral-density (ND) filter is recommended to improve measurement accuracy. An **ND8** filter was used during the development of this project. Its actual filter factor (6.276× in my setup) increased a typical highlight exposure from about **2.4 s** to **15 s**, making precise measurements much easier.

The measurements are performed without any multigrade filter in the measuring device; the contrast filters are used only when producing the calibration test strips.

After all measurements have been collected, the results are stored in a CSV file and processed by the supplied Python script. The script fits a quadratic model to the measured data and generates the coefficients used by the firmware, including a ready-to-use C function for the Arduino implementation.

### Prerequisites
pip3 install numpy pandas sklearn matplotlib seaborn


## Known Issues During Calibration and How to Avoid Them

### Avoid measurements below 0.8 EV

Using an ND filter improves the accuracy of highlight measurements by increasing the exposure time. However, if the light reaching the sensor is below **0.8 EV**, the sensor becomes noticeably less accurate. This can introduce errors into the calibration and affect all subsequent exposure calculations.

If necessary, increase the enlarger brightness rather than measuring at very low light levels.

### Avoid long exposures when calibrating dark tones

Shadow calibration exposures should preferably remain below **45 seconds**. Longer exposures are increasingly affected by **reciprocity failure**, causing the measured response of the paper to deviate from the expected value.

If shadow exposures become too long, increase the enlarger illumination or open the lens aperture during calibration to keep the exposure time within a reliable range.



## Tips and Tricks, Final Thoughts

* **Reciprocity failure compensation** would be a useful future enhancement. At present, long exposure corrections must be applied manually when required.

* The biggest challenge in darkroom printing is not technical but artistic. Deciding where to place the tonal range of a print is ultimately a creative choice. This calculator cannot make those decisions for you—it simply provides a consistent and reliable starting point, leaving the final interpretation to the printer.


## Technical Notes

The calculator internally uses **Exposure Value (EV)** as its primary unit. Light intensity, EV, and optical density are related by the following equations:

```text
lux = 2.5 × 2^EV

EV = log2(lux / 2.5)

Optical Density = log10(lux / 2.5)
```

where:

* **lux** is the measured illuminance,
* **EV** is the exposure value relative to the device calibration,
* **Optical Density** is the base-10 logarithmic attenuation of light.

Using EV allows exposure differences to be expressed as simple additions and subtractions, while optical density provides a convenient way to characterize photographic filters and negatives.


## Gallery

The prints shown below are first prints made directly from the values calculated by the device, without using test strips or making any manual adjustments.

They are slightly flat, although the latest firmware revision includes improvements that should produce better tonal separation.


![samples](https://github.com/arvjus/exp-meter/blob/main/gallery/samples.jpeg)

Device construction:

![top](https://github.com/arvjus/exp-meter/blob/main/gallery/top.jpeg)
![front](https://github.com/arvjus/exp-meter/blob/main/gallery/front.jpeg)
![back](https://github.com/arvjus/exp-meter/blob/main/gallery/back.jpeg)
![inside](https://github.com/arvjus/exp-meter/blob/main/gallery/inside.jpeg)

Contrast / Filter function for Ilford Multigrade RC V papper:

![tfilter_rc5](https://github.com/arvjus/exp-meter/blob/main/gallery/filter_rc5.png)

Test strips, created during calibration process:

![test-strips](https://github.com/arvjus/exp-meter/blob/main/gallery/test-strips.jpeg)



